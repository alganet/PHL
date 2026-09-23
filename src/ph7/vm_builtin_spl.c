/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
 * SPL iterators, slice 1 (NEWPLAN band D): SeekableIterator, ArrayIterator,
 * ArrayObject. (natsort()/natcasesort() used to be declared here as prelude
 * wrappers over uasort(...,'strnatcmp'); they are C builtins in hashmap_sort.c
 * now -- see ph7_hashmap_natsort -- and the methods below delegate to them.)
 * Embedded-PHP chunk following the Reflection architecture — installed
 * inside the bCompilingBuiltin window, backed by the engine's native array
 * internal-pointer builtins (reset/next/key/current keep their position on a
 * property, so ArrayIterator's cursor IS the backing array's pointer).
 */

/*
 * ---------------------------------------------------------------------------
 * The weak-reference family: WeakReference and WeakMap, declared and bodied in C.
 *
 * They used to be PHP classes over three global thunks (`__weak_create()`,
 * `__weak_get()`, `__weak_drop()`) that traded the cell pointer back and forth as
 * an opaque int. The cell never belonged in PHP: it is a C lifetime, and the moment
 * a class can have C METHOD bodies the thunks are just the methods, spelled with
 * the pointer left in the open. They are gone; `__h` is the only slot left, and it
 * is private to a final, uncloneable class.
 * ---------------------------------------------------------------------------
 */
/* The shared cell for a target, created on first use. Takes ONE handle. */
static VmWeakCell * WkCellFor(ph7_vm *pVm,ph7_class_instance *pObj)
{
	SyHashEntry *pEntry = SyHashGet(&pVm->hWeakCell,(const void *)&pObj,sizeof(void *));
	VmWeakCell *pCell;
	if( pEntry ){
		pCell = (VmWeakCell *)pEntry->pUserData;
		pCell->nRef++;
		return pCell;
	}
	pCell = (VmWeakCell *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmWeakCell));
	if( pCell == 0 ){
		return 0;
	}
	pCell->pObj = pObj;
	pCell->pRef = 0;
	pCell->nRef = 1;
	/* SyHash stores the key POINTER (no copy): key off the cell's own pObj field —
	 * heap-stable for the entry's whole lifetime, and it holds the live pointer
	 * bytes until the release hook nulls it (which happens only after the entry
	 * is deleted). */
	if( SyHashInsert(&pVm->hWeakCell,(const void *)&pCell->pObj,sizeof(void *),pCell) != SXRET_OK ){
		SyMemBackendFree(&pVm->sAllocator,pCell);
		return 0;
	}
	return pCell;
}
/* Give one handle back; the last one frees the cell. */
static void WkCellDrop(ph7_vm *pVm,VmWeakCell *pCell)
{
	if( pCell == 0 || pCell->nRef == 0 ){
		return;
	}
	pCell->nRef--;
	if( pCell->nRef == 0 ){
		if( pCell->pObj ){
			/* Target still alive: unhook the registry entry before freeing. */
			void *pDummy = 0;
			SyHashDeleteEntry(&pVm->hWeakCell,(const void *)&pCell->pObj,sizeof(void *),&pDummy);
		}
		SyMemBackendFree(&pVm->sAllocator,pCell);
	}
}
/* The cell a WeakReference instance holds, or NULL. */
static VmWeakCell * WkCellOf(ph7_class_instance *pRef)
{
	return (VmWeakCell *)(sxuptr)(sxu64)PH7_NativeAttrInt(pRef,"__h");
}
/* Hand an instance back without owning a reference of our own (ph7_result_value's
 * MemObjStore takes the one the result needs). */
static void SplResultBorrowed(ph7_context *pCtx,ph7_class_instance *pObj)
{
	ph7_value sObj;
	PH7_MemObjInit(pCtx->pVm,&sObj);
	sObj.x.pOther = pObj;
	MemObjSetType(&sObj,MEMOBJ_OBJ);
	ph7_result_value(pCtx,&sObj);
}
/*
 * The ONE WeakReference published for a target, created on first ask.
 *
 * php answers the same object for the same target every time, so the cell caches
 * what it handed out. `*pbOwned` says whether the caller holds the fresh instance's
 * reference (and so must give it up once it has been stored somewhere) or is merely
 * borrowing the published one.
 */
static ph7_class_instance * WkRefFor(ph7_vm *pVm,ph7_class_instance *pObj,int *pbOwned)
{
	VmWeakCell *pCell = WkCellFor(pVm,pObj);
	ph7_class *pClass;
	ph7_class_instance *pRef;
	*pbOwned = 0;
	if( pCell == 0 ){
		return 0;
	}
	if( pCell->pRef ){
		/* Give back the handle WkCellFor just took: the publication owns the only one. */
		WkCellDrop(pVm,pCell);
		return pCell->pRef;
	}
	/* Built directly rather than through `new`, whose constructor exists only to
	 * refuse (below). */
	pClass = PH7_VmExtractClass(pVm,"WeakReference",sizeof("WeakReference")-1,FALSE,0);
	pRef = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;
	if( pRef == 0 ){
		WkCellDrop(pVm,pCell);
		return 0;
	}
	PH7_NativeSetAttrInt(pVm,pRef,"__h",(sxi64)(sxu64)(sxuptr)pCell);
	pCell->pRef = pRef;
	*pbOwned = 1;
	return pRef;
}
/* WeakReference::create(object $object): WeakReference */
static int vm_builtin_WeakReference_create(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pRef;
	int bOwned = 0;
	if( nArg < 1 || (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pRef = WkRefFor(pCtx->pVm,(ph7_class_instance *)apArg[0]->x.pOther,&bOwned);
	if( pRef == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( bOwned ){
		PH7_NativeResultObject(pCtx,pRef);
	}else{
		SplResultBorrowed(pCtx,pRef);
	}
	return PH7_OK;
}
/* WeakReference::get(): ?object — the target, or null once it has died. */
static int vm_builtin_WeakReference_get(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	VmWeakCell *pCell = pThis ? WkCellOf(pThis) : 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pCell == 0 || pCell->pObj == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	SplResultBorrowed(pCtx,pCell->pObj);
	return PH7_OK;
}
/*
 * WeakReference::__construct()
 *
 * php declares it PUBLIC and refuses to run it: the class has no way to be built
 * except through create(), and the refusal is an Error rather than a visibility
 * failure, so `(new ReflectionClass('WeakReference'))->newInstance()` says the same
 * thing `new WeakReference()` does.
 */
static int vm_builtin_WeakReference_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return PH7_VmThrowException(pCtx,"Error",
		"Direct instantiation of WeakReference is not allowed, use WeakReference::create instead");
}
/* The handle dies with the instance. This is xRelease, not __destruct: php's
 * WeakReference declares no destructor and Reflection must not grow one. */
static void WkRefRelease(ph7_vm *pVm,ph7_class_instance *pThis)
{
	VmWeakCell *pCell = WkCellOf(pThis);
	if( pCell == 0 ){
		return;
	}
	if( pCell->pRef == pThis ){
		pCell->pRef = 0;   /* stop publishing an object that is going away */
	}
	PH7_NativeSetAttrInt(pVm,pThis,"__h",0);
	WkCellDrop(pVm,pCell);
}
/*
 * WeakMap.
 *
 * Two private arrays keyed by the target's object id (`spl_object_id`, which this
 * engine never reuses): __r holds the WeakReference for the key, __v the value
 * mapped to it. Building the weakness out of WeakReference rather than out of raw
 * cells is what makes `clone $map` right for free -- the copied array increments
 * each WeakReference's own reference count, and no cell is dropped twice.
 */
#define WM_REFS "__r"
#define WM_VALS "__v"
/* One of the two backing arrays, materialized and separated from any copy that
 * shares it (a cloned WeakMap starts out sharing both). */
static ph7_hashmap * WmStore(ph7_vm *pVm,ph7_class_instance *pWm,const char *zSlot)
{
	ph7_value *pSlot = PH7_NativeAttr(pWm,zSlot);
	if( pSlot == 0 ){
		return 0;
	}
	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){
		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){
			return 0;
		}
	}
	return PH7_HashmapCowSeparate(pVm,pSlot);
}
/* The entry for an object id, or NULL. */
static ph7_hashmap_node * WmFind(ph7_vm *pVm,ph7_hashmap *pMap,sxi64 iId)
{
	ph7_value sKey;
	ph7_hashmap_node *pNode = 0;
	if( pMap == 0 ){
		return 0;
	}
	PH7_MemObjInitFromInt(pVm,&sKey,iId);
	if( PH7_HashmapLookup(pMap,&sKey,&pNode) != SXRET_OK ){
		pNode = 0;
	}
	PH7_MemObjRelease(&sKey);
	return pNode;
}
static void WmPut(ph7_vm *pVm,ph7_hashmap *pMap,sxi64 iId,ph7_value *pVal)
{
	ph7_value sKey;
	if( pMap == 0 ){
		return;
	}
	PH7_MemObjInitFromInt(pVm,&sKey,iId);
	PH7_HashmapInsert(pMap,&sKey,pVal);
	PH7_MemObjRelease(&sKey);
}
static void WmErase(ph7_vm *pVm,ph7_hashmap *pMap,sxi64 iId)
{
	ph7_hashmap_node *pNode = WmFind(pVm,pMap,iId);
	if( pNode ){
		PH7_HashmapUnlinkNode(pNode,TRUE);
	}
}
/* The target a __r entry still points at, or NULL once it has died. */
static ph7_class_instance * WmNodeTarget(ph7_hashmap_node *pNode)
{
	ph7_value *pVal = pNode ? HashmapExtractNodeValue(pNode) : 0;
	VmWeakCell *pCell;
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_OBJ) == 0 ){
		return 0;
	}
	pCell = WkCellOf((ph7_class_instance *)pVal->x.pOther);
	return pCell ? pCell->pObj : 0;
}
/* Forget every entry whose key has died. php prunes on count() and on iteration,
 * which is the only reason a WeakMap's size ever changes on its own. */
static void WmPrune(ph7_vm *pVm,ph7_class_instance *pWm)
{
	ph7_hashmap *pRefs = WmStore(pVm,pWm,WM_REFS);
	ph7_hashmap *pVals = WmStore(pVm,pWm,WM_VALS);
	ph7_hashmap_node *pNode,*pNext;
	if( pRefs == 0 ){
		return;
	}
	/* pFirst then the pPrev chain IS insertion order here: MACRO_LD_PUSH links a
	 * new node in through pNext, so pNext points at the OLDER neighbour. */
	for( pNode = pRefs->pFirst ; pNode ; pNode = pNext ){
		pNext = pNode->pPrev;
		if( WmNodeTarget(pNode) == 0 ){
			sxi64 iId = pNode->xKey.iKey;
			PH7_HashmapUnlinkNode(pNode,TRUE);
			WmErase(pVm,pVals,iId);
		}
	}
}
/* Every WeakMap entry point but count() takes an object key and says so the same
 * way php does. Answers the id, or -1 after raising the TypeError. */
static sxi64 WmKeyId(ph7_context *pCtx,int nArg,ph7_value **apArg,ph7_class_instance **ppObj)
{
	if( nArg < 1 || (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){
		PH7_VmThrowException(pCtx,"TypeError","WeakMap key must be an object");
		return -1;
	}
	*ppObj = (ph7_class_instance *)apArg[0]->x.pOther;
	return (sxi64)(*ppObj)->nObjId;
}
static int vm_builtin_WeakMap_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pObj = 0;
	ph7_hashmap *pRefs,*pVals;
	sxi64 iId = WmKeyId(pCtx,nArg,apArg,&pObj);
	if( pThis == 0 || iId < 0 ){
		return PH7_OK;
	}
	pRefs = WmStore(pVm,pThis,WM_REFS);
	pVals = WmStore(pVm,pThis,WM_VALS);
	if( WmFind(pVm,pRefs,iId) == 0 ){
		/* First value for this key: hold it through the very WeakReference
		 * create() publishes, so the map and userland share one cell. */
		int bOwned = 0;
		ph7_class_instance *pRef = WkRefFor(pVm,pObj,&bOwned);
		ph7_value sRef;
		if( pRef == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		PH7_MemObjInit(pVm,&sRef);
		sRef.x.pOther = pRef;
		MemObjSetType(&sRef,MEMOBJ_OBJ);
		WmPut(pVm,pRefs,iId,&sRef);   /* takes a reference of its own */
		if( bOwned ){
			PH7_ClassInstanceUnref(pRef);
		}
	}
	WmPut(pVm,pVals,iId,nArg > 1 ? apArg[1] : 0);
	return PH7_OK;
}
static int vm_builtin_WeakMap_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pObj = 0;
	ph7_hashmap_node *pNode;
	sxi64 iId = WmKeyId(pCtx,nArg,apArg,&pObj);
	if( pThis == 0 || iId < 0 ){
		return PH7_OK;
	}
	if( WmNodeTarget(WmFind(pVm,WmStore(pVm,pThis,WM_REFS),iId)) != pObj ){
		return PH7_VmThrowException(pCtx,"Error","Object %z#%d not contained in WeakMap",
			&pObj->pClass->sName,(int)pObj->nObjId);
	}
	pNode = WmFind(pVm,WmStore(pVm,pThis,WM_VALS),iId);
	if( pNode ){
		ph7_result_value(pCtx,HashmapExtractNodeValue(pNode));
	}
	return PH7_OK;
}
static int vm_builtin_WeakMap_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pObj = 0;
	sxi64 iId = WmKeyId(pCtx,nArg,apArg,&pObj);
	if( pThis == 0 || iId < 0 ){
		return PH7_OK;
	}
	ph7_result_bool(pCtx,WmNodeTarget(WmFind(pVm,WmStore(pVm,pThis,WM_REFS),iId)) == pObj);
	return PH7_OK;
}
static int vm_builtin_WeakMap_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pObj = 0;
	sxi64 iId = WmKeyId(pCtx,nArg,apArg,&pObj);
	if( pThis == 0 || iId < 0 ){
		return PH7_OK;
	}
	WmErase(pVm,WmStore(pVm,pThis,WM_REFS),iId);
	WmErase(pVm,WmStore(pVm,pThis,WM_VALS),iId);
	return PH7_OK;
}
static int vm_builtin_WeakMap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_hashmap *pRefs;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	WmPrune(pVm,pThis);
	pRefs = WmStore(pVm,pThis,WM_REFS);
	ph7_result_int64(pCtx,pRefs ? (ph7_int64)pRefs->nEntry : 0);
	return PH7_OK;
}
/*
 * The WeakMap walk, as the vtable an InternalIterator drives.
 *
 * The cursor is the object id it sits on (POS), plus the id it expects to move to
 * (AUX). Both are looked up fresh at every step, which is what makes the walk LIVE:
 * an entry added during a foreach is reached (it is the current node's new pNext),
 * one removed ahead of the cursor is skipped, and removing the CURRENT entry -- the
 * `foreach($m as $k=>$v) unset($m[$k]);` idiom -- still lands on the successor AUX
 * recorded when the cursor settled.
 */
static void WmSettle(ph7_vm *pVm,ph7_class_instance *pIt,ph7_hashmap_node *pNode)
{
	ph7_class_instance *pWm = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);
	ph7_class_instance *pTarget = 0;
	ph7_hashmap_node *pVal;
	ph7_value *pSlot;
	while( pNode ){
		pTarget = WmNodeTarget(pNode);
		if( pTarget ){
			break;
		}
		pNode = pNode->pPrev;   /* a key that died since the last prune */
	}
	if( pNode == 0 || pTarget == 0 || pWm == 0 ){
		PH7_NativeSetAttrBool(pVm,pIt,PH7_NATIVE_IT_DONE,1);
		return;
	}
	PH7_NativeSetAttrInt(pVm,pIt,PH7_NATIVE_IT_POS,pNode->xKey.iKey);
	PH7_NativeSetAttrInt(pVm,pIt,PH7_NATIVE_IT_AUX,pNode->pPrev ? pNode->pPrev->xKey.iKey : -1);
	PH7_NativeSetAttrObj(pVm,pIt,PH7_NATIVE_IT_KEY,pTarget);
	pVal = WmFind(pVm,WmStore(pVm,pWm,WM_VALS),pNode->xKey.iKey);
	pSlot = pVal ? PH7_NativeAttr(pIt,PH7_NATIVE_IT_CUR) : 0;
	if( pSlot ){
		PH7_MemObjStore(HashmapExtractNodeValue(pVal),pSlot);
	}else{
		PH7_NativeSetAttrObj(pVm,pIt,PH7_NATIVE_IT_CUR,0);   /* stores NULL */
	}
	PH7_NativeSetAttrBool(pVm,pIt,PH7_NATIVE_IT_DONE,0);
}
static void WmRewind(ph7_vm *pVm,ph7_class_instance *pIt)
{
	ph7_class_instance *pWm = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);
	ph7_hashmap *pRefs;
	if( pWm == 0 ){
		PH7_NativeSetAttrBool(pVm,pIt,PH7_NATIVE_IT_DONE,1);
		return;
	}
	WmPrune(pVm,pWm);
	pRefs = WmStore(pVm,pWm,WM_REFS);
	WmSettle(pVm,pIt,pRefs ? pRefs->pFirst : 0);
}
static void WmNext(ph7_vm *pVm,ph7_class_instance *pIt)
{
	ph7_class_instance *pWm = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);
	ph7_hashmap *pRefs = pWm ? WmStore(pVm,pWm,WM_REFS) : 0;
	ph7_hashmap_node *pNode = WmFind(pVm,pRefs,PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS));
	if( pNode ){
		pNode = pNode->pPrev;   /* the next-inserted entry; see WmPrune */
	}else{
		/* The entry we were sitting on is gone: fall back on the successor
		 * recorded when it settled. */
		sxi64 iAux = PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_AUX);
		pNode = iAux < 0 ? 0 : WmFind(pVm,pRefs,iAux);
	}
	WmSettle(pVm,pIt,pNode);
}
static const PH7_NativeIterVtab sWmIterVtab = { WmRewind, WmNext };
/* WeakMap::getIterator(): Iterator — a PHP GENERATOR before, which a C body cannot
 * be; php answers an InternalIterator here, and so does this. */
