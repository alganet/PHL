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
/* Emit the declared #[...] attributes of a target as a summary list:
 * [ {name, line} ... ]. Argument values stay lazy — the PHP layer pulls
 * them through __reflect_attr_args when ReflectionAttribute needs them. */
static void ReflectMapAddAttrs(ph7_context *pCtx, ph7_value *pMap, SySet *pAttrs)
{
	ph7_value *pList = ph7_context_new_array(pCtx);
	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);
	sxu32 n;
	if( pList == 0 ){
		return;
	}
	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){
		ph7_value *pMeta = ph7_context_new_array(pCtx);
		if( pMeta == 0 ){ break; }
		ReflectMapAddStr(pCtx, pMeta, "name", SyStringData(&aA[n].sName), (int)SyStringLength(&aA[n].sName));
		ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)aA[n].nLine);
		ph7_array_add_elem(pList, 0, pMeta);
	}
	ph7_array_add_strkey_elem(pMap, "attrs", pList);
}
/* Emit a doc-comment field: the text when present, else boolean false
 * (getDocComment()'s exact return contract). */
static void ReflectMapAddDoc(ph7_context *pCtx, ph7_value *pMap, const SyString *pDoc)
{
	if( SyStringLength(pDoc) > 0 ){
		ReflectMapAddStr(pCtx, pMap, "doc", SyStringData(pDoc), (int)SyStringLength(pDoc));
	}else{
		ReflectMapAddBool(pCtx, pMap, "doc", 0);
	}
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
/*
 * The memo record. Its pClass field is the hash KEY: SyHashInsert borrows the
 * key bytes it is handed rather than duplicating them.
 */
typedef struct ReflectInfoMemo ReflectInfoMemo;
struct ReflectInfoMemo
{
	ph7_class *pClass;
	ph7_value sInfo;
};
/*
 * array|null __phl_rcinfo(object|string $target)
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
 *
 * Memoized per class (pVm->hClassInfo). Building one walks the whole
 * inheritance chain, and the still-PHP Reflection classes ask for the SAME
 * class's descriptor once per member they construct — without the memo a
 * getMethods() over a ten-method class builds it eleven times. This was a
 * `static $c` array inside the prelude function; it moved into the VM when the
 * function became C. Only SUCCESSFUL lookups are remembered, so a class that
 * has not been autoloaded yet is re-queried.
 */
static int vm_builtin_phl_rcinfo(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass;
	ph7_value *pInfo, *pConsts, *pProps, *pMethods, *pList;
	SyHashEntry *pMemo;
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
	pMemo = SyHashGet(&pVm->hClassInfo, (const void *)&pClass, sizeof(ph7_class *));
	if( pMemo ){
		ph7_result_value(pCtx, &((ReflectInfoMemo *)pMemo->pUserData)->sInfo);
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
	ReflectMapAddBool(pCtx, pInfo, "enum", (pClass->iFlags & PH7_CLASS_ENUM) != 0);
	if( pClass->nEnumBacking == MEMOBJ_INT ){
		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "int", (int)sizeof("int")-1);
	}else if( pClass->nEnumBacking == MEMOBJ_STRING ){
		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "string", (int)sizeof("string")-1);
	}else{
		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "", 0);
	}
	{
		/* Enum case names in declaration order (empty list for non-enums) */
		ph7_value *pCases = ph7_context_new_array(pCtx);
		if( pCases ){
			ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);
			sxu32 nCase;
			for( nCase = 0 ; nCase < SySetUsed(&pClass->aEnumCases) ; nCase++ ){
				ph7_value *pNm = ph7_context_new_scalar(pCtx);
				if( pNm ){
					ph7_value_string(pNm,apCase[nCase]->sName.zString,(int)apCase[nCase]->sName.nByte);
					ph7_array_add_elem(pCases,0,pNm);
				}
			}
			ph7_array_add_strkey_elem(pInfo,"cases",pCases);
		}
	}
	if( pClass->pBase ){
		ReflectMapAddStr(pCtx, pInfo, "parent", SyStringData(&pClass->pBase->sName),
			(int)SyStringLength(&pClass->pBase->sName));
	}else{
		ReflectMapAddNull(pCtx, pInfo, "parent");
	}
	/* Transitive interfaces */
	SySetInit(&aIfaceSet, &pVm->sAllocator, sizeof(ph7_class *));
	ReflectInterfacesOf(pClass, &aIfaceSet);
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
	ReflectMapAddDoc(pCtx, pInfo, &pClass->sDoc);
	ReflectMapAddAttrs(pCtx, pInfo, &pClass->aAttrs);
	/* Members, in PHP's reporting order — the shared walk (ReflectMembers), so
	 * this descriptor and the native ReflectionClass accessors can never drift
	 * apart on which members exist or what order they come in. */
	{
		SySet aMembers;
		sxu32 nM;
		SySetInit(&aMembers, &pVm->sAllocator, sizeof(ReflectMember));
		ReflectMembers(pVm, pClass, &aMembers, 1);
		for( nM = 0 ; nM < SySetUsed(&aMembers) ; nM++ ){
			ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, nM);
			ph7_value *pMeta = ph7_context_new_array(pCtx);
			if( pMeta == 0 ){ break; }
			if( pM->iKind == REFLECT_MEMBER_METHOD ){
				ph7_class_method *pMeth = pM->pMeth;
				/* A __construct key whose method has a DIFFERENT own name is a trait
				 * `use T { m as __construct; }` alias. php lists such a method under
				 * BOTH names (its own and __construct) and getConstructor() resolves
				 * the __construct one, so the entry is emitted under this key too
				 * rather than skipped. (The legacy PHP-4 class-name-constructor mount
				 * alias that also produced a __construct key is gone, removed in 8.0.) */
				ReflectMapAddInt(pCtx, pMeta, "vis", (sxi64)pMeth->iProtection);
				ReflectMapAddBool(pCtx, pMeta, "static", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);
				ReflectMapAddBool(pCtx, pMeta, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);
				ReflectMapAddBool(pCtx, pMeta, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);
				ReflectMapAddStr(pCtx, pMeta, "decl", SyStringData(&pM->pDecl->sName),
					(int)SyStringLength(&pM->pDecl->sName));
				ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)pMeth->nLine);
				ReflectMapAddDyn(pCtx, pMethods, &pM->sKey, pMeta);
				continue;
			}
			{
				ph7_class_attr *pAttr = pM->pAttr;
				ReflectMapAddInt(pCtx, pMeta, "vis", (sxi64)pAttr->iProtection);
				ReflectMapAddStr(pCtx, pMeta, "decl", SyStringData(&pM->pDecl->sName),
					(int)SyStringLength(&pM->pDecl->sName));
				ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)pAttr->nLine);
				ReflectMapAddDoc(pCtx, pMeta, &pAttr->sDoc);
				ReflectMapAddAttrs(pCtx, pMeta, &pAttr->aAttrs);
				ReflectMapAddBool(pCtx, pMeta, "typed", (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0);
				if( SyStringLength(&pAttr->sTypeName) > 0 ){
					ReflectMapAddStr(pCtx, pMeta, "typetext", SyStringData(&pAttr->sTypeName),
						(int)SyStringLength(&pAttr->sTypeName));
				}else{
					ReflectMapAddNull(pCtx, pMeta, "typetext");
				}
				if( pM->iKind == REFLECT_MEMBER_CONST ){
					ReflectMapAddBool(pCtx, pMeta, "final", (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) != 0);
					ReflectMapAddBool(pCtx, pMeta, "enumcase", (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0);
					ReflectMapAddDyn(pCtx, pConsts, &pM->sKey, pMeta);
				}else{
					ReflectMapAddBool(pCtx, pMeta, "static", (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0);
					ReflectMapAddBool(pCtx, pMeta, "readonly", (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0);
					ReflectMapAddBool(pCtx, pMeta, "privset", (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) != 0);
					ReflectMapAddBool(pCtx, pMeta, "protset", (pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET) != 0);
					ReflectMapAddBool(pCtx, pMeta, "hookget", (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) != 0);
					ReflectMapAddBool(pCtx, pMeta, "hookset", (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET) != 0);
					ReflectMapAddBool(pCtx, pMeta, "virtual", (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) != 0);
					ReflectMapAddBool(pCtx, pMeta, "hasdef", SySetUsed(&pAttr->aByteCode) > 0);
					ReflectMapAddDyn(pCtx, pProps, &pM->sKey, pMeta);
				}
			}
		}
		SySetRelease(&aMembers);
	}
	/* From the LOOKUP, not the listing: an inherited private __construct is not
	 * reported as a member but still decides instantiability. */
	ReflectCtorCloneVis(pVm, pClass, &iCtorVis, &iCloneVis);
	ReflectMapAddInt(pCtx, pInfo, "ctorvis", (sxi64)iCtorVis);
	ReflectMapAddInt(pCtx, pInfo, "clonevis", (sxi64)iCloneVis);
	ph7_array_add_strkey_elem(pInfo, "consts", pConsts);
	ph7_array_add_strkey_elem(pInfo, "props", pProps);
	ph7_array_add_strkey_elem(pInfo, "methods", pMethods);
	{
		/* Remember it. The memo slot SHARES the descriptor's hashmap through
		 * its reference count — which is exactly what the PHP `$c[$k] = $info`
		 * did — so the context value below can still be released normally. It
		 * lives for the VM's lifetime, like the class it describes. */
		ReflectInfoMemo *pKeep;
		pKeep = (ReflectInfoMemo *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(ReflectInfoMemo));
		if( pKeep ){
			pKeep->pClass = pClass;
			PH7_MemObjInit(pVm, &pKeep->sInfo);
			PH7_MemObjStore(pInfo, &pKeep->sInfo);
			/* The key bytes are BORROWED by SyHashInsert, never copied, so they
			 * have to be the record's own field rather than a local. */
			if( SyHashInsert(&pVm->hClassInfo, (const void *)&pKeep->pClass,
				sizeof(ph7_class *), pKeep) != SXRET_OK ){
				PH7_MemObjRelease(&pKeep->sInfo);
				SyMemBackendFree(&pVm->sAllocator, pKeep);
			}
		}
	}
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
	 || (pAttr = ReflectFetchConst(pClass, apArg[1])) == 0
	 || (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Constant slots are evaluated lazily on first access */
	if( PH7_VmMaterializeClassConst(pCtx->pVm,pClass,pAttr) != SXRET_OK ){
		/* Initializer raised: the throw is in flight; report null here */
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
	if( VmClassStaticDeferPending(pClass) ){
		/* Reading a static through reflection materializes the class's static
		 * table exactly as `C::$s` does, so a default that threw at the
		 * declaration raises HERE (php: getStaticPropertyValue() /
		 * getStaticProperties() / ReflectionProperty::getValue() all do). */
		sxi32 rcMat = PH7_VmMaterializeClassStatics(pCtx->pVm, pClass);
		if( rcMat != SXRET_OK ){
			return rcMat;
		}
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
	if( VmClassStaticDeferPending(pClass) ){
		/* A WRITE materializes the table too (php's setStaticPropertyValue()
		 * raises on a broken default before storing anything). */
		sxi32 rcMat = PH7_VmMaterializeClassStatics(pCtx->pVm, pClass);
		if( rcMat != SXRET_OK ){
			return rcMat;
		}
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
		if( nArg > 1 ){
			ReflectCollectArgs(pCtx, apArg[1], &aArg, &aNames);
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
	if( VmClassStaticDeferPending(pClass) ){
		/* Same materialization the engine's own access sites run (see
		 * PH7_VmMaterializeClassStatics). */
		sxi32 rcMat = PH7_VmMaterializeClassStatics(pCtx->pVm, pClass);
		if( rcMat != SXRET_OK ){
			return rcMat;
		}
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
			SyHashEntry *pSlot;
			if( VmClassStaticDeferPending(pClass) ){
				/* isInitialized() reads the slot state, so it materializes the
				 * table too (php raises the default's error before answering). */
				sxi32 rcMat = PH7_VmMaterializeClassStatics(pCtx->pVm, pClass);
				if( rcMat != SXRET_OK ){
					return rcMat;
				}
			}
			pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));
			iState |= 1 | 2;
			if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){
				iState &= ~2;
			}
		}
	}
	ph7_result_int(pCtx, iState);
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
	ReflectMapAddBool(pCtx, pInfo, "fstatic", (pFunc->iFlags & VM_FUNC_STATIC_CL) != 0);
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
	ReflectMapAddDoc(pCtx, pInfo, &pFunc->sDoc);
	ReflectMapAddAttrs(pCtx, pInfo, &pFunc->aAttrs);
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
		ReflectMapAddAttrs(pCtx, pMeta, &aArg[n].aAttrs);
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
/* One `type &$name = default` part into the param-meta shape. */
static void ReflectSigParam(ph7_context *pCtx, ph7_value *pParams,
	const char *z, int n, int iPos, int *pbVariadic)
{
	ph7_value *pMeta = ph7_context_new_array(pCtx);
	const char *zDef = 0;
	const char *zName;
	int nDef = 0, nName;
	int iEq, iDollar, iSpace, bVariadic, bTyped = 0, bOptional = 0;
	if( pMeta == 0 ){
		return;
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
	if( zDef && nDef == 1 && zDef[0] == '?' ){
		/* `= ?` is the table's OPTIONAL-but-no-default marker, php's own shape
		 * for a parameter like ReflectionClass::getStaticPropertyValue()'s
		 * $default: isOptional() true, isDefaultValueAvailable() FALSE, so
		 * getDefaultValue() raises. Reporting it as a default (which is what
		 * a bare `hasdef` did) made that call answer NULL instead. */
		bOptional = 1;
		zDef = 0;
		nDef = 0;
	}
	bVariadic = ReflectSigHas(z,n,"...",3);
	if( bVariadic ){
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
	zName = iDollar < 0 ? z : &z[iDollar+1];
	nName = iDollar < 0 ? n : n - iDollar - 1;
	ReflectMapAddStr(pCtx,pMeta,"name",zName,nName);
	ReflectMapAddInt(pCtx,pMeta,"pos",(sxi64)iPos);
	ReflectMapAddBool(pCtx,pMeta,"byref",ReflectSigHas(z,n,"&",1));
	ReflectMapAddBool(pCtx,pMeta,"variadic",bVariadic);
	ReflectMapAddBool(pCtx,pMeta,"hasdef",zDef != 0);
	ReflectMapAddBool(pCtx,pMeta,"optional",bOptional || bVariadic || zDef != 0);
	if( iSpace >= 0 && iDollar >= 0 && iSpace < iDollar ){
		/* The type is whatever precedes the first space, so `?DOMNode $child`
		 * types as `?DOMNode` and an untyped `$x` types as nothing. */
		bTyped = 1;
		ReflectMapAddBool(pCtx,pMeta,"nullable",
			z[0] == '?' || ReflectSigHasNoCase(z,iSpace,"null",4));
		ReflectMapAddStr(pCtx,pMeta,"typetext",z,iSpace);
	}else{
		ReflectMapAddBool(pCtx,pMeta,"nullable",0);
		ReflectMapAddNull(pCtx,pMeta,"typetext");
	}
	SXUNUSED(bTyped);
	ReflectMapAddBool(pCtx,pMeta,"promoted",0);
	{
		ph7_value *pEmpty = ph7_context_new_array(pCtx);
		if( pEmpty ){
			ph7_array_add_strkey_elem(pMeta,"attrs",pEmpty);
		}
	}
	if( zDef ){
		ph7_value *pVal = ph7_context_new_scalar(pCtx);
		ReflectMapAddStr(pCtx,pMeta,"deftext",zDef,nDef);
		/* The VALUE too, so getDefaultValue() reads the meta instead of
		 * re-parsing the text through a second prelude helper. */
		if( pVal && ReflectSigScalar(pCtx,zDef,nDef,pVal) ){
			ReflectMapAddBool(pCtx,pMeta,"defscalar",1);
			ph7_array_add_strkey_elem(pMeta,"defval",pVal);
		}else{
			ReflectMapAddBool(pCtx,pMeta,"defscalar",0);
			ReflectMapAddNull(pCtx,pMeta,"defval");
		}
	}else{
		ReflectMapAddNull(pCtx,pMeta,"deftext");
		ReflectMapAddBool(pCtx,pMeta,"defscalar",0);
		ReflectMapAddNull(pCtx,pMeta,"defval");
	}
	if( bVariadic ){
		*pbVariadic = 1;
	}
	ph7_array_add_elem(pParams,0,pMeta);
}
/*
 * Rewrite a descriptor's params/variadic/minarg from its declared signature.
 * A no-op for a descriptor without one (a compiled function already carries the
 * real thing), so every exit of __reflect_func_info can run it unconditionally.
 */
static void ReflectSigFixup(ph7_context *pCtx, ph7_value *pInfo)
{
	ph7_value *pSig, *pRet2, *pParams;
	const char *zSig, *zPart;
	int nSig, nPart, iPos = 0, bVariadic = 0;
	/* A native method's declared RETURN type lives in its own slot, because the
	 * compiled function it hangs off has none. */
	pRet2 = ph7_array_fetch(pInfo,"ret2",-1);
	if( pRet2 && (pRet2->iFlags & MEMOBJ_STRING) ){
		ph7_array_add_strkey_elem(pInfo,"rettext",pRet2);
	}
	pSig = ph7_array_fetch(pInfo,"sig",-1);
	if( pSig == 0 || (pSig->iFlags & MEMOBJ_STRING) == 0 ){
		return;
	}
	zSig = (const char *)SyBlobData(&pSig->sBlob);
	nSig = (int)SyBlobLength(&pSig->sBlob);
	if( nSig < 1 ){
		return;
	}
	pParams = ph7_context_new_array(pCtx);
	if( pParams == 0 ){
		return;
	}
	/* Split on the top-level commas; a quoted default may hold its own. */
	while( nSig > 0 ){
		int iComma = ReflectSigFindUnquoted(zSig,nSig,',');
		zPart = zSig;
		nPart = iComma < 0 ? nSig : iComma;
		ReflectSigTrim(&zPart,&nPart);
		if( nPart > 0 ){
			ReflectSigParam(pCtx,pParams,zPart,nPart,iPos,&bVariadic);
			iPos++;
		}
		if( iComma < 0 ){
			break;
		}
		zSig += iComma + 1;
		nSig -= iComma + 1;
	}
	ph7_array_add_strkey_elem(pInfo,"params",pParams);
	ReflectMapAddInt(pCtx,pInfo,"minarg",-1);
	ReflectMapAddBool(pCtx,pInfo,"variadic",bVariadic);
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
		ReflectMapAddBool(pCtx, pInfo, "fstatic", 0);
		ReflectMapAddBool(pCtx, pInfo, "byref", 0);
		ReflectMapAddBool(pCtx, pInfo, "generator", 0);
		ReflectMapAddBool(pCtx, pInfo, "strict", 0);
		ReflectMapAddBool(pCtx, pInfo, "file", 0);
		ReflectMapAddInt(pCtx, pInfo, "line", 0);
		ReflectMapAddInt(pCtx, pInfo, "endline", 0);
		ReflectMapAddBool(pCtx, pInfo, "doc", 0);
		{
			ph7_value *pEmpty = ph7_context_new_array(pCtx);
			if( pEmpty ){
				ph7_array_add_strkey_elem(pInfo, "attrs", pEmpty);
			}
		}
		if( pHost->zRet ){
			ReflectMapAddStr(pCtx, pInfo, "rettext", pHost->zRet, (int)SyStrlen(pHost->zRet));
		}else{
			ReflectMapAddNull(pCtx, pInfo, "rettext");
		}
		ReflectMapAddBool(pCtx, pInfo, "retnullable", 0);
		if( pParams ){
			ph7_array_add_strkey_elem(pInfo, "params", pParams);
		}
		ReflectMapAddBool(pCtx, pInfo, "variadic", 0);
		ReflectMapAddInt(pCtx, pInfo, "minarg", (sxi64)pHost->nMinArg);
		if( pHost->zSig ){
			ReflectMapAddStr(pCtx, pInfo, "sig", pHost->zSig, (int)SyStrlen(pHost->zSig));
		}else{
			ReflectMapAddStr(pCtx, pInfo, "sig", "", 0);
		}
		ReflectSigFixup(pCtx, pInfo);
		ph7_result_value(pCtx, pInfo);
		return PH7_OK;
	}
	ReflectFillFuncCommon(pCtx, pInfo, pFunc);
	ReflectMapAddInt(pCtx, pInfo, "minarg", -1);
	if( (pFunc->iFlags & VM_FUNC_NATIVE) && pFunc->pNative ){
		/* A C-bodied METHOD declares its parameters in the one place a native class
		 * can: the spec's signature string, which already drives arity and the
		 * by-ref mask. Without this it reached Reflection as a bytecode function
		 * with an EMPTY parameter set, so every native method reported NO
		 * parameters -- the method-side twin of the aBuiltinSig[] debt a converted
		 * FUNCTION owes, and Fiber/Generator/XMLWriter/Closure had it too. */
		if( pFunc->pNative->zSig ){
			ReflectMapAddStr(pCtx, pInfo, "sig", pFunc->pNative->zSig,
				(int)SyStrlen(pFunc->pNative->zSig));
		}
		if( pFunc->pNative->zRet && SyStringLength(&pFunc->sReturnTypeName) == 0 ){
			ReflectMapAddStr(pCtx, pInfo, "ret2", pFunc->pNative->zRet,
				(int)SyStrlen(pFunc->pNative->zRet));
		}
	}else if( (pFunc->iFlags & VM_FUNC_INTERNAL) && SySetUsed(&pFunc->aArgs) == 0 && pMeth == 0 ){
		/* Embedded-PHP builtin (max/min...): declared argless, actual
		 * signature comes from the static table */
		const char *zRet = 0;
		const char *zSig = PH7_VmBuiltinSigLookup(SyStringData(&pFunc->sName), SyStringLength(&pFunc->sName), &zRet);
		if( zSig ){
			ReflectMapAddStr(pCtx, pInfo, "sig", zSig, (int)SyStrlen(zSig));
		}
		if( zRet && SyStringLength(&pFunc->sReturnTypeName) == 0 ){
			ReflectMapAddStr(pCtx, pInfo, "ret2", zRet, (int)SyStrlen(zRet));
		}
	}
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
				if( (aEnv[n].iFlags & VM_FUNC_ARG_BY_REF) && aEnv[n].nIdx != SXU32_HIGH ){
					/* Captured by reference: report the slot's live value */
					ph7_value *pLive = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aEnv[n].nIdx);
					ReflectMapAddDyn(pCtx, pUsed, &aEnv[n].sName, pLive ? pLive : &aEnv[n].sValue);
					continue;
				}
				ReflectMapAddDyn(pCtx, pUsed, &aEnv[n].sName, &aEnv[n].sValue);
			}
			ph7_array_add_strkey_elem(pInfo, "used", pUsed);
		}
	}
	ReflectSigFixup(pCtx, pInfo);
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
	ReflectCollectArgs(pCtx, apArg[3], &aCallArg, 0);
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
 * array|null __reflect_attr_args(string $kind, mixed $target, ?string $member,
 *                                int $paramIdx, int $attrIdx)
 * Evaluate the recorded argument expressions of one declared attribute:
 * kind 'class' (target = class), 'attr' (class + property/constant name),
 * 'method' (class + method), 'fn' (function name or Closure), 'param'
 * (function spec + parameter index). Named arguments become string keys.
 */
