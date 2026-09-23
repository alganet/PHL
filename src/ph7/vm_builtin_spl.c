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
		{ "__h", PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 } },
	};
	static const PH7_NativeMethodDef aRefMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "", "", vm_builtin_WeakReference_construct },
		{ "create",      PH7_MOD_PUBLIC|PH7_MOD_STATIC, "object $object", "WeakReference",
		  vm_builtin_WeakReference_create },
		{ "get",         PH7_MOD_PUBLIC, "", "?object", vm_builtin_WeakReference_get },
	};
	static const PH7_NativePropDef aMapProp[] = {
		{ WM_REFS, PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
		{ WM_VALS, PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
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
static int vm_builtin_SplStore_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap = SplStore(pCtx->pVm,PH7_ContextThis(pCtx));
	if( pMap && nArg > 1 ){
		/* A NULL key is `$o[] = $v` — the append form, which is how php's offsetSet()
		 * receives it. */
		PH7_HashmapInsert(pMap,(apArg[0]->iFlags & MEMOBJ_NULL) ? 0 : apArg[0],apArg[1]);
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
		PH7_HashmapInsert(pMap,0,apArg[0]);
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
		{ SPL_D, PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
		{ SPL_F, PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 } },
	};
	static const PH7_NativePropDef aObjProp[] = {
		{ SPL_D,  PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
		{ SPL_F,  PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 } },
		{ SPL_IT, PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "ArrayIterator", 0.0 } },
	};
	static const PH7_NativeConstDef aConst[] = {
		{ "STD_PROP_LIST",  PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1, 0, 0.0 },
		{ "ARRAY_AS_PROPS", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },
	};
	/* php's declaration order, which is the order Reflection reports. */
	static const PH7_NativeMethodDef aItMethod[] = {
		{ "__construct",  PH7_MOD_PUBLIC, "object|array $array = [], int $flags = 0", 0,
		  vm_builtin_ArrayIterator_construct },
		{ "offsetExists", PH7_MOD_PUBLIC, "mixed $key", 0, vm_builtin_SplStore_offsetExists },
		{ "offsetGet",    PH7_MOD_PUBLIC, "mixed $key", 0, vm_builtin_SplStore_offsetGet },
		{ "offsetSet",    PH7_MOD_PUBLIC, "mixed $key, mixed $value", 0, vm_builtin_SplStore_offsetSet },
		{ "offsetUnset",  PH7_MOD_PUBLIC, "mixed $key", 0, vm_builtin_SplStore_offsetUnset },
		{ "append",       PH7_MOD_PUBLIC, "mixed $value", 0, vm_builtin_SplStore_append },
		{ "getArrayCopy", PH7_MOD_PUBLIC, "", 0, vm_builtin_SplStore_getArrayCopy },
		{ "count",        PH7_MOD_PUBLIC, "", 0, vm_builtin_SplStore_count },
		{ "getFlags",     PH7_MOD_PUBLIC, "", 0, vm_builtin_SplStore_getFlags },
		{ "setFlags",     PH7_MOD_PUBLIC, "int $flags", 0, vm_builtin_SplStore_setFlags },
		{ "asort",        PH7_MOD_PUBLIC, "int $flags = 0", 0, vm_builtin_SplStore_asort },
		{ "ksort",        PH7_MOD_PUBLIC, "int $flags = 0", 0, vm_builtin_SplStore_ksort },
		{ "uasort",       PH7_MOD_PUBLIC, "callable $callback", 0, vm_builtin_SplStore_uasort },
		{ "uksort",       PH7_MOD_PUBLIC, "callable $callback", 0, vm_builtin_SplStore_uksort },
		{ "natsort",      PH7_MOD_PUBLIC, "", 0, vm_builtin_SplStore_natsort },
		{ "natcasesort",  PH7_MOD_PUBLIC, "", 0, vm_builtin_SplStore_natcasesort },
		{ "current",      PH7_MOD_PUBLIC, "", 0, vm_builtin_ArrayIterator_current },
		{ "key",          PH7_MOD_PUBLIC, "", 0, vm_builtin_ArrayIterator_key },
		{ "next",         PH7_MOD_PUBLIC, "", 0, vm_builtin_ArrayIterator_next },
		{ "rewind",       PH7_MOD_PUBLIC, "", 0, vm_builtin_ArrayIterator_rewind },
		{ "valid",        PH7_MOD_PUBLIC, "", 0, vm_builtin_ArrayIterator_valid },
		{ "seek",         PH7_MOD_PUBLIC, "int $offset", 0, vm_builtin_ArrayIterator_seek },
	};
	static const PH7_NativeMethodDef aObjMethod[] = {
		{ "__construct",      PH7_MOD_PUBLIC,
		  "object|array $array = [], int $flags = 0, string $iteratorClass = 'ArrayIterator'", 0,
		  vm_builtin_ArrayObject_construct },
		{ "offsetExists",     PH7_MOD_PUBLIC, "mixed $key", 0, vm_builtin_SplStore_offsetExists },
		{ "offsetGet",        PH7_MOD_PUBLIC, "mixed $key", 0, vm_builtin_SplStore_offsetGet },
		{ "offsetSet",        PH7_MOD_PUBLIC, "mixed $key, mixed $value", 0, vm_builtin_SplStore_offsetSet },
		{ "offsetUnset",      PH7_MOD_PUBLIC, "mixed $key", 0, vm_builtin_SplStore_offsetUnset },
		{ "append",           PH7_MOD_PUBLIC, "mixed $value", 0, vm_builtin_SplStore_append },
		{ "getArrayCopy",     PH7_MOD_PUBLIC, "", 0, vm_builtin_SplStore_getArrayCopy },
		{ "count",            PH7_MOD_PUBLIC, "", 0, vm_builtin_SplStore_count },
		{ "getFlags",         PH7_MOD_PUBLIC, "", 0, vm_builtin_SplStore_getFlags },
		{ "setFlags",         PH7_MOD_PUBLIC, "int $flags", 0, vm_builtin_SplStore_setFlags },
		{ "asort",            PH7_MOD_PUBLIC, "int $flags = 0", 0, vm_builtin_SplStore_asort },
		{ "ksort",            PH7_MOD_PUBLIC, "int $flags = 0", 0, vm_builtin_SplStore_ksort },
		{ "uasort",           PH7_MOD_PUBLIC, "callable $callback", 0, vm_builtin_SplStore_uasort },
		{ "uksort",           PH7_MOD_PUBLIC, "callable $callback", 0, vm_builtin_SplStore_uksort },
		{ "natsort",          PH7_MOD_PUBLIC, "", 0, vm_builtin_SplStore_natsort },
		{ "natcasesort",      PH7_MOD_PUBLIC, "", 0, vm_builtin_SplStore_natcasesort },
		{ "exchangeArray",    PH7_MOD_PUBLIC, "object|array $array", 0, vm_builtin_ArrayObject_exchangeArray },
		{ "getIterator",      PH7_MOD_PUBLIC, "", 0, vm_builtin_ArrayObject_getIterator },
		{ "setIteratorClass", PH7_MOD_PUBLIC, "string $iteratorClass", 0, vm_builtin_ArrayObject_setIteratorClass },
		{ "getIteratorClass", PH7_MOD_PUBLIC, "", 0, vm_builtin_ArrayObject_getIteratorClass },
		{ "__get",            PH7_MOD_PUBLIC, "$name", 0, vm_builtin_ArrayObject_get },
		{ "__set",            PH7_MOD_PUBLIC, "$name, $value", 0, vm_builtin_ArrayObject_set },
		{ "__isset",          PH7_MOD_PUBLIC, "$name", 0, vm_builtin_ArrayObject_isset },
		{ "__unset",          PH7_MOD_PUBLIC, "$name", 0, vm_builtin_ArrayObject_unset },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "SeekableIterator", 0, "Iterator", PH7_CLASS_INTERFACE,
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
	if( pThis == 0 || DualDriver(pThis) == 0 ){
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
	if( pThis == 0 || DualDriver(pThis) == 0 ){
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
	if( pThis == 0 || DualDriver(pThis) == 0 ){
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
		{ IT_IN, PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
		{ IT_IT, PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
		{ IT_CD, PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
		{ IT_CK, PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
		{ IT_CF, PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 } },
		{ IT_CP, PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 } },
	};
	static const PH7_NativePropDef aLimitProp[] = {
		{ IT_OFF, PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 } },
		{ IT_LIM, PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_INT, -1, 0, 0.0 } },
	};
	static const PH7_NativePropDef aCbProp[] = {
		{ IT_CB, PH7_MOD_PRIVATE, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
	};
	static const PH7_NativeMethodDef aOuterMethod[] = {
		{ "getInnerIterator", PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", 0, 0 },
	};
	static const PH7_NativeMethodDef aIterIterMethod[] = {
		{ "__construct",      PH7_MOD_PUBLIC, "Traversable $iterator, ?string $class = null", 0,
		  vm_builtin_IteratorIterator_construct },
		{ "getInnerIterator", PH7_MOD_PUBLIC, "", 0, vm_builtin_Dual_getInnerIterator },
		{ "rewind",           PH7_MOD_PUBLIC, "", 0, vm_builtin_IteratorIterator_rewind },
		{ "valid",            PH7_MOD_PUBLIC, "", 0, vm_builtin_Dual_valid },
		{ "key",              PH7_MOD_PUBLIC, "", 0, vm_builtin_Dual_key },
		{ "current",          PH7_MOD_PUBLIC, "", 0, vm_builtin_Dual_current },
		{ "next",             PH7_MOD_PUBLIC, "", 0, vm_builtin_IteratorIterator_next },
	};
	static const PH7_NativeMethodDef aFilterMethod[] = {
		{ "accept",      PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", 0, 0 },
		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator", 0, vm_builtin_FilterIterator_construct },
		{ "rewind",      PH7_MOD_PUBLIC, "", 0, vm_builtin_FilterIterator_rewind },
		{ "next",        PH7_MOD_PUBLIC, "", 0, vm_builtin_FilterIterator_next },
	};
	static const PH7_NativeMethodDef aCbFilterMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator, callable $callback", 0,
		  vm_builtin_CallbackFilterIterator_construct },
		{ "accept",      PH7_MOD_PUBLIC, "", 0, vm_builtin_CallbackFilterIterator_accept },
	};
	static const PH7_NativeMethodDef aLimitMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator, int $offset = 0, int $limit = -1", 0,
		  vm_builtin_LimitIterator_construct },
		{ "rewind",      PH7_MOD_PUBLIC, "", 0, vm_builtin_LimitIterator_rewind },
		{ "valid",       PH7_MOD_PUBLIC, "", 0, vm_builtin_LimitIterator_valid },
		{ "next",        PH7_MOD_PUBLIC, "", 0, vm_builtin_LimitIterator_next },
		{ "seek",        PH7_MOD_PUBLIC, "int $offset", 0, vm_builtin_LimitIterator_seek },
		{ "getPosition", PH7_MOD_PUBLIC, "", 0, vm_builtin_LimitIterator_getPosition },
	};
	static const PH7_NativeMethodDef aInfiniteMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator", 0,
		  vm_builtin_InfiniteIterator_construct },
		{ "next",        PH7_MOD_PUBLIC, "", 0, vm_builtin_InfiniteIterator_next },
	};
	static const PH7_NativeMethodDef aNoRewindMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "Iterator $iterator", 0,
		  vm_builtin_NoRewindIterator_construct },
		{ "rewind",      PH7_MOD_PUBLIC, "", 0, vm_builtin_NoRewindIterator_rewind },
		{ "valid",       PH7_MOD_PUBLIC, "", 0, vm_builtin_NoRewindIterator_valid },
		{ "key",         PH7_MOD_PUBLIC, "", 0, vm_builtin_NoRewindIterator_key },
		{ "current",     PH7_MOD_PUBLIC, "", 0, vm_builtin_NoRewindIterator_current },
		{ "next",        PH7_MOD_PUBLIC, "", 0, vm_builtin_NoRewindIterator_next },
	};
	static const PH7_NativeMethodDef aEmptyMethod[] = {
		{ "current", PH7_MOD_PUBLIC, "", 0, vm_builtin_EmptyIterator_current },
		{ "next",    PH7_MOD_PUBLIC, "", 0, vm_builtin_EmptyIterator_nop },
		{ "key",     PH7_MOD_PUBLIC, "", 0, vm_builtin_EmptyIterator_key },
		{ "valid",   PH7_MOD_PUBLIC, "", 0, vm_builtin_EmptyIterator_valid },
		{ "rewind",  PH7_MOD_PUBLIC, "", 0, vm_builtin_EmptyIterator_nop },
	};
	/*
	 * PH7_CLASS_NOCLONE on every dual iterator: php refuses `clone` for all of them
	 * (its inner iterator handle cannot be duplicated), and a slot-by-slot copy here
	 * would share the inner iterator's cursor between two decorators. EmptyIterator
	 * has no state and php clones it happily.
	 */
	static const PH7_NativeClassSpec aSpec[] = {
		{ "OuterIterator", 0, "Iterator", PH7_CLASS_INTERFACE,
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
		{ "EmptyIterator", 0, "Iterator", 0,
		  aEmptyMethod, SX_ARRAYSIZE(aEmptyMethod), 0, 0, 0, 0, 0, 0 },
	};
	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
}
static const char zSplLib[] =
"class RegexIterator extends FilterIterator {"
" const USE_KEY = 1;"
" const INVERT_MATCH = 2;"
" const MATCH = 0;"
" const GET_MATCH = 1;"
" const ALL_MATCHES = 2;"
" const SPLIT = 3;"
" const REPLACE = 4;"
" public $replacement = null;"
" private $__re = '';"
" private $__mode = 0;"
" private $__rflags = 0;"
" private $__pflags = 0;"
" private $__cur = null;"
" public function __construct($iterator, $pattern, $mode = 0, $flags = 0, $pregFlags = 0){"
"  parent::__construct($iterator);"
"  $this->__re = (string)$pattern;"
"  $this->__mode = (int)$mode;"
"  $this->__rflags = (int)$flags;"
"  $this->__pflags = (int)$pregFlags;"
" }"
" public function accept(){"
"  $in = $this->getInnerIterator();"
"  if( !$in->valid() ){ return false; }"
"  $subject = ($this->__rflags & self::USE_KEY) ? $in->key() : $in->current();"
"  $subject = (string)$subject;"
"  $this->__cur = null;"
"  $ok = false;"
"  if( $this->__mode === self::MATCH ){"
"   $ok = preg_match($this->__re, $subject) > 0;"
"  }elseif( $this->__mode === self::GET_MATCH ){"
"   $m = null;"
"   $ok = preg_match($this->__re, $subject, $m, $this->__pflags) > 0;"
"   $this->__cur = $m;"
"  }elseif( $this->__mode === self::ALL_MATCHES ){"
"   $m = null;"
"   $ok = preg_match_all($this->__re, $subject, $m, $this->__pflags) > 0;"
"   $this->__cur = $m;"
"  }elseif( $this->__mode === self::SPLIT ){"
"   $this->__cur = preg_split($this->__re, $subject, -1, $this->__pflags);"
"   $ok = is_array($this->__cur) && count($this->__cur) > 1;"
"  }elseif( $this->__mode === self::REPLACE ){"
"   $n = 0;"
"   $this->__cur = preg_replace($this->__re, (string)$this->replacement, $subject, -1, $n);"
"   $ok = $n > 0;"
"  }"
"  if( $this->__rflags & self::INVERT_MATCH ){ $ok = !$ok; }"
"  return $ok;"
" }"
" public function current(){"
"  if( $this->__mode === self::MATCH ){ return $this->getInnerIterator()->current(); }"
"  return $this->__cur;"
" }"
" public function getRegex(){ return $this->__re; }"
" public function getMode(){ return $this->__mode; }"
" public function setMode($mode){ $this->__mode = (int)$mode; }"
" public function getFlags(){ return $this->__rflags; }"
" public function setFlags($flags){ $this->__rflags = (int)$flags; }"
" public function getPregFlags(){ return $this->__pflags; }"
" public function setPregFlags($pregFlags){ $this->__pflags = (int)$pregFlags; }"
"}"
"class AppendIterator implements OuterIterator {"
" private $__its = [];"
" private $__idx = 0;"
" public function __construct(){}"
" public function append($iterator){"
"  $this->__its[] = $iterator;"
"  if( count($this->__its) === 1 ){ $iterator->rewind(); }"
" }"
" public function getInnerIterator(){ return $this->__its[$this->__idx] ?? null; }"
" public function getIteratorIndex(){"
"  return isset($this->__its[$this->__idx]) ? $this->__idx : null;"
" }"
" public function getArrayIterator(){ return new ArrayIterator($this->__its); }"
" private function __apAdvance(){"
"  while( isset($this->__its[$this->__idx])"
"   && !$this->__its[$this->__idx]->valid()"
"   && isset($this->__its[$this->__idx + 1]) ){"
"   $this->__idx++;"
"   $this->__its[$this->__idx]->rewind();"
"  }"
" }"
" public function rewind(){"
"  $this->__idx = 0;"
"  if( isset($this->__its[0]) ){ $this->__its[0]->rewind(); }"
"  $this->__apAdvance();"
" }"
" public function valid(){"
"  $in = $this->getInnerIterator();"
"  return $in !== null && $in->valid();"
" }"
" public function current(){ $in = $this->getInnerIterator(); return $in ? $in->current() : null; }"
" public function key(){ $in = $this->getInnerIterator(); return $in ? $in->key() : null; }"
" public function next(){"
"  $in = $this->getInnerIterator();"
"  if( $in ){ $in->next(); }"
"  $this->__apAdvance();"
" }"
"}"
"interface RecursiveIterator extends Iterator {"
" public function hasChildren();"
" public function getChildren();"
"}"
"class RecursiveArrayIterator extends ArrayIterator implements RecursiveIterator {"
" const CHILD_ARRAYS_ONLY = 4;"
" public function hasChildren(){"
"  $c = $this->current();"
"  return is_array($c) || is_object($c);"
" }"
" public function getChildren(){"
"  $c = get_class($this);"
"  return new $c($this->current());"
" }"
"}"
"abstract class RecursiveFilterIterator extends FilterIterator implements RecursiveIterator {"
" public function __construct(RecursiveIterator $iterator){"
"  parent::__construct($iterator);"
" }"
" public function hasChildren(){"
"  return $this->getInnerIterator()->hasChildren();"
" }"
" public function getChildren(){"
"  return new static($this->getInnerIterator()->getChildren());"
" }"
"}"
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
	/* Also before the chunk: RegexIterator and AppendIterator are still PHP and name
	 * FilterIterator / OuterIterator as they compile. */
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