static int vm_builtin_WeakMap_getIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pIt;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 ){
		return PH7_OK;
	}
	pIt = PH7_NativeIteratorNew(pCtx->pVm,pThis);
	if( pIt == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeResultObject(pCtx,pIt);
	return PH7_OK;
}
/*
 * Declare both classes. WeakMap's three interfaces all declare METHODS, so they are
 * attached only once its own exist -- PH7_ClassImplement stubs a missing one as
 * ABSTRACT, which would leave the class uninstantiable.
 */
static sxi32 VmInstallWeak(ph7_vm *pVm)
{
	static const PH7_NativePropDef aRefProp[] = {
		/* The shared cell, as a pointer. Private to a final class and never handed
		 * to PHP -- what `__weak_create()` used to return into a userland slot. */
		{ "__h", PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeMethodDef aRefMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "", "", vm_builtin_WeakReference_construct },
		{ "create",      PH7_MOD_PUBLIC|PH7_MOD_STATIC, "object $object", "WeakReference",
		  vm_builtin_WeakReference_create },
		{ "get",         PH7_MOD_PUBLIC, "", "?object", vm_builtin_WeakReference_get },
	};
	static const PH7_NativePropDef aMapProp[] = {
		{ WM_REFS, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ WM_VALS, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeMethodDef aMapMethod[] = {
		/* php leaves the key parameter UNTYPED and screens it in the body, so that
		 * a scalar key is "WeakMap key must be an object" rather than a ZPP report. */
		/* php's declaration order, which is the order Reflection reports. */
		{ "offsetGet",    PH7_MOD_PUBLIC, "$object", "mixed", vm_builtin_WeakMap_offsetGet },
		{ "offsetSet",    PH7_MOD_PUBLIC, "$object, mixed $value", "void", vm_builtin_WeakMap_offsetSet },
		{ "offsetExists", PH7_MOD_PUBLIC, "$object", "bool", vm_builtin_WeakMap_offsetExists },
		{ "offsetUnset",  PH7_MOD_PUBLIC, "$object", "void", vm_builtin_WeakMap_offsetUnset },
		{ "count",        PH7_MOD_PUBLIC, "", "int", vm_builtin_WeakMap_count },
		{ "getIterator",  PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_WeakMap_getIterator },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		/* Uncloneable in php too: the cell `__h` names is shared, and a slot-by-slot
		 * copy would drop it twice. */
		{ "WeakReference", 0, 0, PH7_CLASS_FINAL|PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aRefMethod, SX_ARRAYSIZE(aRefMethod), 0, 0, aRefProp, SX_ARRAYSIZE(aRefProp),
		  WkRefRelease, 0 },
		{ "WeakMap", 0, 0, PH7_CLASS_FINAL|PH7_CLASS_NOSERIALIZE,
		  aMapMethod, SX_ARRAYSIZE(aMapMethod), 0, 0, aMapProp, SX_ARRAYSIZE(aMapProp),
		  0, &sWmIterVtab },
	};
	static const char *azMapIface[] = { "ArrayAccess", "Countable", "IteratorAggregate" };
	ph7_class *pMap;
	sxu32 n;
	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
	if( rc != SXRET_OK ){
		return rc;
	}
	pMap = PH7_VmExtractClass(&(*pVm),"WeakMap",sizeof("WeakMap")-1,FALSE,0);
	if( pMap == 0 ){
		return SXERR_NOTFOUND;
	}
	for( n = 0 ; n < SX_ARRAYSIZE(azMapIface) ; n++ ){
		ph7_class *pIface = PH7_VmExtractClass(&(*pVm),azMapIface[n],
			(sxu32)SyStrlen(azMapIface[n]),FALSE,0);
		if( pIface == 0 ){
			return SXERR_NOTFOUND;
		}
		rc = PH7_ClassImplement(pMap,pIface);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	return SXRET_OK;
}

/*
 * ArrayIterator / ArrayObject — the array STORE, in C.
 *
 * These two shared one implementation through `trait __SplStoreT`, the last PHL-only
 * TRAIT and the last name in the §4 ledger that was not a function. php shares nothing
 * between them at the TYPE level: both have no parent and no common interface beyond
 * ArrayAccess/Countable, and the storage lives in ext/spl's own `spl_array_object` struct
 * behind handlers. The recorded decision follows php: no shared type at all —
 * one set of C bodies, named by BOTH spec rows. The builder installs a method table per
 * class anyway, so "replaying the method table" is a second row and nothing else, and the
 * php-visible shape stays exact (a native abstract BASE would have given both classes a
 * parent php does not have).
 *
 * The store itself stays a plain PHP array in a declared private slot, exactly as the trait
 * had it: nothing here is a C handle, so `clone` and `serialize()` keep working as php's do
 * and neither class wants the NOCLONE/NOSERIALIZE flags an engine-state class needs. The
 * bodies delegate to the engine's OWN array builtins (asort, ksort, uasort, reset, current,
 * next, key), which is what the PHP did — one layer down, with no dispatcher round trip.
 */
#define SPL_D  "__d"  /* the stored array */
#define SPL_F  "__f"  /* the flags word */
#define SPL_IT "__it" /* ArrayObject's iterator class name */
/*
 * The instance's storage slot, separated for writing (every caller may mutate it). Answers
 * the SLOT rather than the hashmap because that is what the array builtins below take.
 */
static ph7_value * SplStoreSlot(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,SPL_D) : 0;
	if( pSlot == 0 ){
		return 0;
	}
	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){
		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){
			return 0;
		}
	}
	if( PH7_HashmapCowSeparate(pVm,pSlot) == 0 ){
		return 0;
	}
	return pSlot;
}
static ph7_hashmap * SplStore(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_value *pSlot = SplStoreSlot(pVm,pThis);
	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;
}
/*
 * `$this->__d = $array` for the constructor and exchangeArray(), with php's refusal.
 *
 * php DECLARES `object|array $array` — which is what Reflection prints — and then words the
 * refusal as `must be of type array`, so the shared ZPP screen cannot say both (rule 41's
 * shape) and the check is written here. An OBJECT contributes its properties, as the PHP
 * did through get_object_vars().
 */
static sxi32 SplInitStore(ph7_context *pCtx,ph7_class_instance *pThis,
	ph7_value *pArray,const char *zOwner)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,SPL_D) : 0;
	if( pSlot == 0 ){
		return PH7_OK;
	}
	if( pArray == 0 ){
		/* No argument at all: php's `$array = []` default. A native method has no compiled
		 * parameter records for the defaults to live in (rule 33's neighbour), so the body
		 * applies it — and an EXPLICIT null still has to reach the refusal below, which is
		 * why the two cases are distinguished here rather than by a NULL check. */
		ph7_hashmap *pEmpty = PH7_NewHashmap(pVm,0,0);
		if( pEmpty == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		PH7_MemObjRelease(pSlot);
		pSlot->x.pOther = pEmpty;
		MemObjSetType(pSlot,MEMOBJ_HASHMAP);
		return PH7_OK;
	}
	if( pArray->iFlags & MEMOBJ_HASHMAP ){
		PH7_MemObjRelease(pSlot);
		PH7_MemObjStore(pArray,pSlot); /* a copy: the store is the object's own */
		return PH7_OK;
	}
	if( pArray->iFlags & MEMOBJ_OBJ ){
		/* The PHP read get_object_vars($array): the properties this scope can see, by
		 * their plain names. php itself keeps the OBJECT and reads its property table
		 * live (so getArrayCopy() answers the mangled private names and count() answers
		 * the visible ones) — a divergence this conversion carries over unchanged rather
		 * than widening, recorded in §7.4. */
		ph7_class_instance *pObj = (ph7_class_instance *)pArray->x.pOther;
		ph7_hashmap *pMap;
		SyHashEntry *pEntry;
		PH7_MemObjRelease(pSlot);
		pMap = PH7_NewHashmap(pVm,0,0);
		if( pMap == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		SyHashResetLoopCursor(&pObj->hAttr);
		while( pObj && (pEntry = SyHashGetNextEntry(&pObj->hAttr)) != 0 ){
			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;
			ph7_value sKey;
			ph7_value *pVal;
			if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT) ){
				continue;
			}
			if( pVmAttr->pAttr->iProtection != PH7_CLASS_PROT_PUBLIC ){
				continue;
			}
			pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx);
			if( pVal == 0 ){
				continue;
			}
			PH7_MemObjInitFromString(pVm,&sKey,&pVmAttr->pAttr->sName);
			PH7_HashmapInsert(pMap,&sKey,pVal);
			PH7_MemObjRelease(&sKey);
		}
		pSlot->x.pOther = pMap;
		MemObjSetType(pSlot,MEMOBJ_HASHMAP);
		return PH7_OK;
	}
	return PH7_VmThrowException(pCtx,"TypeError",
		"%s(): Argument #1 ($array) must be of type array, %s given",
		zOwner,ph7_type_name(pArray));
}
/* Hand one of the engine's own array builtins the instance's storage slot. */
static int SplArrayCall(ph7_context *pCtx,ProchHostFunction xFunc,ph7_value *pExtra)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *apCall[2];
	ph7_value *pSlot = SplStoreSlot(pCtx->pVm,pThis);
	if( pSlot == 0 ){
		return PH7_OK;
	}
	apCall[0] = pSlot;
	apCall[1] = pExtra;
	return xFunc(pCtx,pExtra ? 2 : 1,apCall);
}
static int vm_builtin_SplStore_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));
	ph7_hashmap_node *pNode = 0;
	int bFound = 0;
	if( pMap && nArg > 0 ){
		/* array_key_exists(), not isset(): php's offsetExists() answers true for a key
		 * holding NULL (the PHP said array_key_exists too). */
		bFound = PH7_HashmapLookup(pMap,apArg[0],&pNode) == SXRET_OK;
	}
	ph7_result_bool(pCtx,bFound);
	return PH7_OK;
}
static int vm_builtin_SplStore_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_hashmap *pMap = SplStore(pVm,PH7_ContextThis(pCtx));
	ph7_hashmap_node *pNode = 0;
	if( pMap == 0 || nArg < 1 || PH7_HashmapLookup(pMap,apArg[0],&pNode) != SXRET_OK ){
		/* php warns "Undefined array key" for a missing offset, with the key rendered
		 * the way the LOOKUP folded it (an integer bare, a string quoted) — the same
		 * pair OP_LOAD_IDX prints. This one now reports the CALLER's line, where the
		 * PHP reported the chunk's. */
		if( nArg > 0 ){
			SyBlob sMsg;
			SyBlobInit(&sMsg,&pVm->sAllocator);
			if( PH7_HashmapKeyIsInt(apArg[0]) ){
				if( (apArg[0]->iFlags & MEMOBJ_INT) == 0 ){
					PH7_MemObjToInteger(apArg[0]);
				}
				SyBlobFormat(&sMsg,"Undefined array key %qd",apArg[0]->x.iVal);
			}else{
				SyString sKey;
				SyStringInitFromBuf(&sKey,SyBlobData(&apArg[0]->sBlob),
					SyBlobLength(&apArg[0]->sBlob));
				SyBlobFormat(&sMsg,"Undefined array key \"%z\"",&sKey);
			}
			SyBlobNullAppend(&sMsg);
			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));
			SyBlobRelease(&sMsg);
		}
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	ph7_result_value(pCtx,(ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx));
	return PH7_OK;
}
/*
 * Insert into the store, keeping php's cursor rule.
 *
 * php's ArrayIterator position is an INTEGER index into the bucket array, so a
 * cursor that ran off the end sits AT the element count: inserting a new key there
 * makes it valid again and the iterator RESUMES on the element just added. PHL
 * carries a node POINTER, which is null past the end and loses that. Re-point it
 * here -- the only place the difference shows, since overwriting an EXISTING key
 * inserts no node and php's dead cursor stays dead. AppendIterator depends on this:
 * php's append() after exhaustion is what makes the walk continue.
 */
static void SplStoreInsert(ph7_hashmap *pMap,ph7_value *pKey,ph7_value *pVal)
{
	sxu32 nBefore = pMap->nEntry;
	int bPastEnd = pMap->pCur == 0;
	PH7_HashmapInsert(pMap,pKey,pVal);
	if( bPastEnd && pMap->nEntry > nBefore ){
		pMap->pCur = pMap->pLast;
	}
}
static int vm_builtin_SplStore_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));
	if( pMap && nArg > 1 ){
		/* A NULL key is `$o[] = $v` — the append form, which is how php's offsetSet()
		 * receives it. */
		SplStoreInsert(pMap,(apArg[0]->iFlags & MEMOBJ_NULL) ? 0 : apArg[0],apArg[1]);
	}
	return PH7_OK;
}
static int vm_builtin_SplStore_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));
	ph7_hashmap_node *pNode = 0;
	if( pMap && nArg > 0 && PH7_HashmapLookup(pMap,apArg[0],&pNode) == SXRET_OK ){
		PH7_HashmapUnlinkNode(pNode,TRUE);
	}
	return PH7_OK;
}
static int vm_builtin_SplStore_append(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));
	if( pMap && nArg > 0 ){
		SplStoreInsert(pMap,0,apArg[0]);
	}
	return PH7_OK;
}
static int vm_builtin_SplStore_getArrayCopy(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pSlot = SplStoreSlot(pCtx->pVm,PH7_ContextThis(pCtx));
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pSlot ){
		ph7_result_value(pCtx,pSlot); /* a COPY: the caller must not alias the store */
	}
	return PH7_OK;
}
static int vm_builtin_SplStore_count(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);
	return PH7_OK;
}
static int vm_builtin_SplStore_getFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_int64(pCtx,pThis ? PH7_NativeAttrInt(pThis,SPL_F) : 0);
	return PH7_OK;
}
static int vm_builtin_SplStore_setFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	if( pThis && nArg > 0 ){
		PH7_NativeSetAttrInt(pCtx->pVm,pThis,SPL_F,ph7_value_to_int(apArg[0]));
	}
	return PH7_OK;
}
/*
 * The six sorts. Each is the engine's own builtin over the stored array — including the
 * `$flags` the PHP DROPPED on the floor (`asort($this->__d)` ignored its own parameter, so
 * `$it->asort(SORT_STRING)` sorted numerically). natsort/natcasesort go through asort with
 * php's own flag pair rather than by name: the shared body reads ph7_function_name() to tell
 * the two apart, and a native method's name is `ArrayIterator::natcasesort`.
 */
static int vm_builtin_SplStore_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SplArrayCall(pCtx,ph7_hashmap_asort,nArg > 0 ? apArg[0] : 0);
}
static int vm_builtin_SplStore_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SplArrayCall(pCtx,ph7_hashmap_ksort,nArg > 0 ? apArg[0] : 0);
}
static int vm_builtin_SplStore_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SplArrayCall(pCtx,ph7_hashmap_uasort,nArg > 0 ? apArg[0] : 0);
}
static int vm_builtin_SplStore_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SplArrayCall(pCtx,ph7_hashmap_uksort,nArg > 0 ? apArg[0] : 0);
}
static int SplNatSort(ph7_context *pCtx,int bFold)
{
	ph7_value sFlags;
	int rc;
	/* SORT_NATURAL (6), plus SORT_FLAG_CASE (8) for the folding twin — the same pair
	 * ph7_hashmap_natsort forwards to asort(). */
	PH7_MemObjInitFromInt(pCtx->pVm,&sFlags,bFold ? (6|8) : 6);
	rc = SplArrayCall(pCtx,ph7_hashmap_asort,&sFlags);
	PH7_MemObjRelease(&sFlags);
	return rc;
}
static int vm_builtin_SplStore_natsort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return SplNatSort(pCtx,0);
}
static int vm_builtin_SplStore_natcasesort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return SplNatSort(pCtx,1);
}
/* ArrayIterator's cursor: the stored array's own internal pointer, as the PHP had it. */
static int vm_builtin_ArrayIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pMap == 0 || pMap->pCur == 0 ){
		/* Past the end php answers NULL, where current() the FUNCTION answers false. */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return SplArrayCall(pCtx,ph7_hashmap_current,0);
}
static int vm_builtin_ArrayIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return SplArrayCall(pCtx,ph7_hashmap_simple_key,0);
}
static int vm_builtin_ArrayIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	SplArrayCall(pCtx,ph7_hashmap_next,0);
	ph7_result_null(pCtx); /* next() the METHOD returns void */
	return PH7_OK;
}
static int vm_builtin_ArrayIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	SplArrayCall(pCtx,ph7_hashmap_reset,0);
	ph7_result_null(pCtx);
	return PH7_OK;
}
static int vm_builtin_ArrayIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx,pMap && pMap->pCur ? 1 : 0);
	return PH7_OK;
}
static int vm_builtin_ArrayIterator_seek(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));
	ph7_int64 iOffset = nArg > 0 ? ph7_value_to_int(apArg[0]) : 0;
	ph7_int64 i;
	if( pMap == 0 ){
		return PH7_OK;
	}
	if( iOffset < 0 || iOffset >= (ph7_int64)pMap->nEntry ){
		return PH7_VmThrowException(pCtx,"OutOfBoundsException",
			"Seek position %qd is out of range",iOffset);
	}
	pMap->pCur = pMap->pFirst;
	for( i = 0 ; i < iOffset && pMap->pCur ; ++i ){
		pMap->pCur = pMap->pCur->pPrev; /* insertion order: pFirst, then the pPrev chain */
	}
	return PH7_OK;
}
static int vm_builtin_ArrayIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_hashmap *pMap;
	sxi32 rc = SplInitStore(pCtx,pThis,nArg > 0 ? apArg[0] : 0,
		"ArrayIterator::__construct");
	if( rc != PH7_OK ){
		return rc;
	}
	if( pThis && nArg > 1 ){
		PH7_NativeSetAttrInt(pVm,pThis,SPL_F,ph7_value_to_int(apArg[1]));
	}
	pMap = SplStore(pVm,pThis);
	if( pMap ){
		pMap->pCur = pMap->pFirst; /* reset($this->__d) */
	}
	return PH7_OK;
}
/* ArrayObject */
static int vm_builtin_ArrayObject_setIteratorClass(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zName;
	int nName;
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	if( nName != (int)sizeof("ArrayIterator")-1
	 || SyMemcmp(zName,"ArrayIterator",sizeof("ArrayIterator")-1) != 0 ){
		ph7_class *pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,FALSE,0);
		ph7_class *pBase = PH7_VmExtractClass(pVm,"ArrayIterator",
			sizeof("ArrayIterator")-1,FALSE,0);
		if( pClass == 0 || pBase == 0 || pClass == pBase
		 || !PH7_VmInstanceOf(pClass,pBase) ){
			return PH7_VmThrowException(pCtx,"TypeError",
				"ArrayObject::setIteratorClass(): Argument #1 ($iteratorClass) must be "
				"a class name derived from ArrayIterator, %.*s given",nName,zName);
		}
	}
	PH7_NativeSetAttrStr(pVm,pThis,SPL_IT,zName,nName);
	return PH7_OK;
}
static int vm_builtin_ArrayObject_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi32 rc = SplInitStore(pCtx,pThis,nArg > 0 ? apArg[0] : 0,
		"ArrayObject::__construct");
	if( rc != PH7_OK ){
		return rc;
	}
	if( pThis && nArg > 1 ){
		PH7_NativeSetAttrInt(pVm,pThis,SPL_F,ph7_value_to_int(apArg[1]));
	}
	if( nArg > 2 ){
		return vm_builtin_ArrayObject_setIteratorClass(pCtx,1,&apArg[2]);
	}
	return PH7_OK;
}
static int vm_builtin_ArrayObject_exchangeArray(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,SPL_D) : 0;
	sxi32 rc;
	if( pSlot ){
		ph7_result_value(pCtx,pSlot); /* the OLD store is the return value */
	}
	rc = SplInitStore(pCtx,pThis,nArg > 0 ? apArg[0] : 0,"ArrayObject::exchangeArray");
	return rc;
}
static int vm_builtin_ArrayObject_getIteratorClass(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zName = 0;
	int nName = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		PH7_NativeAttrStr(pThis,SPL_IT,&zName,&nName);
	}
	ph7_result_string(pCtx,nName > 0 ? zName : "ArrayIterator",nName > 0 ? nName : -1);
	return PH7_OK;
}
static int vm_builtin_ArrayObject_getIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pIt;
	ph7_class *pClass;
	const char *zName = 0;
	int nName = 0;
	ph7_value *pSlot;
	ph7_class_method *pCons;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 ){
		return PH7_OK;
	}
	PH7_NativeAttrStr(pThis,SPL_IT,&zName,&nName);
	if( nName < 1 ){
		zName = "ArrayIterator";
		nName = (int)sizeof("ArrayIterator")-1;
	}
	pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,FALSE,0);
	if( pClass == 0 ){
		return PH7_OK;
	}
	pIt = PH7_NewClassInstance(pVm,pClass);
	if( pIt == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	pIt->iRef++;
	pSlot = SplStoreSlot(pVm,pThis);
	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);
	if( pCons && pSlot ){
		/* `new $c($this->__d)`: the iterator gets a COPY of the store, as the PHP did —
		 * a user subclass of ArrayIterator runs its own constructor here. */
		ph7_value *apCtor[1];
		apCtor[0] = pSlot;
		PH7_VmCallClassMethod(pVm,pIt,pCons,0,1,apCtor);
	}
	PH7_NativeResultObject(pCtx,pIt);
	PH7_ClassInstanceUnref(pIt);
	return PH7_OK;
}
/*
 * ARRAY_AS_PROPS (flag 2) reaches the store through the four magic accessors, which is how
 * the PHP did it. php has no such methods — it implements the flag in its property handler,
 * so `getMethods()` does not list them (a surface divergence carried over, §7.4).
 */