static int vm_builtin_reflect_attr_args(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SySet *pAttrs = 0;
	ph7_class *pDeclCls = 0; /* class an attribute is declared on: scope for self:: in its args */
	ph7_attribute *pAttrRec;
	ph7_value *pOut;
	const char *zKind;
	int nKind;
	sxu32 nAttrIdx, n;
	if( nArg < 5 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	zKind = ph7_value_to_string(apArg[0], &nKind);
	nAttrIdx = (sxu32)ph7_value_to_int(apArg[4]);
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
		ph7_result_null(pCtx);
		return PH7_OK;
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
	ph7_result_value(pCtx, pOut);
	return PH7_OK;
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
 * ?ReflectionType __reflect_make_type(?string $text)
 *
 * Still a global: the reflection classes that ANSWER a type (getType,
 * getReturnType, ReflectionEnum::getBackingType) are prelude PHP for now, and
 * this is their factory. It retires with them.
 */
static int vm_builtin_reflect_make_type(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	int nText = 0;
	const char *zText;
	ph7_class_instance *pType;
	if( nArg < 1 || ph7_value_is_null(apArg[0]) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	zText = ph7_value_to_string(apArg[0], &nText);
	pType = ReflectMakeType(pCtx, zText, nText);
	if( pType == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_NativeResultObject(pCtx, pType);
	return PH7_OK;
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
		{ "ReflectionType", 0, "Stringable", PH7_CLASS_ABSTRACT|PH7_CLASS_NOCLONE,
		  aBaseMethod, SX_ARRAYSIZE(aBaseMethod), 0, 0, aBaseProp, SX_ARRAYSIZE(aBaseProp), 0, 0 },
		{ "ReflectionNamedType", "ReflectionType", 0, PH7_CLASS_NOCLONE,
		  aNamedMethod, SX_ARRAYSIZE(aNamedMethod), 0, 0, aNamedProp, SX_ARRAYSIZE(aNamedProp), 0, 0 },
		{ "ReflectionUnionType", "ReflectionType", 0, PH7_CLASS_NOCLONE,
		  aCompMethod, SX_ARRAYSIZE(aCompMethod), 0, 0, aCompProp, SX_ARRAYSIZE(aCompProp), 0, 0 },
		{ "ReflectionIntersectionType", "ReflectionType", 0, PH7_CLASS_NOCLONE,
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
 * The shared getAttributes() body.
 *
 * Hands the target's #[...] SUMMARY list and the [kind, target, member,
 * paramIdx] spec to the prelude builder (__reflect_build_attrs, chunk 7),
 * which turns them into ReflectionAttribute objects and applies the
 * name / IS_INSTANCEOF filter. Argument VALUES stay lazy — the spec is what
 * reopens them later through __reflect_attr_args — so every reflector that
 * declares getAttributes() only has to say WHICH target it is.
 */
static int ReflectBuildAttrs(ph7_context *pCtx, SySet *pAttrs, const char *zKind,
	const char *zTarget, int nTarget, const char *zMember, int nMember,
	int iParamIdx, int iTargetBit, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value sMeta, sSpec, sTarget, sName, sFlags, sRes;
	ph7_value *apCall[5];
	sxi32 rc;
	PH7_MemObjInit(pVm, &sMeta);
	PH7_MemObjInit(pVm, &sSpec);
	PH7_MemObjInit(pVm, &sTarget);
	PH7_MemObjInit(pVm, &sName);
	PH7_MemObjInit(pVm, &sFlags);
	PH7_MemObjInit(pVm, &sRes);
	{
		ph7_value *pMeta = ph7_context_new_array(pCtx);
		ph7_value *pSpec = ph7_context_new_array(pCtx);
		ph7_value *pKind = ph7_context_new_scalar(pCtx);
		ph7_value *pTgt  = ph7_context_new_scalar(pCtx);
		ph7_value *pMem  = ph7_context_new_scalar(pCtx);
		ph7_value *pIdx  = ph7_context_new_scalar(pCtx);
		if( pMeta == 0 || pSpec == 0 || pKind == 0 || pTgt == 0 || pMem == 0 || pIdx == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		ReflectMapAddAttrs(pCtx, pMeta, pAttrs);
		{
			ph7_value *pList = ph7_array_fetch(pMeta, "attrs", -1);
			if( pList ){
				PH7_MemObjStore(pList, &sMeta);
			}
		}
		ph7_value_string(pKind, zKind, -1);
		ph7_value_string(pTgt, zTarget, nTarget);
		if( zMember ){
			ph7_value_string(pMem, zMember, nMember);
		}else{
			ph7_value_null(pMem);
		}
		ph7_value_int(pIdx, iParamIdx);
		ph7_array_add_elem(pSpec, 0, pKind);
		ph7_array_add_elem(pSpec, 0, pTgt);
		ph7_array_add_elem(pSpec, 0, pMem);
		ph7_array_add_elem(pSpec, 0, pIdx);
		PH7_MemObjStore(pSpec, &sSpec);
	}
	ph7_value_int(&sTarget, iTargetBit);
	if( nArg > 0 ){
		PH7_MemObjStore(apArg[0], &sName);
	}else{
		ph7_value_null(&sName);
	}
	ph7_value_int(&sFlags, nArg > 1 ? ph7_value_to_int(apArg[1]) : 0);
	apCall[0] = &sMeta;
	apCall[1] = &sSpec;
	apCall[2] = &sTarget;
	apCall[3] = &sName;
	apCall[4] = &sFlags;
	{
		ph7_value sFn;
		SyString sStr;
		PH7_MemObjInit(pVm, &sFn);
		SyStringInitFromBuf(&sStr, "__reflect_build_attrs", sizeof("__reflect_build_attrs")-1);
		PH7_MemObjInitFromString(pVm, &sFn, &sStr);
		rc = PH7_VmCallUserFunction(pVm, &sFn, 5, apCall, &sRes);
		PH7_MemObjRelease(&sFn);
	}
	if( rc == SXRET_OK ){
		ph7_result_value(pCtx, &sRes);
	}
	PH7_MemObjRelease(&sMeta);
	PH7_MemObjRelease(&sSpec);
	PH7_MemObjRelease(&sTarget);
	PH7_MemObjRelease(&sName);
	PH7_MemObjRelease(&sFlags);
	PH7_MemObjRelease(&sRes);
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
	/* 64 = Attribute::TARGET_CONSTANT */
	return ReflectBuildAttrs(pCtx, &pCons->aAttrs, "const", zName, nName, 0, 0, 0, 64,
		nArg, apArg);
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
		{ "ReflectionGenerator", 0, 0, PH7_CLASS_FINAL,
		  aGenMethod, SX_ARRAYSIZE(aGenMethod), 0, 0, aGenProp, SX_ARRAYSIZE(aGenProp), 0, 0 },
		{ "ReflectionFiber", 0, 0, PH7_CLASS_FINAL,
		  aFiberMethod, SX_ARRAYSIZE(aFiberMethod), 0, 0, aFiberProp, SX_ARRAYSIZE(aFiberProp), 0, 0 },
		{ "ReflectionConstant", 0, "Reflector", 0,
		  aConstMethod, SX_ARRAYSIZE(aConstMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0 },
		{ "ReflectionExtension", 0, "Reflector", 0,
		  aExtMethod, SX_ARRAYSIZE(aExtMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0 },
		{ "ReflectionZendExtension", 0, "Reflector", 0,
		  aZendMethod, SX_ARRAYSIZE(aZendMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0 },
		{ "ReflectionReference", 0, 0, PH7_CLASS_FINAL,
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
	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){ iMods |= 128; }
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
	/* 1 = Attribute::TARGET_CLASS */
	return ReflectBuildAttrs(pCtx, &pClass->aAttrs, "class", zName, nName, 0, 0, 0, 1,
		nArg, apArg);
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
static int vm_builtin_ReflectionClass_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_value sName;
	ph7_value *apCtor[1];
	ph7_class_instance *pExt;
	sxi32 rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !ReflectClassFlag(pCtx, PH7_CLASS_INTERNAL) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
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
/* __toString(): php's export format, still chunk 9. */
static int vm_builtin_ReflectionClass_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value sSelf, sRes, sFn;
	ph7_value *apCall[1];
	SyString sStr;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 ){
		ph7_result_string(pCtx, "", 0);
		return PH7_OK;
	}
	PH7_MemObjInit(pVm, &sSelf);
	PH7_MemObjInit(pVm, &sRes);
	PH7_MemObjInit(pVm, &sFn);
	sSelf.x.pOther = pThis;
	sSelf.iFlags = MEMOBJ_OBJ;
	SyStringInitFromBuf(&sStr, "__reflect_export_class", sizeof("__reflect_export_class")-1);
	PH7_MemObjInitFromString(pVm, &sFn, &sStr);
	apCall[0] = &sSelf;
	if( PH7_VmCallUserFunction(pVm, &sFn, 1, apCall, &sRes) == SXRET_OK ){
		ph7_result_value(pCtx, &sRes);
	}
	PH7_MemObjRelease(&sFn);
	PH7_MemObjRelease(&sRes);
	/* sSelf borrows the receiver and never took a reference: not released. */
	return PH7_OK;
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
/* Reflection::getModifierNames(int $modifiers) */
static int vm_builtin_Reflection_getModifierNames(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	static const struct { sxi64 iBit; const char *zName; } aMod[] = {
		{ 64,  "abstract" },
		{ 32,  "final" },
		{ 1,   "public" },
		{ 2,   "protected" },
		{ 4,   "private" },
		{ 16,  "static" },
		{ 128, "readonly" },
	};
	ph7_value *pOut = ph7_context_new_array(pCtx);
	sxi64 iMods = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;
	sxu32 n;
	if( pOut == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	for( n = 0 ; n < SX_ARRAYSIZE(aMod) ; n++ ){
		ph7_value *pName;
		if( (iMods & aMod[n].iBit) == 0 ){
			continue;
		}
		pName = ph7_context_new_scalar(pCtx);
		if( pName == 0 ){ break; }
		ph7_value_string(pName, aMod[n].zName, -1);
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
PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm)
{
	static const struct {
		const char *zName;
		ProchHostFunction xFunc;
	} aFunc[] = {
		{ "__phl_rcinfo",             vm_builtin_phl_rcinfo },
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
		{ "__reflect_attr_args",      vm_builtin_reflect_attr_args },
		{ "__reflect_make_type",      vm_builtin_reflect_make_type },
	};
	sxu32 n;
	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){
		ph7_create_function(&(*pVm), aFunc[n].zName, aFunc[n].xFunc, 0);
	}
	return PH7_VmInstallReflectionLib(&(*pVm));
}