static int vm_builtin_ArrayObject_get(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_hashmap *pMap;
	ph7_hashmap_node *pNode = 0;
	if( pThis == 0 || nArg < 1 || (PH7_NativeAttrInt(pThis,SPL_F) & 2) == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pMap = SplStore(pVm,pThis);
	if( pMap && PH7_HashmapLookup(pMap,apArg[0],&pNode) == SXRET_OK ){
		ph7_result_value(pCtx,(ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx));
		return PH7_OK;
	}
	/* `?? null`: a missing key must not raise the undefined-key warning from in here —
	 * php reports the missing PROPERTY, and PHL's magic-read path already does. */
	ph7_result_null(pCtx);
	return PH7_OK;
}
static int vm_builtin_ArrayObject_set(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_hashmap *pMap;
	if( pThis == 0 || nArg < 2 || (PH7_NativeAttrInt(pThis,SPL_F) & 2) == 0 ){
		return PH7_OK;
	}
	pMap = SplStore(pCtx->pVm,pThis);
	if( pMap ){
		PH7_HashmapInsert(pMap,apArg[0],apArg[1]);
	}
	return PH7_OK;
}
static int vm_builtin_ArrayObject_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_hashmap *pMap;
	ph7_hashmap_node *pNode = 0;
	int bSet = 0;
	if( pThis && nArg > 0 && (PH7_NativeAttrInt(pThis,SPL_F) & 2) ){
		pMap = SplStore(pVm,pThis);
		if( pMap && PH7_HashmapLookup(pMap,apArg[0],&pNode) == SXRET_OK ){
			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);
			bSet = pVal && (pVal->iFlags & MEMOBJ_NULL) == 0; /* isset(), not exists */
		}
	}
	ph7_result_bool(pCtx,bSet);
	return PH7_OK;
}
static int vm_builtin_ArrayObject_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_hashmap *pMap;
	ph7_hashmap_node *pNode = 0;
	if( pThis && nArg > 0 && (PH7_NativeAttrInt(pThis,SPL_F) & 2) ){
		pMap = SplStore(pCtx->pVm,pThis);
		if( pMap && PH7_HashmapLookup(pMap,apArg[0],&pNode) == SXRET_OK ){
			PH7_HashmapUnlinkNode(pNode,TRUE);
		}
	}
	return PH7_OK;
}
/*
 * Declare both classes plus SeekableIterator, which ArrayIterator implements and which
 * therefore cannot wait for the chunk. RecursiveArrayIterator still lives there and extends
 * ArrayIterator, so this install has to run BEFORE the chunk is evaluated.
 */
static sxi32 VmInstallSplStore(ph7_vm *pVm)
{
	static const PH7_NativeMethodDef aSeekMethod[] = {
		{ "seek", PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "int $offset", 0, 0 },
	};
	static const PH7_NativePropDef aItProp[] = {
		{ SPL_D, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ SPL_F, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativePropDef aObjProp[] = {
		{ SPL_D,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ SPL_F,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
		{ SPL_IT, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "ArrayIterator", 0.0 }, 0 },
	};
	static const PH7_NativeConstDef aConst[] = {
		{ "STD_PROP_LIST",  PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1, 0, 0.0 },
		{ "ARRAY_AS_PROPS", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },
	};
	/* php's declaration order, which is the order Reflection reports. */
	static const PH7_NativeMethodDef aItMethod[] = {
		{ "__construct",  PH7_MOD_PUBLIC, "object|array $array = [], int $flags = 0", 0,
		  vm_builtin_ArrayIterator_construct },
		{ "offsetExists", PH7_MOD_PUBLIC, "mixed $key", "@bool", vm_builtin_SplStore_offsetExists },
		{ "offsetGet",    PH7_MOD_PUBLIC, "mixed $key", "@mixed", vm_builtin_SplStore_offsetGet },
		{ "offsetSet",    PH7_MOD_PUBLIC, "mixed $key, mixed $value", "@void", vm_builtin_SplStore_offsetSet },
		{ "offsetUnset",  PH7_MOD_PUBLIC, "mixed $key", "@void", vm_builtin_SplStore_offsetUnset },
		{ "append",       PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplStore_append },
		{ "getArrayCopy", PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplStore_getArrayCopy },
		{ "count",        PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplStore_count },
		{ "getFlags",     PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplStore_getFlags },
		{ "setFlags",     PH7_MOD_PUBLIC, "int $flags", "@void", vm_builtin_SplStore_setFlags },
		{ "asort",        PH7_MOD_PUBLIC, "int $flags = 0", "@true", vm_builtin_SplStore_asort },
		{ "ksort",        PH7_MOD_PUBLIC, "int $flags = 0", "@true", vm_builtin_SplStore_ksort },
		{ "uasort",       PH7_MOD_PUBLIC, "callable $callback", "@true", vm_builtin_SplStore_uasort },
		{ "uksort",       PH7_MOD_PUBLIC, "callable $callback", "@true", vm_builtin_SplStore_uksort },
		{ "natsort",      PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplStore_natsort },
		{ "natcasesort",  PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplStore_natcasesort },
		{ "current",      PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_ArrayIterator_current },
		{ "key",          PH7_MOD_PUBLIC, "", "@string|int|null", vm_builtin_ArrayIterator_key },
		{ "next",         PH7_MOD_PUBLIC, "", "@void", vm_builtin_ArrayIterator_next },
		{ "rewind",       PH7_MOD_PUBLIC, "", "@void", vm_builtin_ArrayIterator_rewind },
		{ "valid",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ArrayIterator_valid },
		{ "seek",         PH7_MOD_PUBLIC, "int $offset", "@void", vm_builtin_ArrayIterator_seek },
	};
	static const PH7_NativeMethodDef aObjMethod[] = {
		{ "__construct",      PH7_MOD_PUBLIC,
		  "object|array $array = [], int $flags = 0, string $iteratorClass = 'ArrayIterator'", 0,
		  vm_builtin_ArrayObject_construct },
		{ "offsetExists",     PH7_MOD_PUBLIC, "mixed $key", "@bool", vm_builtin_SplStore_offsetExists },
		{ "offsetGet",        PH7_MOD_PUBLIC, "mixed $key", "@mixed", vm_builtin_SplStore_offsetGet },
		{ "offsetSet",        PH7_MOD_PUBLIC, "mixed $key, mixed $value", "@void", vm_builtin_SplStore_offsetSet },
		{ "offsetUnset",      PH7_MOD_PUBLIC, "mixed $key", "@void", vm_builtin_SplStore_offsetUnset },
		{ "append",           PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplStore_append },
		{ "getArrayCopy",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplStore_getArrayCopy },
		{ "count",            PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplStore_count },
		{ "getFlags",         PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplStore_getFlags },
		{ "setFlags",         PH7_MOD_PUBLIC, "int $flags", "@void", vm_builtin_SplStore_setFlags },
		{ "asort",            PH7_MOD_PUBLIC, "int $flags = 0", "@true", vm_builtin_SplStore_asort },
		{ "ksort",            PH7_MOD_PUBLIC, "int $flags = 0", "@true", vm_builtin_SplStore_ksort },
		{ "uasort",           PH7_MOD_PUBLIC, "callable $callback", "@true", vm_builtin_SplStore_uasort },
		{ "uksort",           PH7_MOD_PUBLIC, "callable $callback", "@true", vm_builtin_SplStore_uksort },
		{ "natsort",          PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplStore_natsort },
		{ "natcasesort",      PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplStore_natcasesort },
		{ "exchangeArray",    PH7_MOD_PUBLIC, "object|array $array", "@array", vm_builtin_ArrayObject_exchangeArray },
		{ "getIterator",      PH7_MOD_PUBLIC, "", "@Iterator", vm_builtin_ArrayObject_getIterator },
		{ "setIteratorClass", PH7_MOD_PUBLIC, "string $iteratorClass", "@void", vm_builtin_ArrayObject_setIteratorClass },
		{ "getIteratorClass", PH7_MOD_PUBLIC, "", "@string", vm_builtin_ArrayObject_getIteratorClass },
		{ "__get",            PH7_MOD_PUBLIC, "$name", 0, vm_builtin_ArrayObject_get },
		{ "__set",            PH7_MOD_PUBLIC, "$name, $value", 0, vm_builtin_ArrayObject_set },
		{ "__isset",          PH7_MOD_PUBLIC, "$name", 0, vm_builtin_ArrayObject_isset },
		{ "__unset",          PH7_MOD_PUBLIC, "$name", 0, vm_builtin_ArrayObject_unset },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		/* `interface X extends Iterator` is a PARENT, not an implemented interface:
		 * the compiler puts it in pBase and Reflection walks pBase to answer which
		 * class DECLARED an inherited method. Naming it in zImplements instead made
		 * current()/key()/next()/rewind()/valid() report this interface as their
		 * declaring class where php reports Iterator. */
		{ "SeekableIterator", "Iterator", 0, PH7_CLASS_INTERFACE,
		  aSeekMethod, SX_ARRAYSIZE(aSeekMethod), 0, 0, 0, 0, 0, 0 },
		{ "ArrayIterator", 0, "SeekableIterator,ArrayAccess,Countable", 0,
		  aItMethod, SX_ARRAYSIZE(aItMethod), aConst, SX_ARRAYSIZE(aConst),
		  aItProp, SX_ARRAYSIZE(aItProp), 0, 0 },
		{ "ArrayObject", 0, "IteratorAggregate,ArrayAccess,Countable", 0,
		  aObjMethod, SX_ARRAYSIZE(aObjMethod), aConst, SX_ARRAYSIZE(aConst),
		  aObjProp, SX_ARRAYSIZE(aObjProp), 0, 0 },
	};
	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
}
/*
 * ---------------------------------------------------------------------------
 * The SPL DUAL ITERATORS: IteratorIterator and the decorators built on it.
 *
 * php's `spl_dual_it_object` is a CACHE, and that is the whole design. rewind()
 * and next() move the INNER iterator and then COPY its current()/key() onto the
 * decorator; valid(), current() and key() answer out of that copy and never reach
 * the inner iterator again. The chunk forwarded all five live, which is three
 * observable divergences at once: a fresh decorator was valid() BEFORE rewind()
 * (php answers false — nothing has been fetched yet), current() followed an inner
 * iterator that had been moved behind the decorator's back (php answers what it
 * cached), and a decorator left past the end still answered the inner's stale
 * key(). Everything below is written around the cache because the cache IS the
 * class.
 *
 * Two slots hold what php holds in two fields: `__in` is `inner.zobject` — the
 * object getInnerIterator() answers — and `__it` is `inner.iterator`, the Iterator
 * actually driven. They differ for exactly one input: an IteratorAggregate whose
 * getIterator() answers another IteratorAggregate. php unwraps ONE level in the
 * constructor and lets the engine's get_iterator handler unwrap the rest at
 * iteration time, so `new IteratorIterator($aggOfAgg)` answers the inner AGGREGATE
 * from getInnerIterator() and still iterates. The chunk's `while` loop unwrapped
 * to the bottom and answered the ArrayIterator instead.
 */
#define IT_IN  "__in"   /* php's inner.zobject: what getInnerIterator() answers */
#define IT_IT  "__it"   /* php's inner.iterator: the Iterator actually driven */
#define IT_CD  "__cd"   /* the cached current() */
#define IT_CK  "__ck"   /* the cached key() */
#define IT_CF  "__cf"   /* 1 while the cached pair is live (php's IS_UNDEF check) */
#define IT_CP  "__cp"   /* php's current.pos */
#define IT_OFF "__off"  /* LimitIterator's offset */
#define IT_LIM "__lim"  /* LimitIterator's count, -1 for "all" */
#define IT_CB  "__cb"   /* CallbackFilterIterator's callback */
#define AP_LIST "__ai"  /* AppendIterator's php `u.append.zarrayit`: the real ArrayIterator
                         * holding everything append()ed, whose OWN cursor is php's
                         * `u.append.iterator` -- one position, which is why
                         * getArrayIterator()->rewind() moves getIteratorIndex(). */

/*
 * php's SPL_FETCH_AND_CHECK_DUAL_IT: a subclass whose constructor never called
 * parent::__construct() has no inner iterator, and php refuses every method on it
 * rather than answering a null-flavoured nothing.
 */
static sxi32 DualNotReady(ph7_context *pCtx)
{
	return PH7_VmThrowException(pCtx,"Error",
		"The object is in an invalid state as the parent constructor was not called");
}
static ph7_class_instance * DualDriver(ph7_class_instance *pThis)
{
	return pThis ? PH7_NativeAttrObj(pThis,IT_IT) : 0;
}
static int DualFilled(ph7_class_instance *pThis)
{
	return pThis && PH7_NativeAttrInt(pThis,IT_CF) != 0;
}
/*
 * php's SPL_FETCH_AND_CHECK_DUAL_IT tests `dit_type`, which means "the constructor
 * ran" -- and for every decorator but one that is the same thing as "an inner
 * iterator exists". AppendIterator's constructor takes NO iterator: it builds an
 * empty list and is immediately usable (valid() false, current()/key() null, no
 * refusal), so its readiness lives in the list slot instead.
 */
static int DualReady(ph7_class_instance *pThis)
{
	return pThis && (PH7_NativeAttrObj(pThis,IT_IT) != 0
		|| PH7_NativeAttrObj(pThis,AP_LIST) != 0);
}
/*
 * Assign one of the instance's own slots. The slot pointer is re-resolved here on
 * purpose: it lives inside pVm->aMemObj, a SySet that REALLOCATES as the VM
 * reserves objects, so any pointer taken before a call into user code (and every
 * inner->current() is one) may be stale by the time the call returns.
 */
static void DualSetSlot(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,ph7_value *pVal)
{
	ph7_value *pSlot = PH7_NativeAttr(pThis,zName);
	SXUNUSED(pVm);
	if( pSlot ){
		PH7_MemObjStore(pVal,pSlot);
	}
}
/* php's spl_dual_it_free: drop the cached pair. */
static void DualFree(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_value *pSlot;
	if( pThis == 0 ){
		return;
	}
	pSlot = PH7_NativeAttr(pThis,IT_CD);
	if( pSlot ){ PH7_MemObjRelease(pSlot); }
	pSlot = PH7_NativeAttr(pThis,IT_CK);
	if( pSlot ){ PH7_MemObjRelease(pSlot); }
	PH7_NativeSetAttrInt(pVm,pThis,IT_CF,0);
}
/* Call a zero-argument method on the driven iterator, propagating a throw (rule:
 * a native body that answers PH7_OK with an exception in flight lets the caller
 * carry on). A missing method is the foreach opcode's leniency, not an error. */
static sxi32 DualCall(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nLen,
	ph7_value *pResult)
{
	ph7_class_instance *pIn = DualDriver(pThis);
	if( pIn == 0 ){
		return SXRET_OK;
	}
	return VmIterCallMethod(pVm,pIn,zName,nLen,pResult);
}
/* php's spl_dual_it_valid: the INNER's valid(), not the cache's. */
static sxi32 DualInnerValid(ph7_vm *pVm,ph7_class_instance *pThis,int *pbValid)
{
	ph7_value sVal;
	sxi32 rc;
	*pbValid = 0;
	PH7_MemObjInit(pVm,&sVal);
	rc = DualCall(pVm,pThis,"valid",sizeof("valid")-1,&sVal);
	if( rc == SXRET_OK ){
		PH7_MemObjToBool(&sVal);          /* a STATUS, not the answer */
		*pbValid = sVal.x.iVal != 0;
	}
	PH7_MemObjRelease(&sVal);
	return rc;
}
/*
 * php's spl_dual_it_fetch: refill the cache from the inner iterator. `bCheckMore`
 * is php's check_more — false means "the caller already knows the inner is valid",
 * which is how LimitIterator's seek and InfiniteIterator's wrap-around fetch.
 */
static sxi32 DualFetch(ph7_vm *pVm,ph7_class_instance *pThis,int bCheckMore)
{
	ph7_value sVal;
	sxi32 rc;
	int bValid = 1;
	DualFree(pVm,pThis);
	if( DualDriver(pThis) == 0 ){
		return SXRET_OK;
	}
	if( bCheckMore ){
		rc = DualInnerValid(pVm,pThis,&bValid);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	if( !bValid ){
		return SXRET_OK;
	}
	PH7_MemObjInit(pVm,&sVal);
	rc = DualCall(pVm,pThis,"current",sizeof("current")-1,&sVal);
	if( rc != SXRET_OK ){
		PH7_MemObjRelease(&sVal);
		return rc;
	}
	DualSetSlot(pVm,pThis,IT_CD,&sVal);
	PH7_MemObjRelease(&sVal);
	PH7_MemObjInit(pVm,&sVal);
	rc = DualCall(pVm,pThis,"key",sizeof("key")-1,&sVal);
	if( rc != SXRET_OK ){
		PH7_MemObjRelease(&sVal);
		DualFree(pVm,pThis);   /* php drops the half-filled pair when key() throws */
		return rc;
	}
	DualSetSlot(pVm,pThis,IT_CK,&sVal);
	PH7_MemObjRelease(&sVal);
	PH7_NativeSetAttrInt(pVm,pThis,IT_CF,1);
	return SXRET_OK;
}
/* php's spl_dual_it_rewind: free, position back to zero, rewind the inner. */
static sxi32 DualRewindInner(ph7_vm *pVm,ph7_class_instance *pThis)
{
	DualFree(pVm,pThis);
	PH7_NativeSetAttrInt(pVm,pThis,IT_CP,0);
	return DualCall(pVm,pThis,"rewind",sizeof("rewind")-1,0);
}
/* php's spl_dual_it_next: free, advance the inner, count the step. */
static sxi32 DualNextInner(ph7_vm *pVm,ph7_class_instance *pThis)
{
	sxi32 rc;
	DualFree(pVm,pThis);
	rc = DualCall(pVm,pThis,"next",sizeof("next")-1,0);
	PH7_NativeSetAttrInt(pVm,pThis,IT_CP,PH7_NativeAttrInt(pThis,IT_CP)+1);
	return rc;
}
/* Hand back a cached slot, or php's null for an empty cache. */
static int DualResultSlot(ph7_context *pCtx,const char *zName)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pSlot;
	if( !DualReady(pThis) ){
		return DualNotReady(pCtx);
	}
	if( !DualFilled(pThis) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pSlot = PH7_NativeAttr(pThis,zName);
	if( pSlot ){
		ph7_result_value(pCtx,pSlot);
	}
	return PH7_OK;
}
/*
 * The constructor every dual iterator shares. php words the "already built" refusal
 * with the DECLARING class's name and with getIterator() rather than __construct(),
 * so each class hands its own name in.
 */
static sxi32 DualConstruct(ph7_context *pCtx,const char *zOwner,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pObj;
	ph7_class *pIterCls, *pAggCls, *pTravCls;
	ph7_class *pCast = 0;
	ph7_class_instance *pHold = 0;   /* the unwrapped iterator, kept alive across levels */
	int nLevel;
	if( pThis == 0 ){
		return PH7_OK;
	}
	if( PH7_NativeAttrObj(pThis,IT_IN) != 0 ){
		return PH7_VmThrowException(pCtx,"BadMethodCallException",
			"%s::getIterator() must be called exactly once per instance",zOwner);
	}
	if( nArg < 1 || (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){
		return PH7_OK;   /* the shared ZPP screen already refused a non-object */
	}
	pObj = (ph7_class_instance *)apArg[0]->x.pOther;
	pIterCls = PH7_VmExtractClass(pVm,"Iterator",sizeof("Iterator")-1,FALSE,0);
	pAggCls = PH7_VmExtractClass(pVm,"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);
	pTravCls = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,FALSE,0);
	if( pIterCls && PH7_VmInstanceOf(pObj->pClass,pIterCls) ){
		/* Already an Iterator: php ignores $class entirely on this path. */
		PH7_NativeSetAttrObj(pVm,pThis,IT_IN,pObj);
		PH7_NativeSetAttrObj(pVm,pThis,IT_IT,pObj);
		return PH7_OK;
	}
	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){
		/* php's DOWNCAST: $class names the class whose getIterator() to run, which is
		 * how a subclass asks for its parent's traversal. It must be a base of the
		 * argument AND traversable itself. */
		int nName;
		const char *zName = ph7_value_to_string(apArg[1],&nName);
		pCast = nName > 0 ? PH7_VmExtractClass(pVm,zName,(sxu32)nName,FALSE,0) : 0;
		if( pCast == 0 || !PH7_VmInstanceOf(pObj->pClass,pCast)
		 || (pTravCls && !PH7_VmInstanceOf(pCast,pTravCls)) ){
			return PH7_VmThrowException(pCtx,"LogicException",
				"Class to downcast to not found or not base class or does not implement Traversable");
		}
	}
	/*
	 * An IteratorAggregate: run getIterator() — the DOWNCAST class's when one was
	 * named — and keep its answer as the inner object. php stops after one level
	 * here; the loop below is the engine's get_iterator handler, which resolves the
	 * rest lazily, done eagerly because PHL drives the inner through the METHOD
	 * protocol and nothing else would unwrap it.
	 */
	for( nLevel = 0 ; nLevel < 16 ; ++nLevel ){
		ph7_class *pFrom = pCast ? pCast : pObj->pClass;
		ph7_class_method *pMethod;
		ph7_value sInner;
		sxi32 rc;
		if( pAggCls == 0 || !PH7_VmInstanceOf(pObj->pClass,pAggCls) ){
			break;
		}
		pMethod = PH7_ClassExtractMethod(pFrom,"getIterator",sizeof("getIterator")-1);
		if( pMethod == 0 ){
			break;
		}
		PH7_MemObjInit(pVm,&sInner);
		rc = PH7_VmCallClassMethod(pVm,pObj,pMethod,&sInner,0,0);
		if( rc != SXRET_OK ){
			PH7_MemObjRelease(&sInner);
			return rc;
		}
		if( (sInner.iFlags & MEMOBJ_OBJ) == 0 || sInner.x.pOther == 0
		 || (pTravCls && !PH7_VmInstanceOf(((ph7_class_instance *)sInner.x.pOther)->pClass,pTravCls)) ){
			SyString *pName = &pFrom->sName;
			PH7_MemObjRelease(&sInner);
			return PH7_VmThrowException(pCtx,"LogicException",
				"%z::getIterator() must return an object that implements Traversable",pName);
		}
		pObj = (ph7_class_instance *)sInner.x.pOther;
		pObj->iRef++;                 /* survive the release of the call result */
		PH7_MemObjRelease(&sInner);
		if( pHold ){
			PH7_ClassInstanceUnref(pHold);
		}
		pHold = pObj;                 /* this function owns exactly one reference */
		if( nLevel == 0 ){
			/* php's inner.zobject is the FIRST unwrap and nothing deeper. */
			PH7_NativeSetAttrObj(pVm,pThis,IT_IN,pObj);
		}
		pCast = 0;
		if( pIterCls && PH7_VmInstanceOf(pObj->pClass,pIterCls) ){
			break;
		}
	}
	if( PH7_NativeAttrObj(pThis,IT_IN) == 0 ){
		PH7_NativeSetAttrObj(pVm,pThis,IT_IN,pObj);
	}
	PH7_NativeSetAttrObj(pVm,pThis,IT_IT,pObj);
	if( pHold ){
		PH7_ClassInstanceUnref(pHold);   /* both slots hold their own now */
	}
	return PH7_OK;
}
static int vm_builtin_IteratorIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DualConstruct(pCtx,"IteratorIterator",nArg,apArg);
}
static int vm_builtin_FilterIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DualConstruct(pCtx,"FilterIterator",nArg,apArg);
}
static int vm_builtin_CallbackFilterIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis;
	sxi32 rc;
	if( nArg > 1 ){
		/* The shared ZPP screen leaves `callable` to the builtin's own check (a string
		 * satisfies the declared type; whether it NAMES a function does not), so php's
		 * "must be a valid callback, function "x" not found" only appears if the body
		 * asks for it — as every callback-taking builtin already does. */
		rc = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);
		if( rc != PH7_OK ){
			return rc;
		}
	}
	rc = DualConstruct(pCtx,"CallbackFilterIterator",nArg,apArg);
	pThis = PH7_ContextThis(pCtx);
	if( rc == PH7_OK && pThis && nArg > 1 ){
		DualSetSlot(pCtx->pVm,pThis,IT_CB,apArg[1]);
	}
	return rc;
}
static int vm_builtin_InfiniteIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DualConstruct(pCtx,"InfiniteIterator",nArg,apArg);
}
static int vm_builtin_NoRewindIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DualConstruct(pCtx,"NoRewindIterator",nArg,apArg);
}
static int vm_builtin_Dual_getInnerIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pIn;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !DualReady(pThis) ){
		return DualNotReady(pCtx);
	}
	pIn = PH7_NativeAttrObj(pThis,IT_IN);
	if( pIn ){
		SplResultBorrowed(pCtx,pIn);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
static int vm_builtin_Dual_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !DualReady(pThis) ){
		return DualNotReady(pCtx);
	}
	ph7_result_bool(pCtx,DualFilled(pThis));
	return PH7_OK;
}
static int vm_builtin_Dual_current(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return DualResultSlot(pCtx,IT_CD);
}
static int vm_builtin_Dual_key(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return DualResultSlot(pCtx,IT_CK);
}
static int vm_builtin_IteratorIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi32 rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 || DualDriver(pThis) == 0 ){
		return DualNotReady(pCtx);
	}
	rc = DualRewindInner(pVm,pThis);
	if( rc != SXRET_OK ){
		return rc;
	}
	return DualFetch(pVm,pThis,TRUE);
}
static int vm_builtin_IteratorIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi32 rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 || DualDriver(pThis) == 0 ){
		return DualNotReady(pCtx);
	}
	rc = DualNextInner(pVm,pThis);
	if( rc != SXRET_OK ){
		return rc;
	}
	return DualFetch(pVm,pThis,TRUE);
}
/*
 * FilterIterator. php's spl_filter_it_fetch: fetch, ask accept(), and on a refusal
 * step the INNER on directly — without counting the step, which is why a filtered
 * element does not move current.pos. accept() is called on $this, so a user
 * subclass's body is what decides.
 */
static sxi32 DualAccept(ph7_vm *pVm,ph7_class_instance *pThis,int *pbAccept)
{
	ph7_class_method *pMethod = PH7_ClassExtractMethod(pThis->pClass,"accept",sizeof("accept")-1);
	ph7_value sRes;
	sxi32 rc;
	*pbAccept = 0;
	if( pMethod == 0 ){
		return SXRET_OK;
	}
	PH7_MemObjInit(pVm,&sRes);
	rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sRes,0,0);
	if( rc == SXRET_OK ){
		PH7_MemObjToBool(&sRes);
		*pbAccept = sRes.x.iVal != 0;
	}
	PH7_MemObjRelease(&sRes);
	return rc;
}
static sxi32 DualFilterFetch(ph7_vm *pVm,ph7_class_instance *pThis)
{
	for(;;){
		int bAccept = 0;
		sxi32 rc = DualFetch(pVm,pThis,TRUE);
		if( rc != SXRET_OK ){
			return rc;
		}
		if( !DualFilled(pThis) ){
			break;
		}
		rc = DualAccept(pVm,pThis,&bAccept);
		if( rc != SXRET_OK ){
			return rc;
		}
		if( bAccept ){
			return SXRET_OK;
		}
		rc = DualCall(pVm,pThis,"next",sizeof("next")-1,0);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	DualFree(pVm,pThis);
	return SXRET_OK;
}
static int vm_builtin_FilterIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi32 rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 || DualDriver(pThis) == 0 ){
		return DualNotReady(pCtx);
	}
	rc = DualRewindInner(pVm,pThis);
	if( rc != SXRET_OK ){
		return rc;
	}
	return DualFilterFetch(pVm,pThis);
}
static int vm_builtin_FilterIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi32 rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 || DualDriver(pThis) == 0 ){
		return DualNotReady(pCtx);
	}
	rc = DualNextInner(pVm,pThis);
	if( rc != SXRET_OK ){
		return rc;
	}
	return DualFilterFetch(pVm,pThis);
}
/* CallbackFilterIterator::accept(): the callback sees the CACHED pair and the inner
 * iterator, and an empty cache is refused without calling it at all. */
static int vm_builtin_CallbackFilterIterator_accept(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *apCall[3];
	ph7_value sInner,sRes,*pCb;
	sxi32 rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 || DualDriver(pThis) == 0 ){
		return DualNotReady(pCtx);
	}
	if( !DualFilled(pThis) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pCb = PH7_NativeAttr(pThis,IT_CB);
	if( pCb == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	PH7_MemObjInit(pVm,&sInner);
	sInner.x.pOther = PH7_NativeAttrObj(pThis,IT_IN);
	if( sInner.x.pOther ){
		MemObjSetType(&sInner,MEMOBJ_OBJ);
		((ph7_class_instance *)sInner.x.pOther)->iRef++;
	}
	apCall[0] = PH7_NativeAttr(pThis,IT_CD);
	apCall[1] = PH7_NativeAttr(pThis,IT_CK);
	apCall[2] = &sInner;
	PH7_MemObjInit(pVm,&sRes);
	rc = PH7_VmCallUserFunction(pVm,pCb,3,apCall,&sRes);
	PH7_MemObjRelease(&sInner);
	if( rc == SXRET_OK ){
		ph7_result_value(pCtx,&sRes);
	}
	PH7_MemObjRelease(&sRes);
	return rc == SXRET_OK ? PH7_OK : rc;
}
/*
 * LimitIterator. The window is (offset, count) over the inner iterator's own
 * positions, and `__cp` counts them: php's valid() is "inside the window AND the
 * cache is filled", and next() only refills while the window still has room.
 */
static sxi32 DualLimitSeek(ph7_context *pCtx,ph7_class_instance *pThis,sxi64 iPos)
{
	ph7_vm *pVm = pCtx->pVm;
	sxi64 iOff = PH7_NativeAttrInt(pThis,IT_OFF);
	sxi64 iLim = PH7_NativeAttrInt(pThis,IT_LIM);
	ph7_class_instance *pIn = DualDriver(pThis);
	ph7_class *pSeekCls;
	sxi32 rc;
	int bValid;
	DualFree(pVm,pThis);
	if( iPos < iOff ){
		return PH7_VmThrowException(pCtx,"OutOfBoundsException",
			"Cannot seek to %qd which is below the offset %qd",iPos,iOff);
	}
	if( iLim != -1 && (iPos - iOff) >= iLim ){
		return PH7_VmThrowException(pCtx,"OutOfBoundsException",
			"Cannot seek to %qd which is behind offset %qd plus count %qd",iPos,iOff,iLim);
	}
	pSeekCls = PH7_VmExtractClass(pVm,"SeekableIterator",sizeof("SeekableIterator")-1,FALSE,0);
	if( iPos != PH7_NativeAttrInt(pThis,IT_CP) && pIn && pSeekCls
	 && PH7_VmInstanceOf(pIn->pClass,pSeekCls) ){
		/* The inner knows how to jump: hand it the ABSOLUTE position and let its own
		 * refusal (ArrayIterator's "Seek position N is out of range") surface. */
		ph7_class_method *pMethod = PH7_ClassExtractMethod(pIn->pClass,"seek",sizeof("seek")-1);
		ph7_value sPos,*apArg[1];
		PH7_MemObjInitFromInt(pVm,&sPos,iPos);
		apArg[0] = &sPos;
		rc = pMethod ? PH7_VmCallClassMethod(pVm,pIn,pMethod,0,1,apArg) : SXRET_OK;
		PH7_MemObjRelease(&sPos);
		if( rc != SXRET_OK ){
			return rc;
		}
		PH7_NativeSetAttrInt(pVm,pThis,IT_CP,iPos);
		rc = DualInnerValid(pVm,pThis,&bValid);
		if( rc != SXRET_OK ){
			return rc;
		}
		if( bValid ){
			return DualFetch(pVm,pThis,FALSE);
		}
		return SXRET_OK;
	}
	/* Otherwise emulate: a backward seek is a rewind followed by next() calls. */
	if( iPos < PH7_NativeAttrInt(pThis,IT_CP) ){
		rc = DualRewindInner(pVm,pThis);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	for(;;){
		if( iPos <= PH7_NativeAttrInt(pThis,IT_CP) ){
			break;
		}
		rc = DualInnerValid(pVm,pThis,&bValid);
		if( rc != SXRET_OK ){
			return rc;
		}
		if( !bValid ){
			break;
		}
		rc = DualNextInner(pVm,pThis);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	rc = DualInnerValid(pVm,pThis,&bValid);
	if( rc != SXRET_OK ){
		return rc;
	}
	if( bValid ){
		return DualFetch(pVm,pThis,TRUE);
	}
	return SXRET_OK;
}
static int vm_builtin_LimitIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis;
	sxi64 iOff = 0,iLim = -1;
	sxi32 rc;
	/* php screens the two bounds BEFORE it remembers the iterator, so a refused
	 * LimitIterator can still be constructed again. PH7_IntArgResolve is the shared
	 * `int` ZPP: the central signature screen does not cover a non-numeric STRING
	 * against an int parameter, and every builtin that takes one calls this. */
	if( nArg > 1 ){
		rc = PH7_IntArgResolve(pCtx,apArg[1],ph7_function_name(pCtx),2,"$offset","int",&iOff);
		if( rc != PH7_OK ){
			return rc;
		}
		if( iOff < 0 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #2 ($offset) must be greater than or equal to 0",
				ph7_function_name(pCtx));
		}
	}
	if( nArg > 2 ){
		rc = PH7_IntArgResolve(pCtx,apArg[2],ph7_function_name(pCtx),3,"$limit","int",&iLim);
		if( rc != PH7_OK ){
			return rc;
		}
		if( iLim < -1 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #3 ($limit) must be greater than or equal to -1",
				ph7_function_name(pCtx));
		}
	}
	rc = DualConstruct(pCtx,"LimitIterator",nArg,apArg);
	if( rc != PH7_OK ){
		return rc;
	}
	pThis = PH7_ContextThis(pCtx);
	if( pThis ){
		PH7_NativeSetAttrInt(pVm,pThis,IT_OFF,iOff);
		PH7_NativeSetAttrInt(pVm,pThis,IT_LIM,iLim);
	}
	return PH7_OK;
}
static int vm_builtin_LimitIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi32 rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 || DualDriver(pThis) == 0 ){
		return DualNotReady(pCtx);
	}
	rc = DualRewindInner(pVm,pThis);
	if( rc != SXRET_OK ){
		return rc;
	}
	return DualLimitSeek(pCtx,pThis,PH7_NativeAttrInt(pThis,IT_OFF));
}
static int vm_builtin_LimitIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi64 iLim;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 || DualDriver(pThis) == 0 ){
		return DualNotReady(pCtx);
	}
	iLim = PH7_NativeAttrInt(pThis,IT_LIM);
	ph7_result_bool(pCtx,
		(iLim == -1
		 || (PH7_NativeAttrInt(pThis,IT_CP) - PH7_NativeAttrInt(pThis,IT_OFF)) < iLim)
		&& DualFilled(pThis));
	return PH7_OK;
}
static int vm_builtin_LimitIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi64 iLim;
	sxi32 rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 || DualDriver(pThis) == 0 ){
		return DualNotReady(pCtx);
	}
	rc = DualNextInner(pVm,pThis);
	if( rc != SXRET_OK ){
		return rc;
	}
	iLim = PH7_NativeAttrInt(pThis,IT_LIM);
	if( iLim == -1
	 || (PH7_NativeAttrInt(pThis,IT_CP) - PH7_NativeAttrInt(pThis,IT_OFF)) < iLim ){
		return DualFetch(pVm,pThis,TRUE);
	}
	return PH7_OK;   /* past the window: the cache stays empty, so current() is null */
}
static int vm_builtin_LimitIterator_seek(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi64 iPos = 0;
	sxi32 rc;
	if( pThis == 0 || DualDriver(pThis) == 0 ){
		return DualNotReady(pCtx);
	}
	if( nArg > 0 ){
		rc = PH7_IntArgResolve(pCtx,apArg[0],ph7_function_name(pCtx),1,"$offset","int",&iPos);
		if( rc != PH7_OK ){
			return rc;
		}
	}
	rc = DualLimitSeek(pCtx,pThis,iPos);
	if( rc != PH7_OK ){
		return rc;
	}
	ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,IT_CP));
	return PH7_OK;
}
static int vm_builtin_LimitIterator_getPosition(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 || DualDriver(pThis) == 0 ){
		return DualNotReady(pCtx);
	}
	ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,IT_CP));
	return PH7_OK;
}
/* InfiniteIterator::next(): step, and on exhaustion rewind and step into the head
 * again. Both refills are php's check_more=0 form — the validity was just tested. */
static int vm_builtin_InfiniteIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi32 rc;
	int bValid;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 || DualDriver(pThis) == 0 ){
		return DualNotReady(pCtx);
	}
	rc = DualNextInner(pVm,pThis);
	if( rc != SXRET_OK ){
		return rc;
	}
	rc = DualInnerValid(pVm,pThis,&bValid);
	if( rc != SXRET_OK ){
		return rc;
	}
	if( !bValid ){
		rc = DualRewindInner(pVm,pThis);
		if( rc != SXRET_OK ){
			return rc;
		}
		rc = DualInnerValid(pVm,pThis,&bValid);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	if( bValid ){
		return DualFetch(pVm,pThis,FALSE);
	}
	return PH7_OK;
}
/*
 * NoRewindIterator. Its rewind() does nothing at all — and because the four
 * accessors read the INNER live rather than the cache, an instance is usable
 * without ever being rewound, which is the entire point of the class.
 */
static int vm_builtin_NoRewindIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( PH7_ContextThis(pCtx) == 0 || DualDriver(PH7_ContextThis(pCtx)) == 0 ){
		return DualNotReady(pCtx);
	}
	return PH7_OK;
}
static int vm_builtin_NoRewindIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi32 rc;
	int bValid;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 || DualDriver(pThis) == 0 ){
		return DualNotReady(pCtx);
	}
	rc = DualInnerValid(pCtx->pVm,pThis,&bValid);
	if( rc != SXRET_OK ){
		return rc;
	}
	ph7_result_bool(pCtx,bValid);
	return PH7_OK;
}
static int DualForwardLive(ph7_context *pCtx,const char *zName,sxu32 nLen,int bResult)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value sVal;
	sxi32 rc;
	if( pThis == 0 || DualDriver(pThis) == 0 ){
		return DualNotReady(pCtx);
	}
	PH7_MemObjInit(pCtx->pVm,&sVal);
	rc = DualCall(pCtx->pVm,pThis,zName,nLen,bResult ? &sVal : 0);
	if( rc == SXRET_OK && bResult ){
		ph7_result_value(pCtx,&sVal);
	}
	PH7_MemObjRelease(&sVal);
	return rc == SXRET_OK ? PH7_OK : rc;
}
static int vm_builtin_NoRewindIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return DualForwardLive(pCtx,"current",sizeof("current")-1,TRUE);
}
static int vm_builtin_NoRewindIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return DualForwardLive(pCtx,"key",sizeof("key")-1,TRUE);
}
static int vm_builtin_NoRewindIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return DualForwardLive(pCtx,"next",sizeof("next")-1,FALSE);
}
/* EmptyIterator: valid() is false forever, and asking for a value or a key is a
 * BadMethodCallException rather than a null. */
static int vm_builtin_EmptyIterator_nop(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_null(pCtx);
	return PH7_OK;
}
static int vm_builtin_EmptyIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
static int vm_builtin_EmptyIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return PH7_VmThrowException(pCtx,"BadMethodCallException",
		"Accessing the value of an EmptyIterator");
}
static int vm_builtin_EmptyIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return PH7_VmThrowException(pCtx,"BadMethodCallException",
		"Accessing the key of an EmptyIterator");
}
/*
 * ---------------------------------------------------------------------------
 * RegexIterator: a FilterIterator whose accept() runs a regex over the CACHE.
 *
 * Everything that matters here follows from the cache the decorators already
 * keep. php's accept() reads `current.data` (or `current.key` under USE_KEY) and
 * -- in every mode but MATCH -- WRITES THE RESULT BACK INTO THAT SAME SLOT, which
 * is why the class declares no current() of its own: the inherited one already
 * answers the transformed value. The PHP chunk kept a private `$__cur` and
 * overrode current(), and that is where its two wrong answers came from: a
 * REPLACE under USE_KEY must replace into the KEY (php leaves current() alone),
 * and an ARRAY current() is refused outright rather than matched as the string
 * "Array".
 */
#define IT_RE  "__re"   /* php's u.regex.regex: the pattern, as given */
#define IT_RM  "__rm"   /* php's u.regex.mode */
#define IT_RF  "__rf"   /* php's u.regex.flags (USE_KEY / INVERT_MATCH) */
#define IT_RP  "__rp"   /* php's u.regex.preg_flags */
#define REGIT_USE_KEY  1
#define REGIT_INVERTED 2
/* php's ValueError for a mode outside the five. The constructor and setMode()
 * word it identically and differ only in the argument they name. */
static int RegitBadMode(ph7_context *pCtx,const char *zWhere)
{
	return PH7_VmThrowException(pCtx,"ValueError",
		"%s must be RegexIterator::MATCH, RegexIterator::GET_MATCH, "
		"RegexIterator::ALL_MATCHES, RegexIterator::SPLIT, or RegexIterator::REPLACE",
		zWhere);
}
static int vm_builtin_RegexIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zPat;
	int nPat;
	sxi64 iMode = PH7_REGIT_MATCH;
	char zErr[288];
	sxi32 rc;
	if( pThis == 0 ){
		return PH7_OK;
	}
	if( PH7_NativeAttrObj(pThis,IT_IN) != 0 ){
		/* php makes the "already built" refusal before it reads any argument, so
		 * hand this straight to the shared constructor, which words it. */
		return DualConstruct(pCtx,"RegexIterator",nArg,apArg);
	}
	if( nArg < 2 ){
		return PH7_OK;   /* the arity screen already refused */
	}
	if( nArg > 2 ){
		iMode = ph7_value_to_int(apArg[2]);
	}
	if( iMode < PH7_REGIT_MATCH || iMode > PH7_REGIT_REPLACE ){
		return RegitBadMode(pCtx,"RegexIterator::__construct(): Argument #3 ($mode)");
	}
	/* php compiles the pattern HERE and promotes pcre's warning to an
	 * InvalidArgumentException, so a bad pattern is refused by `new` rather than
	 * warning once per element from accept(). */
	zPat = ph7_value_to_string(apArg[1],&nPat);
	if( !PH7_PcrePatternCheck(pVm,zPat,nPat,zErr,sizeof(zErr)) ){
		return PH7_VmThrowException(pCtx,"InvalidArgumentException",
			"RegexIterator::__construct(): %s",zErr);
	}
	rc = DualConstruct(pCtx,"RegexIterator",nArg,apArg);
	if( rc != PH7_OK ){
		return rc;
	}
	PH7_NativeSetAttrStr(pVm,pThis,IT_RE,zPat,(sxu32)nPat);
	PH7_NativeSetAttrInt(pVm,pThis,IT_RM,iMode);
	PH7_NativeSetAttrInt(pVm,pThis,IT_RF,nArg > 3 ? ph7_value_to_int(apArg[3]) : 0);
	PH7_NativeSetAttrInt(pVm,pThis,IT_RP,nArg > 4 ? ph7_value_to_int(apArg[4]) : 0);
	return PH7_OK;
}
static int vm_builtin_RegexIterator_accept(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value sSubject,sPattern,sRepl,sOut,*pSlot;
	int iMode,iFlags,bUseKey,bOk = 0;
	sxi32 rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 || DualDriver(pThis) == 0 ){
		return DualNotReady(pCtx);
	}
	if( !DualFilled(pThis) ){
		/* Nothing has been fetched: php answers false without touching the regex. */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iMode = (int)PH7_NativeAttrInt(pThis,IT_RM);
	iFlags = (int)PH7_NativeAttrInt(pThis,IT_RF);
	bUseKey = (iFlags & REGIT_USE_KEY) != 0;
	pSlot = PH7_NativeAttr(pThis,bUseKey ? IT_CK : IT_CD);
	if( pSlot == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( !bUseKey && (pSlot->iFlags & MEMOBJ_HASHMAP) ){
		/* php's `Z_TYPE(current.data) == IS_ARRAY -> RETURN_FALSE`, ahead of every
		 * mode. The chunk's (string)$subject matched the word "Array" instead. */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Take the subject as a VALUE: the slot pointer does not survive a call into
	 * user code, and an object subject reaches __toString() below. */
	PH7_MemObjInit(pVm,&sSubject);
	PH7_MemObjStore(pSlot,&sSubject);
	rc = PH7_MemObjToStringUV(&sSubject);
	if( rc != SXRET_OK ){
		PH7_MemObjRelease(&sSubject);
		return rc;
	}
	PH7_MemObjInit(pVm,&sPattern);
	PH7_MemObjInit(pVm,&sRepl);
	PH7_MemObjInit(pVm,&sOut);
	{
		ph7_value *pRe = PH7_NativeAttr(pThis,IT_RE);
		if( pRe ){
			PH7_MemObjStore(pRe,&sPattern);
		}
	}
	if( iMode == PH7_REGIT_REPLACE ){
		/* php reads the public $replacement property, whose declared ?string makes
		 * the read total: a null answers the empty string. */
		ph7_value *pRepl = PH7_NativeAttr(pThis,"replacement");
		if( pRepl ){
			PH7_MemObjStore(pRepl,&sRepl);
		}
		PH7_MemObjToString(&sRepl);
	}
	rc = PH7_PcreRegitApply(pCtx,iMode,&sPattern,&sSubject,
		(int)PH7_NativeAttrInt(pThis,IT_RP),&sRepl,&sOut,&bOk);
	if( rc == PH7_OK && iMode != PH7_REGIT_MATCH ){
		/* php writes the transformed value over the cached pair -- into the KEY when
		 * a REPLACE is keyed, into current() otherwise -- so the inherited current()
		 * and key() present it. */
		DualSetSlot(pVm,pThis,(iMode == PH7_REGIT_REPLACE && bUseKey) ? IT_CK : IT_CD,&sOut);
	}
	PH7_MemObjRelease(&sSubject);
	PH7_MemObjRelease(&sPattern);
	PH7_MemObjRelease(&sRepl);
	PH7_MemObjRelease(&sOut);
	if( rc != PH7_OK ){
		return rc;
	}
	ph7_result_bool(pCtx,(iFlags & REGIT_INVERTED) ? !bOk : bOk);
	return PH7_OK;
}
static int vm_builtin_RegexIterator_getRegex(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pRe;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 || DualDriver(pThis) == 0 ){
		return DualNotReady(pCtx);
	}
	pRe = PH7_NativeAttr(pThis,IT_RE);
	if( pRe ){
		ph7_result_value(pCtx,pRe);
	}
	return PH7_OK;
}
/* The three getters and the three setters are one pair per slot; only setMode()
 * screens its value, which is php's own asymmetry (setFlags/setPregFlags take
 * any integer). */
static int RegitGet(ph7_context *pCtx,const char *zSlot)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	if( pThis == 0 || DualDriver(pThis) == 0 ){
		return DualNotReady(pCtx);
	}
	ph7_result_int64(pCtx,PH7_NativeAttrInt(pThis,zSlot));
	return PH7_OK;
}
static int RegitSet(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zSlot)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	if( pThis == 0 || DualDriver(pThis) == 0 ){
		return DualNotReady(pCtx);
	}
	if( nArg > 0 ){
		PH7_NativeSetAttrInt(pCtx->pVm,pThis,zSlot,ph7_value_to_int64(apArg[0]));
	}
	return PH7_OK;
}
static int vm_builtin_RegexIterator_getMode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return RegitGet(pCtx,IT_RM);
}
static int vm_builtin_RegexIterator_setMode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg > 0 ){
		sxi64 iMode = ph7_value_to_int64(apArg[0]);
		if( iMode < PH7_REGIT_MATCH || iMode > PH7_REGIT_REPLACE ){
			/* php screens the VALUE before it even fetches the object. */
			return RegitBadMode(pCtx,"RegexIterator::setMode(): Argument #1 ($mode)");
		}
	}
	return RegitSet(pCtx,nArg,apArg,IT_RM);
}
static int vm_builtin_RegexIterator_getFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return RegitGet(pCtx,IT_RF);
}
static int vm_builtin_RegexIterator_setFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return RegitSet(pCtx,nArg,apArg,IT_RF);
}
static int vm_builtin_RegexIterator_getPregFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return RegitGet(pCtx,IT_RP);
}
static int vm_builtin_RegexIterator_setPregFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return RegitSet(pCtx,nArg,apArg,IT_RP);
}
/*
 * ---------------------------------------------------------------------------
 * AppendIterator: an IteratorIterator whose inner iterator is whatever entry a
 * real ArrayIterator is currently pointing at.
 *
 * php keeps the appended iterators in an actual `ArrayIterator` INSTANCE
 * (`u.append.zarrayit`, the object getArrayIterator() hands out) and walks it with
 * a cursor over the SAME storage (`u.append.iterator`). Both halves are
 * php-visible and the chunk had neither: it kept a private PHP array and answered
 * getArrayIterator() with a fresh ArrayIterator over a COPY, so appending through
 * the returned object iterated nothing and `$ai->rewind()` did not restart the
 * walk. The list cursor here is that one ArrayIterator's own `pCur`, driven
 * directly the way php drives its iterator funcs -- not through the class's
 * methods, which php does not call either.
 */
static ph7_class_instance * ApList(ph7_class_instance *pThis)
{
	return pThis ? PH7_NativeAttrObj(pThis,AP_LIST) : 0;
}
static ph7_hashmap * ApMap(ph7_vm *pVm,ph7_class_instance *pThis)
{
	return SplStore(pVm,ApList(pThis));
}
/* The iterator the list cursor points at, or 0 past the end. */
static ph7_class_instance * ApCurrent(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_hashmap *pMap = ApMap(pVm,pThis);
	ph7_value *pVal = (pMap && pMap->pCur) ? HashmapExtractNodeValue(pMap->pCur) : 0;
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_OBJ) == 0 ){
		return 0;
	}
	return (ph7_class_instance *)pVal->x.pOther;
}
static void ApListRewind(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_hashmap *pMap = ApMap(pVm,pThis);
	if( pMap ){
		pMap->pCur = pMap->pFirst;
	}
}
static void ApListNext(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_hashmap *pMap = ApMap(pVm,pThis);
	if( pMap && pMap->pCur ){
		pMap->pCur = pMap->pCur->pPrev;   /* insertion order: pFirst, then the pPrev chain */
	}
}
/*
 * php's spl_append_it_next_iterator: drop the cache and the current inner, then
 * adopt whatever the list cursor points at (rewound). *pbOk is php's SUCCESS --
 * false means the list is exhausted and this iterator has nothing left.
 */
static sxi32 ApAdoptCurrent(ph7_vm *pVm,ph7_class_instance *pThis,int *pbOk)
{
	ph7_class_instance *pIt;
	*pbOk = 0;
	DualFree(pVm,pThis);
	PH7_NativeSetAttrObj(pVm,pThis,IT_IN,0);
	PH7_NativeSetAttrObj(pVm,pThis,IT_IT,0);
	pIt = ApCurrent(pVm,pThis);
	if( pIt == 0 ){
		return SXRET_OK;
	}
	PH7_NativeSetAttrObj(pVm,pThis,IT_IN,pIt);
	PH7_NativeSetAttrObj(pVm,pThis,IT_IT,pIt);
	*pbOk = 1;
	return DualRewindInner(pVm,pThis);
}
/*
 * php's spl_append_it_fetch: step over every exhausted inner iterator, then fill
 * the cache without re-asking valid() (php's check_more = 0 -- the loop above just
 * established it).
 */
static sxi32 ApFetch(ph7_vm *pVm,ph7_class_instance *pThis)
{
	for(;;){
		int bValid = 0, bOk = 0;
		sxi32 rc = DualInnerValid(pVm,pThis,&bValid);
		if( rc != SXRET_OK ){
			return rc;
		}
		if( bValid ){
			break;
		}
		ApListNext(pVm,pThis);
		rc = ApAdoptCurrent(pVm,pThis,&bOk);
		if( rc != SXRET_OK ){
			return rc;
		}
		if( !bOk ){
			return SXRET_OK;   /* nothing left: the cache stays empty and valid() is false */
		}
	}
	return DualFetch(pVm,pThis,FALSE);
}
static int vm_builtin_AppendIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pList;
	ph7_class *pClass;
	ph7_class_method *pCons;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 ){
		return PH7_OK;
	}
	if( ApList(pThis) != 0 ){
		/* php's "already built" refusal, worded from the DECLARING class as everywhere
		 * else in the family. */
		return PH7_VmThrowException(pCtx,"BadMethodCallException",
			"AppendIterator::getIterator() must be called exactly once per instance");
	}
	pClass = PH7_VmExtractClass(pVm,"ArrayIterator",sizeof("ArrayIterator")-1,FALSE,0);
	if( pClass == 0 ){
		return PH7_OK;
	}
	pList = PH7_NewClassInstance(pVm,pClass);
	if( pList == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	pList->iRef++;
	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);
	if( pCons ){
		PH7_VmCallClassMethod(pVm,pList,pCons,0,0,0);
	}
	PH7_NativeSetAttrObj(pVm,pThis,AP_LIST,pList);   /* the slot takes its own reference */
	PH7_ClassInstanceUnref(pList);
	return PH7_OK;
}
/*
 * append(). php's own sequence, and every branch of it is observable:
 *   - a list cursor sitting on a LIVE entry whose cache is empty means the walk has
 *     consumed that entry, so the new iterator goes in behind it and the cursor steps
 *     over;
 *   - if nothing is being iterated yet (or the cache is empty), the cursor is walked
 *     forward until it reaches the iterator just appended, and the fetch resumes there.
 * That second half is what makes an AppendIterator RESUME after exhaustion, and it
 * relies on ArrayIterator::append() reviving a cursor that ran off the end (see
 * SplStoreInsert).
 */
static int vm_builtin_AppendIterator_append(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pIt;
	ph7_hashmap *pMap;
	int bListValid,bInnerValid = 0,nGuard;
	sxi32 rc;
	if( !DualReady(pThis) ){
		return DualNotReady(pCtx);
	}
	if( nArg < 1 || (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){
		return PH7_OK;   /* the shared ZPP screen already refused a non-Iterator */
	}
	pIt = (ph7_class_instance *)apArg[0]->x.pOther;
	pMap = ApMap(pVm,pThis);
	bListValid = pMap && pMap->pCur;
	/* php's spl_dual_it_valid, both times it appears below: the INNER iterator's
	 * valid() (false when there is no inner at all), NOT the cache. */
	rc = DualInnerValid(pVm,pThis,&bInnerValid);
	if( rc != SXRET_OK ){
		return rc;
	}
	pMap = ApMap(pVm,pThis);   /* that call ran user code: re-resolve */
	if( pMap ){
		SplStoreInsert(pMap,0,apArg[0]);
	}
	if( bListValid && !bInnerValid ){
		ApListNext(pVm,pThis);
	}
	if( PH7_NativeAttrObj(pThis,IT_IT) != 0 && bInnerValid ){
		return PH7_OK;   /* mid-walk with a live element: the new tail waits its turn */
	}
	pMap = ApMap(pVm,pThis);
	if( pMap && pMap->pCur == 0 ){
		ApListRewind(pVm,pThis);
	}
	for( nGuard = 0 ; ; ++nGuard ){
		int bOk = 0;
		rc = ApAdoptCurrent(pVm,pThis,&bOk);
		if( rc != SXRET_OK ){
			return rc;
		}
		if( !bOk || PH7_NativeAttrObj(pThis,IT_IN) == pIt ){
			break;
		}
		ApListNext(pVm,pThis);
		if( nGuard > 100000 ){
			break;   /* php's loop has no bound; ours refuses to spin on a mutated list */
		}
	}
	rc = ApFetch(pVm,pThis);
	return rc == SXRET_OK ? PH7_OK : rc;
}
static int vm_builtin_AppendIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi32 rc;
	int bOk = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !DualReady(pThis) ){
		return DualNotReady(pCtx);
	}
	ApListRewind(pVm,pThis);
	rc = ApAdoptCurrent(pVm,pThis,&bOk);
	if( rc == SXRET_OK && bOk ){
		rc = ApFetch(pVm,pThis);
	}
	return rc == SXRET_OK ? PH7_OK : rc;
}
static int vm_builtin_AppendIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi32 rc;
	int bValid = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !DualReady(pThis) ){
		return DualNotReady(pCtx);
	}
	rc = DualInnerValid(pVm,pThis,&bValid);
	if( rc != SXRET_OK ){
		return rc;
	}
	if( bValid ){
		rc = DualNextInner(pVm,pThis);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	rc = ApFetch(pVm,pThis);
	return rc == SXRET_OK ? PH7_OK : rc;
}
/* php re-fetches here (spl_dual_it_fetch with check_more), which is why an
 * AppendIterator FOLLOWS an inner iterator moved behind its back where every other
 * decorator answers its cache. */
static int vm_builtin_AppendIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi32 rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !DualReady(pThis) ){
		return DualNotReady(pCtx);
	}
	rc = DualFetch(pCtx->pVm,pThis,TRUE);
	if( rc != SXRET_OK ){
		return rc;
	}
	return DualResultSlot(pCtx,IT_CD);
}
/* The list cursor's KEY, which is php's index into the appended iterators -- and
 * NULL once the cursor has run off the end, where the chunk kept answering the last
 * index it had seen. */
static int vm_builtin_AppendIterator_getIteratorIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pSlot,*apCall[1];
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !DualReady(pThis) ){
		return DualNotReady(pCtx);
	}
	pSlot = SplStoreSlot(pCtx->pVm,ApList(pThis));
	if( pSlot == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	apCall[0] = pSlot;
	return ph7_hashmap_simple_key(pCtx,1,apCall);
}
/*
 * ---------------------------------------------------------------------------
 * The RECURSIVE pair: RecursiveArrayIterator (an ArrayIterator that descends into
 * its own entries) and RecursiveFilterIterator (a FilterIterator that forwards the
 * two recursion methods to its inner iterator).
 *
 * RecursiveArrayIterator is where php's CHILD_ARRAYS_ONLY flag lives, and the
 * chunk's two-line `is_array($c) || is_object($c)` / `new $c($this->current())`
 * ignored it in both directions: an OBJECT entry claimed children under a flag that
 * exists to say it has none, and the child iterator was built WITHOUT the parent's
 * flags, so the restriction lasted exactly one level. php also answers null rather
 * than descending when there is no current element, and hands back an entry that is
 * ALREADY an instance of the called class instead of wrapping it again.
 */
#define RAI_CHILD_ARRAYS_ONLY 4
/* The entry the store cursor is on, or 0 past the end (php's
 * zend_hash_get_current_data_ex, which every one of these four bodies starts with). */
static ph7_value * RaiCurrentEntry(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_hashmap *pMap = SplStore(pVm,pThis);
	return (pMap && pMap->pCur) ? HashmapExtractNodeValue(pMap->pCur) : 0;
}
static int vm_builtin_RecursiveArrayIterator_hasChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pEntry = RaiCurrentEntry(pCtx->pVm,pThis);
	int bHas = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pEntry ){
		if( pEntry->iFlags & MEMOBJ_HASHMAP ){
			bHas = 1;
		}else if( pEntry->iFlags & MEMOBJ_OBJ ){
			/* php: an object is a child UNLESS the iterator was told arrays only. */
			bHas = (PH7_NativeAttrInt(pThis,SPL_F) & RAI_CHILD_ARRAYS_ONLY) == 0;
		}
	}
	ph7_result_bool(pCtx,bHas);
	return PH7_OK;
}
static int vm_builtin_RecursiveArrayIterator_getChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pEntry = RaiCurrentEntry(pVm,pThis);
	ph7_class_instance *pChild;
	ph7_class_method *pCons;
	ph7_value sEntry,sFlags,*apCtor[2];
	sxi64 iFlags;
	sxi32 rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 || pEntry == 0 ){
		ph7_result_null(pCtx);   /* php descends into nothing when nothing is current */
		return PH7_OK;
	}
	iFlags = PH7_NativeAttrInt(pThis,SPL_F);
	if( pEntry->iFlags & MEMOBJ_OBJ ){
		ph7_class_instance *pObj = (ph7_class_instance *)pEntry->x.pOther;
		if( iFlags & RAI_CHILD_ARRAYS_ONLY ){
			ph7_result_null(pCtx);
			return PH7_OK;
		}
		if( pObj && PH7_VmInstanceOf(pObj->pClass,pThis->pClass) ){
			/* Already one of us: php hands the entry back rather than wrapping it. */
			SplResultBorrowed(pCtx,pObj);
			return PH7_OK;
		}
	}
	/* php's spl_instantiate_child_arg: the CALLED class, constructed with the entry
	 * AND the parent's flags -- which is what carries CHILD_ARRAYS_ONLY down. */
	pChild = PH7_NewClassInstance(pVm,pThis->pClass);
	if( pChild == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	pChild->iRef++;
	PH7_MemObjInit(pVm,&sEntry);
	PH7_MemObjStore(pEntry,&sEntry);
	PH7_MemObjInitFromInt(pVm,&sFlags,iFlags);
	apCtor[0] = &sEntry;
	apCtor[1] = &sFlags;
	pCons = PH7_ClassExtractMethod(pThis->pClass,"__construct",sizeof("__construct")-1);
	rc = pCons ? PH7_VmCallClassMethod(pVm,pChild,pCons,0,2,apCtor) : SXRET_OK;
	PH7_MemObjRelease(&sEntry);
	PH7_MemObjRelease(&sFlags);
	if( rc != SXRET_OK ){
		PH7_ClassInstanceUnref(pChild);
		return rc;
	}
	PH7_NativeResultObject(pCtx,pChild);
	PH7_ClassInstanceUnref(pChild);
	return PH7_OK;
}
/* RecursiveFilterIterator forwards both methods to the object getInnerIterator()
 * answers (php calls on inner.zobject), and wraps the children in ITS OWN class. */
static int vm_builtin_RecursiveFilterIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return DualConstruct(pCtx,"RecursiveFilterIterator",nArg,apArg);
}
static sxi32 RfiCallInner(ph7_context *pCtx,const char *zName,sxu32 nName,ph7_value *pOut)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pIn = pThis ? PH7_NativeAttrObj(pThis,IT_IN) : 0;
	ph7_class_method *pMethod = pIn ? PH7_ClassExtractMethod(pIn->pClass,zName,nName) : 0;
	if( pMethod == 0 ){
		return SXRET_OK;
	}
	return PH7_VmCallClassMethod(pCtx->pVm,pIn,pMethod,pOut,0,0);
}
static int vm_builtin_RecursiveFilterIterator_hasChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value sRes;
	sxi32 rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !DualReady(pThis) ){
		return DualNotReady(pCtx);
	}
	PH7_MemObjInit(pCtx->pVm,&sRes);
	rc = RfiCallInner(pCtx,"hasChildren",sizeof("hasChildren")-1,&sRes);
	if( rc == SXRET_OK ){
		ph7_result_value(pCtx,&sRes);
	}
	PH7_MemObjRelease(&sRes);
	return rc == SXRET_OK ? PH7_OK : rc;
}
static int vm_builtin_RecursiveFilterIterator_getChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pChild;
	ph7_class_method *pCons;
	ph7_value sInner,*apCtor[1];
	sxi32 rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !DualReady(pThis) ){
		return DualNotReady(pCtx);
	}
	PH7_MemObjInit(pVm,&sInner);
	rc = RfiCallInner(pCtx,"getChildren",sizeof("getChildren")-1,&sInner);
	if( rc != SXRET_OK ){
		PH7_MemObjRelease(&sInner);
		return rc;
	}
	pChild = PH7_NewClassInstance(pVm,pThis->pClass);
	if( pChild == 0 ){
		PH7_MemObjRelease(&sInner);
		return PH7_ContextMemoryError(pCtx);
	}
	pChild->iRef++;
	apCtor[0] = &sInner;
	pCons = PH7_ClassExtractMethod(pThis->pClass,"__construct",sizeof("__construct")-1);
	rc = pCons ? PH7_VmCallClassMethod(pVm,pChild,pCons,0,1,apCtor) : SXRET_OK;
	PH7_MemObjRelease(&sInner);
	if( rc != SXRET_OK ){
		PH7_ClassInstanceUnref(pChild);
		return rc;
	}
	PH7_NativeResultObject(pCtx,pChild);
	PH7_ClassInstanceUnref(pChild);
	return PH7_OK;
}
static int vm_builtin_AppendIterator_getArrayIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pList;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !DualReady(pThis) ){
		return DualNotReady(pCtx);
	}
	pList = ApList(pThis);
	if( pList ){
		SplResultBorrowed(pCtx,pList);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
/*
 * The declarations. php's method ORDER is the order Reflection reports, so each
 * table follows spl_iterators.stub.php line for line; the parameter types are the
 * stub's too, which is what makes `Iterator $iterator` refuse an IteratorAggregate
 * everywhere except IteratorIterator (the one class that declares Traversable and
 * unwraps).
 *
 * No RETURN type is declared, on purpose: php marks every one of these
 * `@tentative-return-type`, and a tentative type answers NULL from getReturnType()
 * and false from hasReturnType() — which is exactly what an undeclared zRet answers
 * here. Declaring them would print `Return [ bool ]` where php prints
 * `Tentative return [ bool ]` AND make getReturnType() disagree; leaving them off
 * costs only getTentativeReturnType(). PHL has no tentative-return concept at all
 * (§7.4) — DateTime and the reflectors already report a plain return type where php
 * reports a tentative one.
 */
static sxi32 VmInstallSplDualIterators(ph7_vm *pVm)
{
	static const PH7_NativePropDef aDualProp[] = {
		{ IT_IN, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ IT_IT, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ IT_CD, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ IT_CK, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ IT_CF, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
		{ IT_CP, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativePropDef aLimitProp[] = {
		{ IT_OFF, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
		{ IT_LIM, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, -1, 0, 0.0 }, 0 },
	};
	static const PH7_NativePropDef aCbProp[] = {
		{ IT_CB, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeMethodDef aOuterMethod[] = {
		{ "getInnerIterator", PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", 0, 0 },
	};
	static const PH7_NativeMethodDef aIterIterMethod[] = {
		{ "__construct",      PH7_MOD_PUBLIC, "Traversable $iterator, ?string $class = null", 0,
		  vm_builtin_IteratorIterator_construct },
		{ "getInnerIterator", PH7_MOD_PUBLIC, "", "@?Iterator", vm_builtin_Dual_getInnerIterator },
		{ "rewind",           PH7_MOD_PUBLIC, "", "@void", vm_builtin_IteratorIterator_rewind },
		{ "valid",            PH7_MOD_PUBLIC, "", "@bool", vm_builtin_Dual_valid },
		{ "key",              PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_Dual_key },
		{ "current",          PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_Dual_current },
		{ "next",             PH7_MOD_PUBLIC, "", "@void", vm_builtin_IteratorIterator_next },
	};
	static const PH7_NativeMethodDef aFilterMethod[] = {
		{ "accept",      PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "@bool", 0 },
		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator", 0, vm_builtin_FilterIterator_construct },
		{ "rewind",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_FilterIterator_rewind },
		{ "next",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_FilterIterator_next },
	};
	static const PH7_NativeMethodDef aCbFilterMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator, callable $callback", 0,
		  vm_builtin_CallbackFilterIterator_construct },
		{ "accept",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_CallbackFilterIterator_accept },
	};
	static const PH7_NativeMethodDef aLimitMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator, int $offset = 0, int $limit = -1", 0,
		  vm_builtin_LimitIterator_construct },
		{ "rewind",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_LimitIterator_rewind },
		{ "valid",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_LimitIterator_valid },
		{ "next",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_LimitIterator_next },
		{ "seek",        PH7_MOD_PUBLIC, "int $offset", "@int", vm_builtin_LimitIterator_seek },
		{ "getPosition", PH7_MOD_PUBLIC, "", "@int", vm_builtin_LimitIterator_getPosition },
	};
	static const PH7_NativeMethodDef aInfiniteMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator", 0,
		  vm_builtin_InfiniteIterator_construct },
		{ "next",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_InfiniteIterator_next },
	};
	static const PH7_NativeMethodDef aNoRewindMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator", 0,
		  vm_builtin_NoRewindIterator_construct },
		{ "rewind",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_NoRewindIterator_rewind },
		{ "valid",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_NoRewindIterator_valid },
		{ "key",         PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_NoRewindIterator_key },
		{ "current",     PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_NoRewindIterator_current },
		{ "next",        PH7_MOD_PUBLIC, "", "@void", vm_builtin_NoRewindIterator_next },
	};
	static const PH7_NativePropDef aRegexProp[] = {
		/* The one slot php PRESENTS, declared as php declares it: a ?string, so a
		 * `$it->replacement = 5` coerces and an array is a TypeError. */
		{ "replacement", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, "?string" },
		{ IT_RE, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },
		{ IT_RM, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
		{ IT_RF, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
		{ IT_RP, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeConstDef aRegexConst[] = {
		{ "USE_KEY",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, REGIT_USE_KEY,  0, 0.0 },
		{ "INVERT_MATCH", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, REGIT_INVERTED, 0, 0.0 },
		{ "MATCH",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_MATCH,       0, 0.0 },
		{ "GET_MATCH",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_GET_MATCH,   0, 0.0 },
		{ "ALL_MATCHES",  PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_ALL_MATCHES, 0, 0.0 },
		{ "SPLIT",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_SPLIT,       0, 0.0 },
		{ "REPLACE",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PH7_REGIT_REPLACE,     0, 0.0 },
	};
	static const PH7_NativeMethodDef aRegexMethod[] = {
		{ "__construct",  PH7_MOD_PUBLIC,
		  /* php's stub spells this default `RegexIterator::MATCH`, and one zSig field
		   * cannot say both the TEXT and the VALUE: the constant spelling prints php's
		   * export line but makes getDefaultValue() a "Failed to retrieve" throw, so the
		   * VALUE wins here, as it does in the aBuiltinSig rows with the same shape. */
		  "Iterator $iterator, string $pattern, int $mode = 0, int $flags = 0, int $pregFlags = 0", 0,
		  vm_builtin_RegexIterator_construct },
		{ "accept",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_RegexIterator_accept },
		{ "getMode",      PH7_MOD_PUBLIC, "", "@int", vm_builtin_RegexIterator_getMode },
		{ "setMode",      PH7_MOD_PUBLIC, "int $mode", "@void", vm_builtin_RegexIterator_setMode },
		{ "getFlags",     PH7_MOD_PUBLIC, "", "@int", vm_builtin_RegexIterator_getFlags },
		{ "setFlags",     PH7_MOD_PUBLIC, "int $flags", "@void", vm_builtin_RegexIterator_setFlags },
		{ "getRegex",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_RegexIterator_getRegex },
		{ "getPregFlags", PH7_MOD_PUBLIC, "", "@int", vm_builtin_RegexIterator_getPregFlags },
		{ "setPregFlags", PH7_MOD_PUBLIC, "int $pregFlags", "@void", vm_builtin_RegexIterator_setPregFlags },
	};
	static const PH7_NativeMethodDef aRecursiveMethod[] = {
		{ "hasChildren", PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", 0, 0 },
		{ "getChildren", PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", 0, 0 },
	};
	static const PH7_NativeConstDef aRaiConst[] = {
		{ "CHILD_ARRAYS_ONLY", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RAI_CHILD_ARRAYS_ONLY, 0, 0.0 },
	};
	static const PH7_NativeMethodDef aRaiMethod[] = {
		{ "hasChildren", PH7_MOD_PUBLIC, "", "@bool",
		  vm_builtin_RecursiveArrayIterator_hasChildren },
		{ "getChildren", PH7_MOD_PUBLIC, "", "@?RecursiveArrayIterator",
		  vm_builtin_RecursiveArrayIterator_getChildren },
	};
	static const PH7_NativeMethodDef aRfiMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "RecursiveIterator $iterator", 0,
		  vm_builtin_RecursiveFilterIterator_construct },
		{ "hasChildren", PH7_MOD_PUBLIC, "", "@bool",
		  vm_builtin_RecursiveFilterIterator_hasChildren },
		{ "getChildren", PH7_MOD_PUBLIC, "", "@?RecursiveFilterIterator",
		  vm_builtin_RecursiveFilterIterator_getChildren },
	};
	static const PH7_NativePropDef aAppendProp[] = {
		{ AP_LIST, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeMethodDef aAppendMethod[] = {
		{ "__construct",      PH7_MOD_PUBLIC, "", 0, vm_builtin_AppendIterator_construct },
		{ "append",           PH7_MOD_PUBLIC, "Iterator $iterator", "@void",
		  vm_builtin_AppendIterator_append },
		{ "rewind",           PH7_MOD_PUBLIC, "", "@void", vm_builtin_AppendIterator_rewind },
		{ "valid",            PH7_MOD_PUBLIC, "", "@bool", vm_builtin_Dual_valid },
		{ "current",          PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_AppendIterator_current },
		{ "next",             PH7_MOD_PUBLIC, "", "@void", vm_builtin_AppendIterator_next },
		{ "getIteratorIndex", PH7_MOD_PUBLIC, "", "@?int",
		  vm_builtin_AppendIterator_getIteratorIndex },
		{ "getArrayIterator", PH7_MOD_PUBLIC, "", "@ArrayIterator",
		  vm_builtin_AppendIterator_getArrayIterator },
	};
	static const PH7_NativeMethodDef aEmptyMethod[] = {
		{ "current", PH7_MOD_PUBLIC, "", "@never", vm_builtin_EmptyIterator_current },
		{ "next",    PH7_MOD_PUBLIC, "", "@void", vm_builtin_EmptyIterator_nop },
		{ "key",     PH7_MOD_PUBLIC, "", "@never", vm_builtin_EmptyIterator_key },
		{ "valid",   PH7_MOD_PUBLIC, "", "@false", vm_builtin_EmptyIterator_valid },
		{ "rewind",  PH7_MOD_PUBLIC, "", "@void", vm_builtin_EmptyIterator_nop },
	};
	/*
	 * PH7_CLASS_NOCLONE on every dual iterator: php refuses `clone` for all of them
	 * (its inner iterator handle cannot be duplicated), and a slot-by-slot copy here
	 * would share the inner iterator's cursor between two decorators. EmptyIterator
	 * has no state and php clones it happily.
	 */
	static const PH7_NativeClassSpec aSpec[] = {
		{ "OuterIterator", "Iterator", 0, PH7_CLASS_INTERFACE,
		  aOuterMethod, SX_ARRAYSIZE(aOuterMethod), 0, 0, 0, 0, 0, 0 },
		{ "IteratorIterator", 0, "OuterIterator", PH7_CLASS_NOCLONE,
		  aIterIterMethod, SX_ARRAYSIZE(aIterIterMethod), 0, 0,
		  aDualProp, SX_ARRAYSIZE(aDualProp), 0, 0 },
		{ "FilterIterator", "IteratorIterator", 0, PH7_CLASS_ABSTRACT|PH7_CLASS_NOCLONE,
		  aFilterMethod, SX_ARRAYSIZE(aFilterMethod), 0, 0, 0, 0, 0, 0 },
		{ "CallbackFilterIterator", "FilterIterator", 0, PH7_CLASS_NOCLONE,
		  aCbFilterMethod, SX_ARRAYSIZE(aCbFilterMethod), 0, 0,
		  aCbProp, SX_ARRAYSIZE(aCbProp), 0, 0 },
		{ "LimitIterator", "IteratorIterator", 0, PH7_CLASS_NOCLONE,
		  aLimitMethod, SX_ARRAYSIZE(aLimitMethod), 0, 0,
		  aLimitProp, SX_ARRAYSIZE(aLimitProp), 0, 0 },
		{ "InfiniteIterator", "IteratorIterator", 0, PH7_CLASS_NOCLONE,
		  aInfiniteMethod, SX_ARRAYSIZE(aInfiniteMethod), 0, 0, 0, 0, 0, 0 },
		{ "NoRewindIterator", "IteratorIterator", 0, PH7_CLASS_NOCLONE,
		  aNoRewindMethod, SX_ARRAYSIZE(aNoRewindMethod), 0, 0, 0, 0, 0, 0 },
		{ "RegexIterator", "FilterIterator", 0, PH7_CLASS_NOCLONE,
		  aRegexMethod, SX_ARRAYSIZE(aRegexMethod),
		  aRegexConst, SX_ARRAYSIZE(aRegexConst),
		  aRegexProp, SX_ARRAYSIZE(aRegexProp), 0, 0 },
		{ "AppendIterator", "IteratorIterator", 0, PH7_CLASS_NOCLONE,
		  aAppendMethod, SX_ARRAYSIZE(aAppendMethod), 0, 0,
		  aAppendProp, SX_ARRAYSIZE(aAppendProp), 0, 0 },
		{ "RecursiveIterator", "Iterator", 0, PH7_CLASS_INTERFACE,
		  aRecursiveMethod, SX_ARRAYSIZE(aRecursiveMethod), 0, 0, 0, 0, 0, 0 },
		/* RecursiveArrayIterator is CLONEABLE (php clones an ArrayIterator happily) and
		 * inherits every one of its parent's C bodies, storage slots included. */
		{ "RecursiveArrayIterator", "ArrayIterator", "RecursiveIterator", 0,
		  aRaiMethod, SX_ARRAYSIZE(aRaiMethod),
		  aRaiConst, SX_ARRAYSIZE(aRaiConst), 0, 0, 0, 0 },
		{ "RecursiveFilterIterator", "FilterIterator", "RecursiveIterator",
		  PH7_CLASS_ABSTRACT|PH7_CLASS_NOCLONE,
		  aRfiMethod, SX_ARRAYSIZE(aRfiMethod), 0, 0, 0, 0, 0, 0 },
		{ "EmptyIterator", 0, "Iterator", 0,
		  aEmptyMethod, SX_ARRAYSIZE(aEmptyMethod), 0, 0, 0, 0, 0, 0 },
	};
	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
}
static const char zSplLib[] =
"class RecursiveIteratorIterator implements OuterIterator {"
" const LEAVES_ONLY = 0;"
" const SELF_FIRST = 1;"
" const CHILD_FIRST = 2;"
" const CATCH_GET_CHILD = 16;"
" private $__root = null;"
" private $__st = [];"
" private $__mode = 0;"
" private $__maxDepth = false;"
" private $__post = false;"
" private $__live = false;"
" public function __construct($iterator, $mode = 0, $flags = 0){"
"  while( $iterator instanceof IteratorAggregate ){ $iterator = $iterator->getIterator(); }"
"  if( !($iterator instanceof RecursiveIterator) ){"
"   throw new TypeError('RecursiveIteratorIterator::__construct(): Argument #1"
" ($iterator) must be of type RecursiveIterator, ' . get_debug_type($iterator) . ' given');"
"  }"
"  $this->__root = $iterator;"
"  $this->__mode = (int)$mode | (int)$flags;"
" }"
" public function getInnerIterator(){ return end($this->__st) ?: $this->__root; }"
" public function getSubIterator($level = null){"
"  if( $level === null ){ $level = count($this->__st) - 1; }"
"  return $this->__st[$level] ?? null;"
" }"
" public function getDepth(){ return count($this->__st) - 1; }"
" public function getMaxDepth(){ return $this->__maxDepth; }"
" public function setMaxDepth($maxDepth = -1){"
"  $maxDepth = (int)$maxDepth;"
"  if( $maxDepth < -1 ){"
"   throw new Exception('Parameter max_depth must be >= -1');"
"  }"
"  $this->__maxDepth = $maxDepth === -1 ? false : $maxDepth;"
" }"
" public function callHasChildren(){"
"  $it = end($this->__st);"
"  return $it ? $it->hasChildren() : false;"
" }"
" public function callGetChildren(){"
"  $it = end($this->__st);"
"  return $it ? $it->getChildren() : null;"
" }"
" public function beginIteration(){}"
" public function endIteration(){}"
" public function beginChildren(){}"
" public function endChildren(){}"
" public function nextElement(){}"
" private function __riDepthOk(){"
"  return $this->__maxDepth === false || (count($this->__st) - 1) < $this->__maxDepth;"
" }"
" private function __riDescend(){"
"  /* push the current element's children, positioned at their start */"
"  if( $this->__mode & self::CATCH_GET_CHILD ){"
"   try { $child = $this->callGetChildren(); }"
"   catch (Exception $e) { return false; }"
"  }else{"
"   $child = $this->callGetChildren();"
"  }"
"  if( !($child instanceof RecursiveIterator) ){ return false; }"
"  $child->rewind();"
"  $this->__st[] = $child;"
"  $this->beginChildren();"
"  return true;"
" }"
" private function __riFetch(){"
"  $m = $this->__mode & 3;"
"  for(;;){"
"   if( count($this->__st) === 0 ){"
"    $this->__live = false;"
"    /* php keeps the root level addressable after exhaustion (getDepth 0,"
"     * getSubIterator() returns the root) */"
"    $this->__st = [$this->__root];"
"    $this->endIteration();"
"    return;"
"   }"
"   $it = end($this->__st);"
"   if( !$it->valid() ){"
"    array_pop($this->__st);"
"    $this->endChildren();"
"    if( count($this->__st) === 0 ){ continue; }"
"    if( $m === self::CHILD_FIRST ){"
"     /* the parent node yields now, after its subtree */"
"     $this->__post = true;"
"     $this->__live = true;"
"     return;"
"    }"
"    end($this->__st)->next();"
"    continue;"
"   }"
"   if( $m === self::LEAVES_ONLY && $it->hasChildren() && $this->__riDepthOk() ){"
"    if( $this->__riDescend() ){ continue; }"
"   }"
"   if( $m === self::CHILD_FIRST && $it->hasChildren() && $this->__riDepthOk() ){"
"    if( $this->__riDescend() ){ continue; }"
"   }"
"   $this->__post = false;"
"   $this->__live = true;"
"   $this->nextElement();"
"   return;"
"  }"
" }"
" public function rewind(){"
"  $this->__st = [$this->__root];"
"  $this->__root->rewind();"
"  $this->__post = false;"
"  $this->beginIteration();"
"  $this->__riFetch();"
" }"
" public function valid(){ return $this->__live; }"
" public function current(){"
"  $it = end($this->__st);"
"  return $it ? $it->current() : null;"
" }"
" public function key(){"
"  $it = end($this->__st);"
"  return $it ? $it->key() : null;"
" }"
" public function next(){"
"  if( !$this->__live ){ return; }"
"  $m = $this->__mode & 3;"
"  $it = end($this->__st);"
"  if( $this->__post ){"
"   /* leaving a CHILD_FIRST post-visit: advance past the node */"
"   $this->__post = false;"
"   $it->next();"
"   $this->__riFetch();"
"   return;"
"  }"
"  if( $m === self::SELF_FIRST && $it->hasChildren() && $this->__riDepthOk() ){"
"   if( $this->__riDescend() ){ $this->__riFetch(); return; }"
"  }"
"  $it->next();"
"  $this->__riFetch();"
" }"
"}"
"class SplDoublyLinkedList implements Iterator, Countable, ArrayAccess {"
" const IT_MODE_LIFO = 2;"
" const IT_MODE_FIFO = 0;"
" const IT_MODE_DELETE = 1;"
" const IT_MODE_KEEP = 0;"
" private $__q = [];"
" private $__mode = 0;"
" private $__i = 0;"
" public function __construct(){"
"  if( $this instanceof SplStack ){ $this->__mode = 2; }"
" }"
" public function setIteratorMode($mode){"
"  $mode = (int)$mode;"
"  if( ($this instanceof SplStack || $this instanceof SplQueue)"
"   && ($mode & 2) !== ($this->__mode & 2) ){"
"   throw new RuntimeException(\"Iterators' LIFO/FIFO modes for SplStack/SplQueue"
" objects are frozen\");"
"  }"
"  $this->__mode = $mode;"
" }"
" public function getIteratorMode(){ return $this->__mode; }"
" public function push($value){ $this->__q[] = $value; }"
" public function pop(){"
"  if( count($this->__q) === 0 ){"
"   throw new RuntimeException(\"Can't pop from an empty datastructure\");"
"  }"
"  return array_pop($this->__q);"
" }"
" public function shift(){"
"  if( count($this->__q) === 0 ){"
"   throw new RuntimeException(\"Can't shift from an empty datastructure\");"
"  }"
"  return array_shift($this->__q);"
" }"
" public function unshift($value){ array_unshift($this->__q, $value); }"
" public function top(){"
"  if( count($this->__q) === 0 ){"
"   throw new RuntimeException(\"Can't peek at an empty datastructure\");"
"  }"
"  return $this->__q[count($this->__q) - 1];"
" }"
" public function bottom(){"
"  if( count($this->__q) === 0 ){"
"   throw new RuntimeException(\"Can't peek at an empty datastructure\");"
"  }"
"  return $this->__q[0];"
" }"
" public function isEmpty(){ return count($this->__q) === 0; }"
" public function count(){ return count($this->__q); }"
" public function toArray(){ return $this->__q; }"
" public function add($index, $value){"
"  $index = (int)$index;"
"  if( $index < 0 || $index > count($this->__q) ){"
"   throw new OutOfRangeException(get_class($this) === 'SplDoublyLinkedList'"
"    ? 'SplDoublyLinkedList::add(): Argument #1 ($index) is out of range'"
"    : get_class($this) . '::add(): Argument #1 ($index) is out of range');"
"  }"
"  array_splice($this->__q, $index, 0, [$value]);"
" }"
" public function offsetExists($index){"
"  return is_int($index) || ctype_digit((string)$index)"
"   ? ((int)$index >= 0 && (int)$index < count($this->__q)) : false;"
" }"
" public function offsetGet($index){"
"  $index = (int)$index;"
"  if( $index < 0 || $index >= count($this->__q) ){"
"   throw new OutOfRangeException('SplDoublyLinkedList::offsetGet(): Argument #1"
" ($index) is out of range');"
"  }"
"  return $this->__q[$index];"
" }"
" public function offsetSet($index, $value){"
"  if( $index === null ){ $this->__q[] = $value; return; }"
"  $index = (int)$index;"
"  if( $index < 0 || $index >= count($this->__q) ){"
"   throw new OutOfRangeException('SplDoublyLinkedList::offsetSet(): Argument #1"
" ($index) is out of range');"
"  }"
"  $this->__q[$index] = $value;"
" }"
" public function offsetUnset($index){"
"  $index = (int)$index;"
"  if( $index < 0 || $index >= count($this->__q) ){"
"   throw new OutOfRangeException('SplDoublyLinkedList::offsetUnset(): Argument #1"
" ($index) is out of range');"
"  }"
"  array_splice($this->__q, $index, 1);"
" }"
" public function rewind(){"
"  $this->__i = ($this->__mode & 2) ? count($this->__q) - 1 : 0;"
" }"
" public function valid(){"
"  return $this->__i >= 0 && $this->__i < count($this->__q);"
" }"
" public function current(){ return $this->__q[$this->__i] ?? null; }"
" public function key(){ return $this->__i; }"
" public function next(){"
"  if( $this->__mode & 1 ){"
"   /* IT_MODE_DELETE consumes the element just visited */"
"   if( $this->__mode & 2 ){ array_pop($this->__q); $this->__i = count($this->__q) - 1; }"
"   else { array_shift($this->__q); }"
"  }else{"
"   $this->__i += ($this->__mode & 2) ? -1 : 1;"
"  }"
" }"
" public function prev(){ $this->__i += ($this->__mode & 2) ? 1 : -1; }"
"}"
"class SplStack extends SplDoublyLinkedList {}"
"class SplQueue extends SplDoublyLinkedList {"
" public function enqueue($value){ $this->push($value); }"
" public function dequeue(){ return $this->shift(); }"
"}"
"abstract class SplHeap implements Iterator, Countable {"
" private $__h = [];"
" abstract protected function compare($value1, $value2);"
" private function __hSiftUp($i){"
"  while( $i > 0 ){"
"   $p = ($i - 1) >> 1;"
"   if( $this->compare($this->__h[$i], $this->__h[$p]) <= 0 ){ break; }"
"   $t = $this->__h[$p]; $this->__h[$p] = $this->__h[$i]; $this->__h[$i] = $t;"
"   $i = $p;"
"  }"
" }"
" private function __hSiftDown($i){"
"  $n = count($this->__h);"
"  for(;;){"
"   $l = 2 * $i + 1; $r = $l + 1; $b = $i;"
"   if( $l < $n && $this->compare($this->__h[$l], $this->__h[$b]) > 0 ){ $b = $l; }"
"   if( $r < $n && $this->compare($this->__h[$r], $this->__h[$b]) > 0 ){ $b = $r; }"
"   if( $b === $i ){ break; }"
"   $t = $this->__h[$b]; $this->__h[$b] = $this->__h[$i]; $this->__h[$i] = $t;"
"   $i = $b;"
"  }"
" }"
" public function insert($value){"
"  $this->__h[] = $value;"
"  $this->__hSiftUp(count($this->__h) - 1);"
"  return true;"
" }"
" public function extract(){"
"  $n = count($this->__h);"
"  if( $n === 0 ){"
"   throw new RuntimeException(\"Can't extract from an empty heap\");"
"  }"
"  $top = $this->__h[0];"
"  $last = array_pop($this->__h);"
"  if( $n > 1 ){"
"   $this->__h[0] = $last;"
"   $this->__hSiftDown(0);"
"  }"
"  return $top;"
" }"
" public function top(){"
"  if( count($this->__h) === 0 ){"
"   throw new RuntimeException(\"Can't peek at an empty heap\");"
"  }"
"  return $this->__h[0];"
" }"
" public function isEmpty(){ return count($this->__h) === 0; }"
" public function count(){ return count($this->__h); }"
" public function isCorrupted(){ return false; }"
" public function recoverFromCorruption(){ return true; }"
" public function rewind(){}"
" public function valid(){ return count($this->__h) > 0; }"
" public function current(){ return count($this->__h) ? $this->__h[0] : null; }"
" public function key(){ return count($this->__h) - 1; }"
" public function next(){ if( count($this->__h) ){ $this->extract(); } }"
"}"
"class SplMinHeap extends SplHeap {"
" protected function compare($value1, $value2){ return $value2 <=> $value1; }"
"}"
"class SplMaxHeap extends SplHeap {"
" protected function compare($value1, $value2){ return $value1 <=> $value2; }"
"}"
"class SplPriorityQueue implements Iterator, Countable {"
" const EXTR_DATA = 1;"
" const EXTR_PRIORITY = 2;"
" const EXTR_BOTH = 3;"
" private $__h = [];"
" private $__serial = PHP_INT_MAX;"
" private $__flags = 1;"
" public function compare($priority1, $priority2){ return $priority1 <=> $priority2; }"
" private function __pqCmp($a, $b){"
"  /* NO tie-break: php's heap swaps only on strictly-greater, which fixes"
"   * the (documented-as-undefined) equal-priority order it exhibits */"
"  return $this->compare($a[0], $b[0]);"
" }"
" private function __pqSiftUp($i){"
"  while( $i > 0 ){"
"   $p = ($i - 1) >> 1;"
"   if( $this->__pqCmp($this->__h[$i], $this->__h[$p]) <= 0 ){ break; }"
"   $t = $this->__h[$p]; $this->__h[$p] = $this->__h[$i]; $this->__h[$i] = $t;"
"   $i = $p;"
"  }"
" }"
" private function __pqSiftDown($i){"
"  $n = count($this->__h);"
"  for(;;){"
"   $l = 2 * $i + 1; $r = $l + 1; $b = $i;"
"   if( $l < $n && $this->__pqCmp($this->__h[$l], $this->__h[$b]) > 0 ){ $b = $l; }"
"   if( $r < $n && $this->__pqCmp($this->__h[$r], $this->__h[$b]) > 0 ){ $b = $r; }"
"   if( $b === $i ){ break; }"
"   $t = $this->__h[$b]; $this->__h[$b] = $this->__h[$i]; $this->__h[$i] = $t;"
"   $i = $b;"
"  }"
" }"
" public function insert($value, $priority){"
"  $this->__h[] = [$priority, $this->__serial--, $value];"
"  $this->__pqSiftUp(count($this->__h) - 1);"
"  return true;"
" }"
" private function __pqShape($node){"
"  if( $this->__flags === self::EXTR_BOTH ){"
"   return ['data' => $node[2], 'priority' => $node[0]];"
"  }"
"  if( $this->__flags === self::EXTR_PRIORITY ){ return $node[0]; }"
"  return $node[2];"
" }"
" public function extract(){"
"  $n = count($this->__h);"
"  if( $n === 0 ){"
"   throw new RuntimeException(\"Can't extract from an empty heap\");"
"  }"
"  $top = $this->__h[0];"
"  $last = array_pop($this->__h);"
"  if( $n > 1 ){"
"   $this->__h[0] = $last;"
"   $this->__pqSiftDown(0);"
"  }"
"  return $this->__pqShape($top);"
" }"
" public function top(){"
"  if( count($this->__h) === 0 ){"
"   throw new RuntimeException(\"Can't peek at an empty heap\");"
"  }"
"  return $this->__pqShape($this->__h[0]);"
" }"
" public function setExtractFlags($flags){ $this->__flags = (int)$flags; }"
" public function getExtractFlags(){ return $this->__flags; }"
" public function isEmpty(){ return count($this->__h) === 0; }"
" public function count(){ return count($this->__h); }"
" public function isCorrupted(){ return false; }"
" public function recoverFromCorruption(){ return true; }"
" public function rewind(){}"
" public function valid(){ return count($this->__h) > 0; }"
" public function current(){ return count($this->__h) ? $this->__pqShape($this->__h[0]) : null; }"
" public function key(){ return count($this->__h) - 1; }"
" public function next(){ if( count($this->__h) ){ $this->extract(); } }"
"}"
"class SplFixedArray implements ArrayAccess, Countable, IteratorAggregate, JsonSerializable {"
" private $__a = [];"
" private $__n = 0;"
" public function __construct($size = 0){"
"  $this->setSize((int)$size);"
" }"
" private function __faIdx($index, $method){"
"  if( !is_int($index) ){"
"   if( is_string($index) && ctype_digit($index) ){"
"    $index = (int)$index;"
"   }else{"
"    throw new TypeError('Cannot access offset of type ' . get_debug_type($index)"
"     . ' on SplFixedArray');"
"   }"
"  }"
"  if( $index < 0 || $index >= $this->__n ){"
"   throw new OutOfBoundsException('Index invalid or out of range');"
"  }"
"  return $index;"
" }"
" public function offsetExists($index){"
"  if( !is_int($index) && !(is_string($index) && ctype_digit($index)) ){ return false; }"
"  $index = (int)$index;"
"  return $index >= 0 && $index < $this->__n && $this->__a[$index] !== null;"
" }"
" public function offsetGet($index){ return $this->__a[$this->__faIdx($index, 'offsetGet')]; }"
" public function offsetSet($index, $value){ $this->__a[$this->__faIdx($index, 'offsetSet')] = $value; }"
" public function offsetUnset($index){ $this->__a[$this->__faIdx($index, 'offsetUnset')] = null; }"
" public function getSize(){ return $this->__n; }"
" public function setSize($size){"
"  $size = (int)$size;"
"  if( $size < 0 ){"
"   throw new ValueError('SplFixedArray::setSize(): Argument #1 ($size) must be"
" greater than or equal to 0');"
"  }"
"  if( $size < $this->__n ){"
"   $this->__a = array_slice($this->__a, 0, $size);"
"  }else{"
"   for( $i = $this->__n; $i < $size; $i++ ){ $this->__a[$i] = null; }"
"  }"
"  $this->__n = $size;"
"  return true;"
" }"
" public function count(){ return $this->__n; }"
" public function toArray(){ return $this->__a; }"
" public static function fromArray($array, $preserveKeys = true){"
"  $f = new SplFixedArray(0);"
"  if( $preserveKeys ){"
"   $max = -1;"
"   foreach( $array as $k => $v ){"
"    if( !is_int($k) || $k < 0 ){"
"     throw new InvalidArgumentException('array must contain only positive integer keys');"
"    }"
"    if( $k > $max ){ $max = $k; }"
"   }"
"   $f->setSize($max + 1);"
"   foreach( $array as $k => $v ){ $f[$k] = $v; }"
"  }else{"
"   $vals = array_values($array);"
"   $f->setSize(count($vals));"
"   foreach( $vals as $k => $v ){ $f[$k] = $v; }"
"  }"
"  return $f;"
" }"
" public function getIterator(): Generator {"
"  for( $i = 0; $i < $this->__n; $i++ ){ yield $i => $this->__a[$i]; }"
" }"
" public function jsonSerialize(){ return $this->__a; }"
"}"
"class SplObjectStorage implements Countable, Iterator, ArrayAccess {"
" private $__o = [];"
" private $__i = 0;"
/* php DEPRECATES the next three since 8.5. PHL keeps them working and says
 * nothing: the notice is the only difference, and it is not one valid php
 * depends on. The `__spl_deprecated()` thunk that used to stand in these
 * bodies had been a no-op for exactly that reason, so it is gone. */
" public function attach($object, $info = null){ $this->offsetSet($object, $info); }"
" public function detach($object){ $this->offsetUnset($object); }"
" public function contains($object){ return $this->offsetExists($object); }"
" public function offsetSet($object, $info = null){"
"  $this->__o[spl_object_id($object)] = [$object, $info];"
" }"
" public function offsetExists($object){"
"  return isset($this->__o[spl_object_id($object)]);"
" }"
" public function offsetGet($object){"
"  $id = spl_object_id($object);"
"  if( !isset($this->__o[$id]) ){"
"   throw new UnexpectedValueException('Object not found');"
"  }"
"  return $this->__o[$id][1];"
" }"
" public function offsetUnset($object){"
"  unset($this->__o[spl_object_id($object)]);"
" }"
" public function addAll($storage){"
"  foreach( $storage as $obj ){"
"   $this->offsetSet($obj, $storage[$obj]);"
"  }"
"  return $this->count();"
" }"
" public function removeAll($storage){"
"  foreach( $storage as $obj ){ $this->offsetUnset($obj); }"
"  return $this->count();"
" }"
" public function removeAllExcept($storage){"
"  foreach( $this->__o as $id => $pair ){"
"   if( !$storage->offsetExists($pair[0]) ){ unset($this->__o[$id]); }"
"  }"
"  return $this->count();"
" }"
" public function getHash($object){ return spl_object_hash($object); }"
" public function count($mode = 0){ return count($this->__o); }"
" public function getInfo(){"
"  $pair = array_values($this->__o)[$this->__i] ?? null;"
"  return $pair === null ? null : $pair[1];"
" }"
" public function setInfo($info){"
"  $keys = array_keys($this->__o);"
"  if( isset($keys[$this->__i]) ){ $this->__o[$keys[$this->__i]][1] = $info; }"
" }"
" public function rewind(){ $this->__i = 0; }"
" public function valid(){ return $this->__i < count($this->__o); }"
" public function key(){ return $this->__i; }"
" public function current(){"
"  $pair = array_values($this->__o)[$this->__i] ?? null;"
"  return $pair === null ? null : $pair[0];"
" }"
" public function next(){ $this->__i++; }"
"}"
"interface SplObserver {"
" public function update(SplSubject $subject);"
"}"
"interface SplSubject {"
" public function attach(SplObserver $observer);"
" public function detach(SplObserver $observer);"
" public function notify();"
"}"
"class SplFileInfo implements Stringable {"
" protected $__pathName = '';"
" protected $__fileName = '';"
" public function __construct($path){"
"  $this->__pathName = (string)$path;"
"  $this->__fileName = basename($this->__pathName);"
" }"
" public function getPathname(){ return $this->__pathName; }"
" public function getFilename(){ return $this->__fileName; }"
" public function getPath(){ return dirname($this->__pathName); }"
" public function getBasename($suffix = ''){"
"  $b = basename($this->__pathName);"
"  if( $suffix !== '' && strlen($suffix) < strlen($b) && substr($b, -strlen($suffix)) === $suffix ){"
"   $b = substr($b, 0, -strlen($suffix));"
"  }"
"  return $b;"
" }"
" public function getExtension(){ return pathinfo($this->__pathName, PATHINFO_EXTENSION); }"
" public function getRealPath(){ return realpath($this->__pathName); }"
" public function isDir(){ return is_dir($this->__pathName); }"
" public function isFile(){ return is_file($this->__pathName); }"
" public function isLink(){ return is_link($this->__pathName); }"
" public function isReadable(){ return is_readable($this->__pathName); }"
" public function isWritable(){ return is_writable($this->__pathName); }"
" public function getSize(){ return filesize($this->__pathName); }"
" public function getMTime(){ return filemtime($this->__pathName); }"
" public function getATime(){ return fileatime($this->__pathName); }"
" public function getCTime(){ return filectime($this->__pathName); }"
" public function getType(){ return filetype($this->__pathName); }"
" public function getFileInfo(){ return new SplFileInfo($this->__pathName); }"
" public function getPathInfo(){ return new SplFileInfo(dirname($this->__pathName)); }"
" public function __toString(){ return $this->__pathName; }"
"}"
"class DirectoryIterator extends SplFileInfo implements SeekableIterator {"
" protected $__dir = '';"
" protected $__entries = array();"
" protected $__pos = 0;"
" public function __construct($path){"
"  $this->__dir = (string)$path;"
"  parent::__construct($this->__dir);"
"  $this->__load();"
" }"
" protected function __load(){"
"  $this->__entries = array();"
"  $h = @opendir($this->__dir);"
"  if( $h !== false ){"
"   while( ($e = readdir($h)) !== false ){ $this->__entries[] = $e; }"
"   closedir($h);"
"  }"
"  $this->__pos = 0;"
"  $this->__sync();"
" }"
" protected function __join($name){"
"  $d = $this->__dir;"
"  $last = substr($d, -1);"
"  $sep = ($last === '/' || $last === '\\\\' || $d === '') ? '' : '/';"
"  return $d . $sep . $name;"
" }"
" protected function __sync(){"
"  if( $this->__pos >= 0 && $this->__pos < count($this->__entries) ){"
"   $name = $this->__entries[$this->__pos];"
"   $this->__fileName = $name;"
"   $this->__pathName = $this->__join($name);"
"  }"
" }"
" public function isDot(){ $n = $this->__fileName; return $n === '.' || $n === '..'; }"
" public function getFilename(){ return $this->__fileName; }"
" public function current(){ return $this; }"
" public function key(){ return $this->__pos; }"
" public function next(){ $this->__pos++; $this->__sync(); }"
" public function rewind(){ $this->__pos = 0; $this->__sync(); }"
" public function valid(){ return $this->__pos < count($this->__entries); }"
" public function seek($position){ $this->__pos = (int)$position; $this->__sync(); }"
" public function getFlags(){ return 0; }"
"}"
"class FilesystemIterator extends DirectoryIterator {"
" const CURRENT_AS_PATHNAME = 32;"
" const CURRENT_AS_FILEINFO = 0;"
" const CURRENT_AS_SELF = 16;"
" const CURRENT_MODE_MASK = 240;"
" const KEY_AS_PATHNAME = 0;"
" const KEY_AS_FILENAME = 256;"
" const FOLLOW_SYMLINKS = 512;"
" const KEY_MODE_MASK = 3840;"
" const NEW_CURRENT_AND_KEY = 256;"
" const OTHER_MODE_MASK = 12288;"
" const SKIP_DOTS = 4096;"
" const UNIX_PATHS = 8192;"
" protected $__flags = 4096;"
" public function __construct($path, $flags = 4096){"
"  $this->__flags = (int)$flags;"
"  parent::__construct($path);"
" }"
" protected function __skipDots(){"
"  if( $this->__flags & self::SKIP_DOTS ){"
"   while( ($this->__pos < count($this->__entries)) && $this->isDot() ){ $this->__pos++; $this->__sync(); }"
"  }"
" }"
" public function rewind(){ $this->__pos = 0; $this->__sync(); $this->__skipDots(); }"
" public function next(){ $this->__pos++; $this->__sync(); $this->__skipDots(); }"
" public function current(){"
"  $mode = $this->__flags & self::CURRENT_MODE_MASK;"
"  if( $mode === self::CURRENT_AS_PATHNAME ){ return $this->getPathname(); }"
"  if( $mode === self::CURRENT_AS_SELF ){ return $this; }"
"  return new SplFileInfo($this->getPathname());"
" }"
" public function key(){"
"  if( $this->__flags & self::KEY_AS_FILENAME ){ return $this->getFilename(); }"
"  return $this->getPathname();"
" }"
" public function getFlags(){ return $this->__flags; }"
" public function setFlags($flags){ $this->__flags = (int)$flags; }"
"}"
"class RecursiveDirectoryIterator extends FilesystemIterator implements RecursiveIterator {"
" public function hasChildren(){"
"  if( $this->isDot() ){ return false; }"
"  return $this->isDir();"
" }"
" public function getChildren(){"
"  return new RecursiveDirectoryIterator($this->getPathname(), $this->__flags);"
" }"
" public function getSubPath(){ return ''; }"
" public function getSubPathname(){ return $this->getFilename(); }"
"}"
;

PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm)
{
	sxi32 rc = VmInstallWeak(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Before the chunk, not after: `RecursiveArrayIterator extends ArrayIterator` is
	 * still PHP, and the compiler has to find the native class it extends. */
	rc = VmInstallSplStore(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Also before the chunk: AppendIterator is still PHP and names OuterIterator as
	 * it compiles, as do the Recursive family and the datastructures. */
	rc = VmInstallSplDualIterators(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	return PH7_VmEvalBuiltinChunk(&(*pVm),zSplLib,sizeof(zSplLib)-1);
}

#endif /* PH7_DISABLE_BUILTIN_FUNC */

#ifdef PH7_DISABLE_BUILTIN_FUNC
/* Tiny build: no SPL (builtin layer disabled) */
PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }
#endif
