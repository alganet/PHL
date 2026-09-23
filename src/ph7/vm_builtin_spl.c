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
 * php's DEBUG presentation for a WeakReference (ph7_class::xPresent).
 *
 * var_dump/print_r show ["object"] => the target, or NULL once it has died. The
 * (array) cast and var_export show NOTHING — php's get_debug_info and
 * get_properties disagree here, which is why the callback is told which is asking.
 */
static sxi32 WkPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)
{
	VmWeakCell *pCell;
	ph7_value sKey, sVal;
	if( !bDebug ){
		return SXRET_OK; /* get_properties: php presents no property at all */
	}
	pCell = WkCellOf(pThis);
	PH7_MemObjInitFromString(&(*pVm),&sKey,0);
	PH7_MemObjStringAppend(&sKey,"object",sizeof("object")-1);
	PH7_MemObjInit(&(*pVm),&sVal);
	if( pCell && pCell->pObj ){
		sVal.x.pOther = pCell->pObj;
		MemObjSetType(&sVal,MEMOBJ_OBJ);
	}
	ph7_array_add_elem(pOut,&sKey,&sVal); /* takes its OWN reference */
	PH7_MemObjRelease(&sKey);
	/* Rule 16, the other half: this carrier never HELD a reference — the cell's
	 * is the only one — so releasing it as a MEMOBJ_OBJ would unref the target a
	 * second time and free a live object under its owner (a segfault two
	 * statements later, not here). Blank the carrier before letting it go. */
	sVal.x.pOther = 0;
	sVal.iFlags = MEMOBJ_NULL;
	PH7_MemObjRelease(&sVal);
	return SXRET_OK;
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
		  WkRefRelease, 0, WkPresent },
		{ "WeakMap", 0, 0, PH7_CLASS_FINAL|PH7_CLASS_NOSERIALIZE,
		  aMapMethod, SX_ARRAYSIZE(aMapMethod), 0, 0, aMapProp, SX_ARRAYSIZE(aMapProp),
		  0, &sWmIterVtab, 0 },
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
		  aSeekMethod, SX_ARRAYSIZE(aSeekMethod), 0, 0, 0, 0, 0, 0, 0 },
		{ "ArrayIterator", 0, "SeekableIterator,ArrayAccess,Countable", 0,
		  aItMethod, SX_ARRAYSIZE(aItMethod), aConst, SX_ARRAYSIZE(aConst),
		  aItProp, SX_ARRAYSIZE(aItProp), 0, 0, 0 },
		{ "ArrayObject", 0, "IteratorAggregate,ArrayAccess,Countable", 0,
		  aObjMethod, SX_ARRAYSIZE(aObjMethod), aConst, SX_ARRAYSIZE(aConst),
		  aObjProp, SX_ARRAYSIZE(aObjProp), 0, 0, 0 },
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
		  aOuterMethod, SX_ARRAYSIZE(aOuterMethod), 0, 0, 0, 0, 0, 0, 0 },
		{ "IteratorIterator", 0, "OuterIterator", PH7_CLASS_NOCLONE,
		  aIterIterMethod, SX_ARRAYSIZE(aIterIterMethod), 0, 0,
		  aDualProp, SX_ARRAYSIZE(aDualProp), 0, 0, 0 },
		{ "FilterIterator", "IteratorIterator", 0, PH7_CLASS_ABSTRACT|PH7_CLASS_NOCLONE,
		  aFilterMethod, SX_ARRAYSIZE(aFilterMethod), 0, 0, 0, 0, 0, 0, 0 },
		{ "CallbackFilterIterator", "FilterIterator", 0, PH7_CLASS_NOCLONE,
		  aCbFilterMethod, SX_ARRAYSIZE(aCbFilterMethod), 0, 0,
		  aCbProp, SX_ARRAYSIZE(aCbProp), 0, 0, 0 },
		{ "LimitIterator", "IteratorIterator", 0, PH7_CLASS_NOCLONE,
		  aLimitMethod, SX_ARRAYSIZE(aLimitMethod), 0, 0,
		  aLimitProp, SX_ARRAYSIZE(aLimitProp), 0, 0, 0 },
		{ "InfiniteIterator", "IteratorIterator", 0, PH7_CLASS_NOCLONE,
		  aInfiniteMethod, SX_ARRAYSIZE(aInfiniteMethod), 0, 0, 0, 0, 0, 0, 0 },
		{ "NoRewindIterator", "IteratorIterator", 0, PH7_CLASS_NOCLONE,
		  aNoRewindMethod, SX_ARRAYSIZE(aNoRewindMethod), 0, 0, 0, 0, 0, 0, 0 },
		{ "RegexIterator", "FilterIterator", 0, PH7_CLASS_NOCLONE,
		  aRegexMethod, SX_ARRAYSIZE(aRegexMethod),
		  aRegexConst, SX_ARRAYSIZE(aRegexConst),
		  aRegexProp, SX_ARRAYSIZE(aRegexProp), 0, 0, 0 },
		{ "AppendIterator", "IteratorIterator", 0, PH7_CLASS_NOCLONE,
		  aAppendMethod, SX_ARRAYSIZE(aAppendMethod), 0, 0,
		  aAppendProp, SX_ARRAYSIZE(aAppendProp), 0, 0, 0 },
		{ "RecursiveIterator", "Iterator", 0, PH7_CLASS_INTERFACE,
		  aRecursiveMethod, SX_ARRAYSIZE(aRecursiveMethod), 0, 0, 0, 0, 0, 0, 0 },
		/* RecursiveArrayIterator is CLONEABLE (php clones an ArrayIterator happily) and
		 * inherits every one of its parent's C bodies, storage slots included. */
		{ "RecursiveArrayIterator", "ArrayIterator", "RecursiveIterator", 0,
		  aRaiMethod, SX_ARRAYSIZE(aRaiMethod),
		  aRaiConst, SX_ARRAYSIZE(aRaiConst), 0, 0, 0, 0, 0 },
		{ "RecursiveFilterIterator", "FilterIterator", "RecursiveIterator",
		  PH7_CLASS_ABSTRACT|PH7_CLASS_NOCLONE,
		  aRfiMethod, SX_ARRAYSIZE(aRfiMethod), 0, 0, 0, 0, 0, 0, 0 },
		{ "EmptyIterator", 0, "Iterator", 0,
		  aEmptyMethod, SX_ARRAYSIZE(aEmptyMethod), 0, 0, 0, 0, 0, 0, 0 },
	};
	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
}
/*
 * ---------------------------------------------------------------------------
 * RecursiveIteratorIterator.
 *
 * php's `spl_recursive_it_object` is a STACK OF LEVELS plus a five-value state
 * machine, and reading the struct before the methods (rule 43) is what this
 * conversion turns on. Each level carries the sub-iterator AND its own
 * RecursiveIteratorState; `move_forward` is one loop over that pair, and every
 * method is a two-line reader of it. The chunk instead kept a stack of iterators
 * with the state implied by two booleans (`__post`, `__live`), which is where all
 * eight of its divergences came from:
 *
 *   - getDepth()/getSubIterator()/getInnerIterator() answered from an EMPTY stack
 *     before the first rewind(), so they reported -1 and null where php reports 0
 *     and the root -- php seeds level 0 in the CONSTRUCTOR and never unseeds it.
 *   - valid() answered a `__live` flag that only rewind() sets; php ASKS the
 *     levels (any valid sub-iterator, walking down), so a fresh instance over a
 *     non-empty iterator is already valid().
 *   - LEAVES_ONLY past max depth YIELDED the container; php skips it, which is
 *     the whole point of the mode (`walk-leaves-maxdepth0` returned the `b`
 *     array as if it were a leaf).
 *   - the mode was `$mode | $flags` masked with & 3, so CATCH_GET_CHILD passed
 *     as $mode descended like LEAVES_ONLY; php compares mode EXACTLY and an
 *     unknown mode matches no arm at all, descending nowhere.
 *   - hasChildren() was called on the sub-iterator DIRECTLY, so a subclass
 *     overriding callHasChildren() -- php's documented hook -- was never asked.
 *   - endChildren() ran AFTER the pop, reporting a depth one too low and firing
 *     a spurious final call at depth -1; php calls it before the pop.
 *   - a second rewind() fired beginIteration() again; php's in_iteration latch
 *     makes it once per iteration.
 *   - getChildren() returning a non-RecursiveIterator was silently treated as
 *     "no children"; php throws UnexpectedValueException.
 *
 * The level stack lives in two parallel arrays indexed by level rather than in a
 * C block behind a handle: php SERIALIZES this class (`O:25:"…":0:{}`), and a raw
 * pointer in a hidden slot is exactly what rule 19 exists to keep out of
 * serialize() output. Every slot is PH7_MOD_HIDDEN, so php's zero-property
 * presentation holds for var_dump, print_r, var_export, (array) and Reflection.
 */
#define RIT_ST   "__st"   /* php's iterators[level].zobject */
#define RIT_SS   "__ss"   /* php's iterators[level].state */
#define RIT_LVL  "__lvl"  /* php's object->level */
#define RIT_MD   "__md"   /* php's object->mode, stored UNMASKED */
#define RIT_FL   "__fl"   /* php's object->flags */
#define RIT_MX   "__mx"   /* php's object->max_depth, -1 = unlimited */
#define RIT_II   "__ii"   /* php's object->in_iteration */
#define RIT_RD   "__rd"   /* php's `object->iterators != NULL`: the parent ctor ran */

/* php's RecursiveIteratorState */
#define RS_NEXT  0
#define RS_TEST  1
#define RS_SELF  2
#define RS_CHILD 3
#define RS_START 4

/* php's RecursiveIteratorMode + the one flag */
#define RIT_LEAVES_ONLY     0
#define RIT_SELF_FIRST      1
#define RIT_CHILD_FIRST     2
#define RIT_CATCH_GET_CHILD 16

/*
 * php's `object->iterators != NULL`. Its get_method handler refuses EVERY method
 * on an instance whose parent constructor never ran -- not the individual bodies,
 * which is why the refusal is an Error naming the RUNTIME class and why even
 * getDepth() raises it.
 */
static int RitReady(ph7_class_instance *pThis)
{
	return pThis && PH7_NativeAttrInt(pThis,RIT_RD) != 0;
}
static sxi32 RitNotReady(ph7_context *pCtx)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	SyString *pName = pThis ? &pThis->pClass->sName : 0;
	return PH7_VmThrowException(pCtx,"Error",
		"The %z instance wasn't initialized properly",pName);
}
static int RitInt(ph7_class_instance *pThis,const char *zSlot)
{
	return (int)PH7_NativeAttrInt(pThis,zSlot);
}
/* One of the two level-indexed arrays, materialized on first use. */
static ph7_hashmap * RitMap(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot)
{
	ph7_value *pSlot = PH7_NativeAttr(pThis,zSlot);
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
static ph7_value * RitAt(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot,int iLevel)
{
	ph7_hashmap *pMap = RitMap(pVm,pThis,zSlot);
	ph7_hashmap_node *pNode = 0;
	if( pMap == 0 || HashmapLookupIntKey(pMap,(sxi64)iLevel,&pNode) != SXRET_OK ){
		return 0;
	}
	return HashmapExtractNodeValue(pNode);
}
static void RitPut(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot,int iLevel,ph7_value *pVal)
{
	ph7_hashmap *pMap = RitMap(pVm,pThis,zSlot);
	ph7_value sKey;
	if( pMap == 0 ){
		return;
	}
	PH7_MemObjInitFromInt(pVm,&sKey,(sxi64)iLevel);
	PH7_HashmapInsert(pMap,&sKey,pVal);
	PH7_MemObjRelease(&sKey);
}
static void RitErase(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot,int iLevel)
{
	ph7_hashmap *pMap = RitMap(pVm,pThis,zSlot);
	ph7_hashmap_node *pNode = 0;
	if( pMap && HashmapLookupIntKey(pMap,(sxi64)iLevel,&pNode) == SXRET_OK ){
		PH7_HashmapUnlinkNode(pNode,TRUE);
	}
}
/*
 * The sub-iterator at a level. Re-resolved on every use on purpose: RitAt()
 * hands back a pointer into pVm->aMemObj, which REALLOCATES as the VM reserves
 * objects, and every call into a user iterator reserves some (rule 47).
 */
static ph7_class_instance * RitSub(ph7_vm *pVm,ph7_class_instance *pThis,int iLevel)
{
	ph7_value *pVal = RitAt(pVm,pThis,RIT_ST,iLevel);
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_OBJ) == 0 ){
		return 0;
	}
	return (ph7_class_instance *)pVal->x.pOther;
}
static int RitState(ph7_vm *pVm,ph7_class_instance *pThis,int iLevel)
{
	ph7_value *pVal = RitAt(pVm,pThis,RIT_SS,iLevel);
	return pVal ? (int)ph7_value_to_int64(pVal) : RS_START;
}
static void RitSetState(ph7_vm *pVm,ph7_class_instance *pThis,int iLevel,int iState)
{
	ph7_value sVal;
	PH7_MemObjInitFromInt(pVm,&sVal,(sxi64)iState);
	RitPut(pVm,pThis,RIT_SS,iLevel,&sVal);
	PH7_MemObjRelease(&sVal);
}
/* php's `iterators = erealloc(…, ++level+1)` plus the two field writes. */
static void RitPush(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_instance *pChild)
{
	int iLevel = RitInt(pThis,RIT_LVL) + 1;
	ph7_value sObj;
	PH7_MemObjInit(pVm,&sObj);
	sObj.x.pOther = pChild;
	MemObjSetType(&sObj,MEMOBJ_OBJ);
	/* The map takes its OWN reference through the store; the carrier is blanked
	 * rather than released, because releasing a MEMOBJ_OBJ carrier would unref an
	 * instance this frame never referenced (rule 16). */
	RitPut(pVm,pThis,RIT_ST,iLevel,&sObj);
	sObj.x.pOther = 0;
	MemObjSetType(&sObj,MEMOBJ_NULL);
	PH7_MemObjRelease(&sObj);
	RitSetState(pVm,pThis,iLevel,RS_START);
	PH7_NativeSetAttrInt(pVm,pThis,RIT_LVL,iLevel);
}
static void RitPop(ph7_vm *pVm,ph7_class_instance *pThis)
{
	int iLevel = RitInt(pThis,RIT_LVL);
	if( iLevel <= 0 ){
		return;
	}
	RitErase(pVm,pThis,RIT_ST,iLevel);
	RitErase(pVm,pThis,RIT_SS,iLevel);
	PH7_NativeSetAttrInt(pVm,pThis,RIT_LVL,iLevel-1);
}
/* Drop every level: php's spl_RecursiveIteratorIterator_free_iterators. */
static void RitClear(ph7_vm *pVm,ph7_class_instance *pThis)
{
	int iLevel = RitInt(pThis,RIT_LVL);
	while( iLevel >= 0 ){
		RitErase(pVm,pThis,RIT_ST,iLevel);
		RitErase(pVm,pThis,RIT_SS,iLevel);
		iLevel--;
	}
	PH7_NativeSetAttrInt(pVm,pThis,RIT_LVL,0);
}
/*
 * Call a method, optionally SWALLOWING what it throws -- php clears the exception
 * at four sites when RIT_CATCH_GET_CHILD is set, and PH7_VmCallMethodSwallow is
 * the only way to spell that here (a throw raised under a C call site is
 * dispatched INLINE, so an enclosing user catch would run before this returns).
 * *pbThrew reports a swallowed throw, which php reads back as "retval is UNDEF".
 */
static sxi32 RitCall(ph7_context *pCtx,ph7_class_instance *pObj,const char *zName,sxu32 nName,
	ph7_value *pOut,int bCatch,int *pbThrew)
{
	ph7_class_method *pMethod = pObj ? PH7_ClassExtractMethod(pObj->pClass,zName,nName) : 0;
	if( pbThrew ){
		*pbThrew = FALSE;
	}
	if( pMethod == 0 ){
		return SXRET_OK;
	}
	if( bCatch ){
		return PH7_VmCallMethodSwallow(pCtx->pVm,pObj,pMethod,pOut,0,0,pbThrew);
	}
	return PH7_VmCallClassMethod(pCtx->pVm,pObj,pMethod,pOut,0,0);
}
/*
 * A hook on $this. php caches which of the seven the SUBCLASS overrides and calls
 * the sub-iterator directly when none does; dispatching through $this every time
 * reaches the same body -- the base ones are the no-ops php would have skipped --
 * with the override found automatically.
 */
static sxi32 RitHook(ph7_context *pCtx,const char *zName,sxu32 nName,ph7_value *pOut,
	int bCatch,int *pbThrew)
{
	return RitCall(pCtx,PH7_ContextThis(pCtx),zName,nName,pOut,bCatch,pbThrew);
}
static int RitCatches(ph7_class_instance *pThis)
{
	return (RitInt(pThis,RIT_FL) & RIT_CATCH_GET_CHILD) != 0;
}
/*
 * php's spl_recursive_it_move_forward_ex, transcribed. The switch's fallthroughs
 * (RS_NEXT into RS_START into RS_TEST) are written as a sequential if-chain, and
 * php's `goto next_step` is this loop's `continue`.
 */
static sxi32 RitMoveForward(ph7_context *pCtx)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	int bCatch;
	if( !RitReady(pThis) ){
		return RitNotReady(pCtx);
	}
	bCatch = RitCatches(pThis);
	for(;;){
		ph7_class_instance *pSub;
		int iLevel = RitInt(pThis,RIT_LVL);
		int iState = RitState(pVm,pThis,iLevel);
		int bThrew = 0;
		int bExhausted = 0;
		sxi32 rc;
		pSub = RitSub(pVm,pThis,iLevel);
		if( pSub == 0 ){
			return PH7_OK;
		}
		if( iState == RS_NEXT ){
			rc = RitCall(pCtx,pSub,"next",sizeof("next")-1,0,bCatch,&bThrew);
			if( rc != SXRET_OK ){
				return rc;
			}
			pSub = RitSub(pVm,pThis,iLevel);   /* the call may have moved aMemObj */
			if( pSub == 0 ){
				return PH7_OK;
			}
			iState = RS_START;                 /* php's fallthrough */
		}
		if( iState == RS_START ){
			ph7_value sValid;
			PH7_MemObjInit(pVm,&sValid);
			rc = RitCall(pCtx,pSub,"valid",sizeof("valid")-1,&sValid,FALSE,0);
			if( rc != SXRET_OK ){
				PH7_MemObjRelease(&sValid);
				return rc;
			}
			bExhausted = !ph7_value_to_bool(&sValid);
			PH7_MemObjRelease(&sValid);
			if( !bExhausted ){
				/* php re-reads the level here and returns outright when the valid()
				 * call RE-ENTERED this iterator (a sub-iterator that drove the
				 * decorator behind its back); the stack it was walking is gone. */
				if( RitInt(pThis,RIT_LVL) != iLevel || RitSub(pVm,pThis,iLevel) != pSub ){
					return PH7_OK;
				}
				RitSetState(pVm,pThis,iLevel,RS_TEST);
				iState = RS_TEST;
			}
		}
		if( !bExhausted && iState == RS_TEST ){
			ph7_value sHas;
			int bDescend = 0;
			PH7_MemObjInit(pVm,&sHas);
			rc = RitHook(pCtx,"callHasChildren",sizeof("callHasChildren")-1,&sHas,bCatch,&bThrew);
			if( rc != SXRET_OK ){
				/* php leaves the level on RS_NEXT so a caught-and-resumed traversal
				 * moves on rather than re-asking the same element. */
				RitSetState(pVm,pThis,iLevel,RS_NEXT);
				PH7_MemObjRelease(&sHas);
				return rc;
			}
			/* A SWALLOWED throw leaves php's retval UNDEF, which skips the
			 * has-children test entirely and yields the element. */
			if( !bThrew && ph7_value_to_bool(&sHas) ){
				int iMax = RitInt(pThis,RIT_MX);
				int iMode = RitInt(pThis,RIT_MD);
				if( iMax == -1 || iMax > iLevel ){
					/* php compares the mode EXACTLY: an unrecognized mode matches no
					 * arm, falls out of the switch and yields without descending. */
					if( iMode == RIT_LEAVES_ONLY || iMode == RIT_CHILD_FIRST ){
						RitSetState(pVm,pThis,iLevel,RS_CHILD);
						bDescend = 1;
					}else if( iMode == RIT_SELF_FIRST ){
						RitSetState(pVm,pThis,iLevel,RS_SELF);
						bDescend = 1;
					}
				}else if( iMode == RIT_LEAVES_ONLY ){
					/* Too deep to recurse into and NOT a leaf, so php skips it —
					 * the mode's defining rule, and the one the chunk dropped. */
					RitSetState(pVm,pThis,iLevel,RS_NEXT);
					bDescend = 1;
				}
			}
			PH7_MemObjRelease(&sHas);
			if( bDescend ){
				continue;                      /* php's goto next_step */
			}
			rc = RitHook(pCtx,"nextElement",sizeof("nextElement")-1,0,bCatch,&bThrew);
			RitSetState(pVm,pThis,iLevel,RS_NEXT);
			if( rc != SXRET_OK ){
				return rc;
			}
			return PH7_OK;                     /* yield this element */
		}
		if( !bExhausted && iState == RS_SELF ){
			int iMode = RitInt(pThis,RIT_MD);
			if( iMode == RIT_SELF_FIRST || iMode == RIT_CHILD_FIRST ){
				rc = RitHook(pCtx,"nextElement",sizeof("nextElement")-1,0,bCatch,&bThrew);
				if( rc != SXRET_OK ){
					return rc;
				}
			}
			RitSetState(pVm,pThis,iLevel,iMode == RIT_SELF_FIRST ? RS_CHILD : RS_NEXT);
			return PH7_OK;                     /* yield this element */
		}
		if( !bExhausted && iState == RS_CHILD ){
			ph7_class *pRecCls;
			ph7_class_instance *pChild;
			ph7_value sChild;
			int iMode = RitInt(pThis,RIT_MD);
			PH7_MemObjInit(pVm,&sChild);
			rc = RitHook(pCtx,"callGetChildren",sizeof("callGetChildren")-1,&sChild,bCatch,&bThrew);
			if( rc != SXRET_OK ){
				PH7_MemObjRelease(&sChild);
				return rc;
			}
			if( bThrew ){
				/* Caught: php drops the element and moves to the next one. */
				PH7_MemObjRelease(&sChild);
				RitSetState(pVm,pThis,iLevel,RS_NEXT);
				continue;
			}
			pRecCls = PH7_VmExtractClass(pVm,"RecursiveIterator",
				sizeof("RecursiveIterator")-1,FALSE,0);
			pChild = (sChild.iFlags & MEMOBJ_OBJ) != 0
				? (ph7_class_instance *)sChild.x.pOther : 0;
			if( pChild == 0 || (pRecCls && !PH7_VmInstanceOf(pChild->pClass,pRecCls)) ){
				PH7_MemObjRelease(&sChild);
				return PH7_VmThrowException(pCtx,"UnexpectedValueException",
					"Objects returned by RecursiveIterator::getChildren() must implement RecursiveIterator");
			}
			pChild->iRef++;                    /* survive the release of the call result */
			PH7_MemObjRelease(&sChild);
			RitSetState(pVm,pThis,iLevel,iMode == RIT_CHILD_FIRST ? RS_SELF : RS_NEXT);
			RitPush(pVm,pThis,pChild);
			PH7_ClassInstanceUnref(pChild);    /* the level's slot holds it now */
			rc = RitCall(pCtx,pChild,"rewind",sizeof("rewind")-1,0,FALSE,0);
			if( rc != SXRET_OK ){
				return rc;
			}
			rc = RitHook(pCtx,"beginChildren",sizeof("beginChildren")-1,0,bCatch,&bThrew);
			if( rc != SXRET_OK ){
				return rc;
			}
			continue;                          /* php's goto next_step */
		}
		/* No more elements at this level. */
		if( iLevel <= 0 ){
			return PH7_OK;                     /* done completely */
		}
		/* php calls endChildren BEFORE the pop, so the hook sees the depth it is
		 * leaving rather than the one it lands on. */
		rc = RitHook(pCtx,"endChildren",sizeof("endChildren")-1,0,bCatch,&bThrew);
		if( rc != SXRET_OK ){
			return rc;
		}
		if( RitInt(pThis,RIT_LVL) > 0 && RitSub(pVm,pThis,RitInt(pThis,RIT_LVL)) == pSub ){
			RitPop(pVm,pThis);
		}
	}
}
/*
 * php's spl_recursive_it_valid_ex: ASK the levels, walking down from the current
 * one, and fire endIteration the first time the answer is no.
 */
static sxi32 RitValidEx(ph7_context *pCtx,int *pbValid)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	int iLevel = RitInt(pThis,RIT_LVL);
	sxi32 rc;
	*pbValid = FALSE;
	while( iLevel >= 0 ){
		ph7_class_instance *pSub = RitSub(pVm,pThis,iLevel);
		ph7_value sValid;
		int bOk;
		if( pSub == 0 ){
			iLevel--;
			continue;
		}
		PH7_MemObjInit(pVm,&sValid);
		rc = RitCall(pCtx,pSub,"valid",sizeof("valid")-1,&sValid,FALSE,0);
		if( rc != SXRET_OK ){
			PH7_MemObjRelease(&sValid);
			return rc;
		}
		bOk = ph7_value_to_bool(&sValid);
		PH7_MemObjRelease(&sValid);
		if( bOk ){
			*pbValid = TRUE;
			return PH7_OK;
		}
		iLevel--;
	}
	if( RitInt(pThis,RIT_II) ){
		rc = RitHook(pCtx,"endIteration",sizeof("endIteration")-1,0,FALSE,0);
		PH7_NativeSetAttrInt(pVm,pThis,RIT_II,0);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	PH7_NativeSetAttrInt(pVm,pThis,RIT_II,0);
	return PH7_OK;
}
static int vm_builtin_RecursiveIteratorIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pObj;
	ph7_class_instance *pHold = 0;
	ph7_class *pAggCls,*pRecCls,*pTravCls;
	sxi64 iMode = RIT_LEAVES_ONLY,iFlags = 0;
	sxi32 rc;
	if( pThis == 0 ){
		return PH7_OK;
	}
	/*
	 * php's ZPP here is "o|ll" -- a bare OBJECT -- while the stub declares
	 * `Traversable $iterator`, so the declared type and the refusal text disagree
	 * (rule 41's neighbour). The spec row carries the declared type for Reflection
	 * and this body words both refusals, which is why the method sits on
	 * azSelfChecked[].
	 */
	if( nArg < 1 || (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 || apArg[0]->x.pOther == 0 ){
		char zGiven[64];
		return PH7_VmThrowException(pCtx,"TypeError",
			"RecursiveIteratorIterator::__construct(): Argument #1 ($iterator) "
			"must be of type object, %s given",
			nArg < 1 ? "none" : VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));
	}
	if( nArg > 1 ){
		rc = PH7_IntArgResolve(pCtx,apArg[1],"RecursiveIteratorIterator::__construct",2,"$mode","int",&iMode);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	if( nArg > 2 ){
		rc = PH7_IntArgResolve(pCtx,apArg[2],"RecursiveIteratorIterator::__construct",3,"$flags","int",&iFlags);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	pObj = (ph7_class_instance *)apArg[0]->x.pOther;
	pAggCls = PH7_VmExtractClass(pVm,"IteratorAggregate",sizeof("IteratorAggregate")-1,FALSE,0);
	pRecCls = PH7_VmExtractClass(pVm,"RecursiveIterator",sizeof("RecursiveIterator")-1,FALSE,0);
	pTravCls = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,FALSE,0);
	/*
	 * php's spl_get_iterator_from_aggregate: ONE getIterator() and no more. An
	 * IteratorAggregate whose getIterator() answers another aggregate therefore
	 * fails the RecursiveIterator test below rather than being unwrapped further.
	 */
	if( pAggCls && PH7_VmInstanceOf(pObj->pClass,pAggCls) ){
		ph7_class_method *pMethod = PH7_ClassExtractMethod(pObj->pClass,"getIterator",
			sizeof("getIterator")-1);
		ph7_value sInner;
		PH7_MemObjInit(pVm,&sInner);
		rc = pMethod ? PH7_VmCallClassMethod(pVm,pObj,pMethod,&sInner,0,0) : SXRET_OK;
		if( rc != SXRET_OK ){
			PH7_MemObjRelease(&sInner);
			return rc;
		}
		if( (sInner.iFlags & MEMOBJ_OBJ) == 0 || sInner.x.pOther == 0
		 || (pTravCls && !PH7_VmInstanceOf(((ph7_class_instance *)sInner.x.pOther)->pClass,pTravCls)) ){
			SyString *pName = &pObj->pClass->sName;
			PH7_MemObjRelease(&sInner);
			return PH7_VmThrowException(pCtx,"LogicException",
				"%z::getIterator() must return an object that implements Traversable",pName);
		}
		pObj = (ph7_class_instance *)sInner.x.pOther;
		pObj->iRef++;
		PH7_MemObjRelease(&sInner);
		pHold = pObj;
	}
	if( pRecCls == 0 || !PH7_VmInstanceOf(pObj->pClass,pRecCls) ){
		if( pHold ){
			PH7_ClassInstanceUnref(pHold);
		}
		/* php refuses here rather than from the declared type, so a plain Iterator
		 * gets this sentence and not a TypeError. */
		return PH7_VmThrowException(pCtx,"InvalidArgumentException",
			"An instance of RecursiveIterator or IteratorAggregate creating it is required");
	}
	RitClear(pVm,pThis);
	PH7_NativeSetAttrInt(pVm,pThis,RIT_LVL,0);
	PH7_NativeSetAttrInt(pVm,pThis,RIT_MD,iMode);
	PH7_NativeSetAttrInt(pVm,pThis,RIT_FL,iFlags);
	PH7_NativeSetAttrInt(pVm,pThis,RIT_MX,-1);
	PH7_NativeSetAttrInt(pVm,pThis,RIT_II,0);
	{
		ph7_value sObj;
		PH7_MemObjInit(pVm,&sObj);
		sObj.x.pOther = pObj;
		MemObjSetType(&sObj,MEMOBJ_OBJ);
		RitPut(pVm,pThis,RIT_ST,0,&sObj);
		sObj.x.pOther = 0;
		MemObjSetType(&sObj,MEMOBJ_NULL);
		PH7_MemObjRelease(&sObj);
	}
	RitSetState(pVm,pThis,0,RS_START);
	/* Level 0 exists from HERE, which is what makes getDepth() answer 0 and
	 * getSubIterator() answer the root before any rewind(). */
	PH7_NativeSetAttrInt(pVm,pThis,RIT_RD,1);
	if( pHold ){
		PH7_ClassInstanceUnref(pHold);
	}
	SXUNUSED(nArg);
	return PH7_OK;
}
static int vm_builtin_RecursiveIteratorIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pRoot;
	sxi32 rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !RitReady(pThis) ){
		return RitNotReady(pCtx);
	}
	/* php pops the level FIRST and calls endChildren after, so the hook reports the
	 * depth it has landed on -- the opposite order from the traversal's own pop. */
	while( RitInt(pThis,RIT_LVL) > 0 ){
		RitPop(pVm,pThis);
		rc = RitHook(pCtx,"endChildren",sizeof("endChildren")-1,0,FALSE,0);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	RitSetState(pVm,pThis,0,RS_START);
	pRoot = RitSub(pVm,pThis,0);
	rc = RitCall(pCtx,pRoot,"rewind",sizeof("rewind")-1,0,FALSE,0);
	if( rc != SXRET_OK ){
		return rc;
	}
	/* php's in_iteration latch: a second rewind() does NOT re-announce the
	 * iteration, which is the only reason the flag exists. */
	if( !RitInt(pThis,RIT_II) ){
		rc = RitHook(pCtx,"beginIteration",sizeof("beginIteration")-1,0,FALSE,0);
		if( rc != SXRET_OK ){
			PH7_NativeSetAttrInt(pVm,pThis,RIT_II,1);
			return rc;
		}
	}
	PH7_NativeSetAttrInt(pVm,pThis,RIT_II,1);
	return RitMoveForward(pCtx);
}
static int vm_builtin_RecursiveIteratorIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int bValid = FALSE;
	sxi32 rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !RitReady(PH7_ContextThis(pCtx)) ){
		return RitNotReady(pCtx);
	}
	rc = RitValidEx(pCtx,&bValid);
	if( rc != SXRET_OK ){
		return rc;
	}
	ph7_result_bool(pCtx,bValid);
	return PH7_OK;
}
/* current() and key() read the CURRENT LEVEL live -- php keeps no cache here, the
 * one place the recursive iterator differs from every dual iterator (rule 43). */
static sxi32 RitCurrentLevelCall(ph7_context *pCtx,const char *zName,sxu32 nName)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pSub;
	ph7_value sRes;
	sxi32 rc;
	if( !RitReady(pThis) ){
		return RitNotReady(pCtx);
	}
	pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));
	if( pSub == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_MemObjInit(pVm,&sRes);
	rc = RitCall(pCtx,pSub,zName,nName,&sRes,FALSE,0);
	if( rc == SXRET_OK ){
		ph7_result_value(pCtx,&sRes);
	}
	PH7_MemObjRelease(&sRes);
	return rc == SXRET_OK ? PH7_OK : rc;
}
static int vm_builtin_RecursiveIteratorIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return RitCurrentLevelCall(pCtx,"key",sizeof("key")-1);
}
static int vm_builtin_RecursiveIteratorIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return RitCurrentLevelCall(pCtx,"current",sizeof("current")-1);
}
static int vm_builtin_RecursiveIteratorIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !RitReady(PH7_ContextThis(pCtx)) ){
		return RitNotReady(pCtx);
	}
	return RitMoveForward(pCtx);
}
static int vm_builtin_RecursiveIteratorIterator_getDepth(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !RitReady(pThis) ){
		return RitNotReady(pCtx);
	}
	ph7_result_int64(pCtx,(ph7_int64)RitInt(pThis,RIT_LVL));
	return PH7_OK;
}
static int vm_builtin_RecursiveIteratorIterator_getSubIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pSub;
	int iLevel;
	if( !RitReady(pThis) ){
		return RitNotReady(pCtx);
	}
	iLevel = RitInt(pThis,RIT_LVL);
	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){
		sxi64 iWant = 0;
		sxi32 rc = PH7_IntArgResolve(pCtx,apArg[0],"RecursiveIteratorIterator::getSubIterator",
			1,"$level","?int",&iWant);
		if( rc != SXRET_OK ){
			return rc;
		}
		if( iWant < 0 || iWant > (sxi64)iLevel ){
			ph7_result_null(pCtx);
			return PH7_OK;
		}
		iLevel = (int)iWant;
	}
	pSub = RitSub(pVm,pThis,iLevel);
	if( pSub ){
		SplResultBorrowed(pCtx,pSub);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
static int vm_builtin_RecursiveIteratorIterator_getInnerIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pSub;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !RitReady(pThis) ){
		return RitNotReady(pCtx);
	}
	pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));
	if( pSub ){
		SplResultBorrowed(pCtx,pSub);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
/* The five hooks php declares with empty bodies. They exist to be OVERRIDDEN and
 * to be reachable through parent:: from an override. */
static int vm_builtin_RecursiveIteratorIterator_nop(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !RitReady(PH7_ContextThis(pCtx)) ){
		return RitNotReady(pCtx);
	}
	return PH7_OK;
}
/* php's callHasChildren/callGetChildren ask the CURRENT LEVEL's iterator, which
 * is what makes them the documented interception point for both. */
static int vm_builtin_RecursiveIteratorIterator_callHasChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pSub;
	ph7_value sRes;
	sxi32 rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !RitReady(pThis) ){
		return RitNotReady(pCtx);
	}
	pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));
	if( pSub == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	PH7_MemObjInit(pVm,&sRes);
	rc = RitCall(pCtx,pSub,"hasChildren",sizeof("hasChildren")-1,&sRes,FALSE,0);
	if( rc == SXRET_OK ){
		ph7_result_bool(pCtx,ph7_value_to_bool(&sRes));
	}
	PH7_MemObjRelease(&sRes);
	return rc == SXRET_OK ? PH7_OK : rc;
}
static int vm_builtin_RecursiveIteratorIterator_callGetChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pSub;
	ph7_value sRes;
	sxi32 rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !RitReady(pThis) ){
		return RitNotReady(pCtx);
	}
	pSub = RitSub(pVm,pThis,RitInt(pThis,RIT_LVL));
	if( pSub == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_MemObjInit(pVm,&sRes);
	rc = RitCall(pCtx,pSub,"getChildren",sizeof("getChildren")-1,&sRes,FALSE,0);
	if( rc == SXRET_OK ){
		ph7_result_value(pCtx,&sRes);
	}
	PH7_MemObjRelease(&sRes);
	return rc == SXRET_OK ? PH7_OK : rc;
}
static int vm_builtin_RecursiveIteratorIterator_setMaxDepth(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi64 iMax = -1;
	if( !RitReady(pThis) ){
		return RitNotReady(pCtx);
	}
	if( nArg > 0 ){
		sxi32 rc = PH7_IntArgResolve(pCtx,apArg[0],"RecursiveIteratorIterator::setMaxDepth",
			1,"$maxDepth","int",&iMax);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	if( iMax < -1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"RecursiveIteratorIterator::setMaxDepth(): Argument #1 ($maxDepth) "
			"must be greater than or equal to -1");
	}
	if( iMax > SXI32_HIGH ){
		iMax = SXI32_HIGH;   /* php clamps to INT_MAX; max_depth is an int there */
	}
	PH7_NativeSetAttrInt(pVm,pThis,RIT_MX,iMax);
	return PH7_OK;
}
static int vm_builtin_RecursiveIteratorIterator_getMaxDepth(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	int iMax;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !RitReady(pThis) ){
		return RitNotReady(pCtx);
	}
	iMax = RitInt(pThis,RIT_MX);
	if( iMax == -1 ){
		ph7_result_bool(pCtx,0);   /* php's `int|false`: false means "any depth" */
	}else{
		ph7_result_int64(pCtx,(ph7_int64)iMax);
	}
	return PH7_OK;
}
/*
 * The declaration. Method ORDER follows spl_iterators.stub.php line for line,
 * because that is the order Reflection reports. Every return type is php's
 * `@tentative-return-type` kind (rule 45).
 */
static sxi32 VmInstallSplRecursiveIt(ph7_vm *pVm)
{
	static const PH7_NativePropDef aRitProp[] = {
		{ RIT_ST,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ RIT_SS,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ RIT_LVL, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
		{ RIT_MD,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
		{ RIT_FL,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
		{ RIT_MX,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, -1, 0, 0.0 }, 0 },
		{ RIT_II,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
		{ RIT_RD,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeConstDef aRitConst[] = {
		{ "LEAVES_ONLY",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RIT_LEAVES_ONLY, 0, 0.0 },
		{ "SELF_FIRST",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RIT_SELF_FIRST, 0, 0.0 },
		{ "CHILD_FIRST",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RIT_CHILD_FIRST, 0, 0.0 },
		{ "CATCH_GET_CHILD", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, RIT_CATCH_GET_CHILD, 0, 0.0 },
	};
	static const PH7_NativeMethodDef aRitMethod[] = {
		{ "__construct",      PH7_MOD_PUBLIC,
		  /* php's stub spells the default `RecursiveIteratorIterator::LEAVES_ONLY`;
		   * one zSig field cannot carry both the TEXT and the VALUE, and the value
		   * wins here for the same reason it does on RegexIterator's row. */
		  "Traversable $iterator, int $mode = 0, int $flags = 0", 0,
		  vm_builtin_RecursiveIteratorIterator_construct },
		{ "rewind",           PH7_MOD_PUBLIC, "", "@void",
		  vm_builtin_RecursiveIteratorIterator_rewind },
		{ "valid",            PH7_MOD_PUBLIC, "", "@bool",
		  vm_builtin_RecursiveIteratorIterator_valid },
		{ "key",              PH7_MOD_PUBLIC, "", "@mixed",
		  vm_builtin_RecursiveIteratorIterator_key },
		{ "current",          PH7_MOD_PUBLIC, "", "@mixed",
		  vm_builtin_RecursiveIteratorIterator_current },
		{ "next",             PH7_MOD_PUBLIC, "", "@void",
		  vm_builtin_RecursiveIteratorIterator_next },
		{ "getDepth",         PH7_MOD_PUBLIC, "", "@int",
		  vm_builtin_RecursiveIteratorIterator_getDepth },
		{ "getSubIterator",   PH7_MOD_PUBLIC, "?int $level = null", "@?RecursiveIterator",
		  vm_builtin_RecursiveIteratorIterator_getSubIterator },
		{ "getInnerIterator", PH7_MOD_PUBLIC, "", "@RecursiveIterator",
		  vm_builtin_RecursiveIteratorIterator_getInnerIterator },
		{ "beginIteration",   PH7_MOD_PUBLIC, "", "@void",
		  vm_builtin_RecursiveIteratorIterator_nop },
		{ "endIteration",     PH7_MOD_PUBLIC, "", "@void",
		  vm_builtin_RecursiveIteratorIterator_nop },
		{ "callHasChildren",  PH7_MOD_PUBLIC, "", "@bool",
		  vm_builtin_RecursiveIteratorIterator_callHasChildren },
		{ "callGetChildren",  PH7_MOD_PUBLIC, "", "@?RecursiveIterator",
		  vm_builtin_RecursiveIteratorIterator_callGetChildren },
		{ "beginChildren",    PH7_MOD_PUBLIC, "", "@void",
		  vm_builtin_RecursiveIteratorIterator_nop },
		{ "endChildren",      PH7_MOD_PUBLIC, "", "@void",
		  vm_builtin_RecursiveIteratorIterator_nop },
		{ "nextElement",      PH7_MOD_PUBLIC, "", "@void",
		  vm_builtin_RecursiveIteratorIterator_nop },
		{ "setMaxDepth",      PH7_MOD_PUBLIC, "int $maxDepth = -1", "@void",
		  vm_builtin_RecursiveIteratorIterator_setMaxDepth },
		{ "getMaxDepth",      PH7_MOD_PUBLIC, "", "@int|false",
		  vm_builtin_RecursiveIteratorIterator_getMaxDepth },
	};
	/* PH7_CLASS_NOCLONE: php refuses `clone` outright ("Trying to clone an
	 * uncloneable object"), and a slot-by-slot copy would share one level stack --
	 * and with it one cursor -- between two traversals. */
	static const PH7_NativeClassSpec aSpec[] = {
		{ "RecursiveIteratorIterator", 0, "OuterIterator", PH7_CLASS_NOCLONE,
		  aRitMethod, SX_ARRAYSIZE(aRitMethod),
		  aRitConst, SX_ARRAYSIZE(aRitConst),
		  aRitProp, SX_ARRAYSIZE(aRitProp), 0, 0, 0 },
	};
	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
}
/*
 * ---------------------------------------------------------------------------
 * SplDoublyLinkedList, SplStack and SplQueue.
 *
 * php's `spl_dllist_object` is a linked list plus a FLAGS word, and the flags are
 * where the family's shape lives: SPL_DLLIST_IT_LIFO (2) and IT_DELETE (1) are the
 * iteration mode, and IT_FIX (4) is a bit no CONSTANT names and no user can set --
 * the object handler stamps it at creation for SplStack and SplQueue, and it is
 * both what freezes their LIFO/FIFO choice and why `(new SplStack)->getIteratorMode()`
 * answers 6 rather than 2. The chunk had no notion of it, so both classes reported
 * the wrong mode and `setIteratorMode()` reported the wrong result.
 *
 * Two rules follow from php's own code and neither is guessable from the methods:
 *
 *   - **every ArrayAccess offset is measured from the END of a LIFO list.**
 *     php resolves them through `spl_ptr_llist_offset(llist, offset, flags & LIFO)`,
 *     so `$stack[0]` is the element `top()` answers, not the one `bottom()` does.
 *     The chunk indexed the backing array directly and had the whole SplStack
 *     subscript surface reversed.
 *   - **the traverse POSITION is the list index in both modes.** A LIFO rewind
 *     seeds `count-1` and counts down, a FIFO rewind seeds 0 and counts up, so
 *     `key()` and the element's own place in the list agree either way -- except
 *     under IT_DELETE in FIFO order, where php consumes the head and deliberately
 *     does NOT advance the position (every element reports key 0).
 *
 * `toArray()` was a PHL INVENTION -- php has no such method on any of the three --
 * and it is gone. What php has instead, and the chunk had none of: `__debugInfo()`,
 * the `Serializable` interface with its `serialize()`/`unserialize()` pair, and the
 * `__serialize()`/`__unserialize()` pair that php actually uses (which is why the
 * serialized form is `O:19:"SplDoublyLinkedList":3:{i:0;…}` and not a property dump).
 *
 * The store is a php array in a hidden slot, head->tail, so push/pop/shift/unshift
 * are the engine's OWN array builtins called with the slot (rule 7, and rule 39's
 * reference rule already lives inside them). php's element-POINTER cursor is not
 * modelled: a manual walk that mutates the list under itself resolves by position
 * here and by identity there. That is one probe line (§7.4) and the only one.
 */
#define DLL_Q  "__q"   /* php's llist, head -> tail */
#define DLL_FL "__fl"  /* php's flags word, IT_FIX included */
#define DLL_I  "__i"   /* php's traverse_position */

#define DLL_IT_DELETE 1
#define DLL_IT_LIFO   2
#define DLL_IT_FIX    4   /* php's SPL_DLLIST_IT_FIX: stamped at creation, never by a user */
#define DLL_IT_MASK   3

static ph7_value * DllSlot(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,DLL_Q) : 0;
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
static ph7_hashmap * DllMap(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_value *pSlot = DllSlot(pVm,pThis);
	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;
}
static sxi64 DllCount(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_hashmap *pMap = DllMap(pVm,pThis);
	return pMap ? (sxi64)pMap->nEntry : 0;
}
static int DllFlags(ph7_class_instance *pThis)
{
	return pThis ? (int)PH7_NativeAttrInt(pThis,DLL_FL) : 0;
}
/* The value at a LIST index (head = 0), or NULL. */
static ph7_value * DllAt(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 iIndex)
{
	ph7_hashmap *pMap = DllMap(pVm,pThis);
	ph7_hashmap_node *pNode = 0;
	if( pMap == 0 || HashmapLookupIntKey(pMap,iIndex,&pNode) != SXRET_OK ){
		return 0;
	}
	return HashmapExtractNodeValue(pNode);
}
/*
 * php's spl_ptr_llist_offset: an ArrayAccess offset counts from the TAIL when the
 * list iterates LIFO. Every offsetGet/offsetSet/offsetUnset/add goes through here.
 */
static sxi64 DllOffsetToIndex(ph7_class_instance *pThis,sxi64 iOffset,sxi64 nCount)
{
	if( DllFlags(pThis) & DLL_IT_LIFO ){
		return nCount - 1 - iOffset;
	}
	return iOffset;
}
/* Hand one of the engine's own array builtins this instance's storage slot. */
static int DllArrayCall(ph7_context *pCtx,ProchHostFunction xFunc,ph7_value **apExtra,int nExtra)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *apCall[4];
	ph7_value *pSlot = DllSlot(pCtx->pVm,pThis);
	int i;
	if( pSlot == 0 ){
		return PH7_OK;
	}
	apCall[0] = pSlot;
	for( i = 0 ; i < nExtra && i < 3 ; ++i ){
		apCall[i+1] = apExtra[i];
	}
	return xFunc(pCtx,nExtra+1,apCall);
}
/* php's four "empty datastructure" refusals, which differ only in the verb. */
static sxi32 DllEmpty(ph7_context *pCtx,const char *zVerb)
{
	return PH7_VmThrowException(pCtx,"RuntimeException",
		"Can't %s an empty datastructure",zVerb);
}
/*
 * php words every out-of-range offset from the DECLARING class, not the runtime
 * one: `SplStack::add()` on an out-of-range index still says
 * `SplDoublyLinkedList::add()`. The chunk used get_class($this) and reported the
 * subclass.
 */
static sxi32 DllOutOfRange(ph7_context *pCtx,const char *zMethod)
{
	return PH7_VmThrowException(pCtx,"OutOfRangeException",
		"SplDoublyLinkedList::%s(): Argument #1 ($index) is out of range",zMethod);
}
/*
 * php's ZPP for the four ArrayAccess offsets and add(): the stub leaves `$index`
 * UNTYPED (which is what Reflection prints) while the ZPP is Z_PARAM_LONG, whose
 * TypeError says `must be of type int`. An untyped signature is not screened
 * centrally, so the rule is applied here — rule 41's disagreement, resolved without
 * an azSelfChecked[] row because the declared type is absent rather than different.
 */
static sxi32 DllIndexArg(ph7_context *pCtx,const char *zMethod,ph7_value *pArg,sxi64 *piOut)
{
	char zFunc[64];
	SyBufferFormat(zFunc,sizeof(zFunc),"SplDoublyLinkedList::%s",zMethod);
	return PH7_IntArgResolve(pCtx,pArg,zFunc,1,"$index","int",piOut);
}
static int vm_builtin_SplDll_push(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *apExtra[1];
	if( nArg < 1 ){
		return PH7_OK;
	}
	apExtra[0] = apArg[0];
	DllArrayCall(pCtx,ph7_hashmap_push,apExtra,1);
	ph7_result_null(pCtx);   /* array_push answers the new count; php's push is void */
	return PH7_OK;
}
static int vm_builtin_SplDll_unshift(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *apExtra[1];
	if( nArg < 1 ){
		return PH7_OK;
	}
	apExtra[0] = apArg[0];
	DllArrayCall(pCtx,ph7_hashmap_unshift,apExtra,1);
	ph7_result_null(pCtx);
	return PH7_OK;
}
static int vm_builtin_SplDll_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( DllCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0 ){
		return DllEmpty(pCtx,"pop from");
	}
	return DllArrayCall(pCtx,ph7_hashmap_pop,0,0);
}
static int vm_builtin_SplDll_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( DllCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0 ){
		return DllEmpty(pCtx,"shift from");
	}
	return DllArrayCall(pCtx,ph7_hashmap_shift,0,0);
}
/* top() is the TAIL and bottom() the HEAD, whatever the iteration mode: php reads
 * llist->tail/llist->head directly and never consults the flags here. */
static int vm_builtin_SplDll_top(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi64 nCount = DllCount(pCtx->pVm,pThis);
	ph7_value *pVal;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( nCount == 0 ){
		return DllEmpty(pCtx,"peek at");
	}
	pVal = DllAt(pCtx->pVm,pThis,nCount-1);
	if( pVal ){
		ph7_result_value(pCtx,pVal);
	}
	return PH7_OK;
}
static int vm_builtin_SplDll_bottom(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pVal;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( DllCount(pCtx->pVm,pThis) == 0 ){
		return DllEmpty(pCtx,"peek at");
	}
	pVal = DllAt(pCtx->pVm,pThis,0);
	if( pVal ){
		ph7_result_value(pCtx,pVal);
	}
	return PH7_OK;
}
static int vm_builtin_SplDll_count(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_int64(pCtx,DllCount(pCtx->pVm,PH7_ContextThis(pCtx)));
	return PH7_OK;
}
static int vm_builtin_SplDll_isEmpty(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx,DllCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0);
	return PH7_OK;
}
static int vm_builtin_SplDll_setIteratorMode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	int iFlags = DllFlags(pThis);
	sxi64 iMode = 0;
	sxi32 rc;
	if( pThis == 0 ){
		return PH7_OK;
	}
	if( nArg < 1 ){
		return PH7_OK;   /* the arity screen already refused */
	}
	rc = PH7_IntArgResolve(pCtx,apArg[0],"SplDoublyLinkedList::setIteratorMode",1,"$mode","int",&iMode);
	if( rc != SXRET_OK ){
		return rc;
	}
	if( (iFlags & DLL_IT_FIX) && (iFlags & DLL_IT_LIFO) != ((int)iMode & DLL_IT_LIFO) ){
		return PH7_VmThrowException(pCtx,"RuntimeException",
			"Iterators' LIFO/FIFO modes for SplStack/SplQueue objects are frozen");
	}
	/* php MASKS the value to the two mode bits and re-adds IT_FIX, so a nonsense
	 * mode is silently reduced rather than refused — and the ANSWER is the stored
	 * word, which is how a caller sees the fix bit at all. */
	iFlags = ((int)iMode & DLL_IT_MASK) | (iFlags & DLL_IT_FIX);
	PH7_NativeSetAttrInt(pVm,pThis,DLL_FL,iFlags);
	ph7_result_int64(pCtx,(ph7_int64)iFlags);
	return PH7_OK;
}
static int vm_builtin_SplDll_getIteratorMode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_int64(pCtx,(ph7_int64)DllFlags(PH7_ContextThis(pCtx)));
	return PH7_OK;
}
static int vm_builtin_SplDll_add(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi64 nCount = DllCount(pVm,pThis);
	sxi64 iIndex = 0;
	ph7_value sOff,sLen,sRep,*apExtra[3];
	ph7_hashmap *pRep;
	sxi32 rc;
	if( nArg < 2 ){
		return PH7_OK;
	}
	rc = DllIndexArg(pCtx,"add",apArg[0],&iIndex);
	if( rc != SXRET_OK ){
		return rc;
	}
	if( iIndex < 0 || iIndex > nCount ){
		return DllOutOfRange(pCtx,"add");
	}
	if( iIndex == nCount ){
		/* php: "the last entry + 1" is a push, because there is nothing to insert
		 * before. Note this is the LIST tail in both modes. */
		ph7_value *apOne[1];
		apOne[0] = apArg[1];
		DllArrayCall(pCtx,ph7_hashmap_push,apOne,1);
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pRep = PH7_NewHashmap(pVm,0,0);
	if( pRep == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_MemObjInit(pVm,&sRep);
	sRep.x.pOther = pRep;
	MemObjSetType(&sRep,MEMOBJ_HASHMAP);
	PH7_HashmapInsert(pRep,0,apArg[1]);
	PH7_MemObjInitFromInt(pVm,&sOff,DllOffsetToIndex(pThis,iIndex,nCount));
	PH7_MemObjInitFromInt(pVm,&sLen,0);
	apExtra[0] = &sOff;
	apExtra[1] = &sLen;
	apExtra[2] = &sRep;
	DllArrayCall(pCtx,ph7_hashmap_splice,apExtra,3);
	PH7_MemObjRelease(&sOff);
	PH7_MemObjRelease(&sLen);
	PH7_MemObjRelease(&sRep);
	ph7_result_null(pCtx);   /* array_splice answers what it removed; add() is void */
	return PH7_OK;
}
static int vm_builtin_SplDll_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi64 iIndex = 0;
	sxi32 rc;
	if( nArg < 1 ){
		return PH7_OK;
	}
	rc = DllIndexArg(pCtx,"offsetExists",apArg[0],&iIndex);
	if( rc != SXRET_OK ){
		return rc;
	}
	ph7_result_bool(pCtx,iIndex >= 0 && iIndex < DllCount(pCtx->pVm,PH7_ContextThis(pCtx)));
	return PH7_OK;
}
static int vm_builtin_SplDll_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi64 nCount = DllCount(pCtx->pVm,pThis);
	sxi64 iIndex = 0;
	ph7_value *pVal;
	sxi32 rc;
	if( nArg < 1 ){
		return PH7_OK;
	}
	rc = DllIndexArg(pCtx,"offsetGet",apArg[0],&iIndex);
	if( rc != SXRET_OK ){
		return rc;
	}
	if( iIndex < 0 || iIndex >= nCount ){
		return DllOutOfRange(pCtx,"offsetGet");
	}
	pVal = DllAt(pCtx->pVm,pThis,DllOffsetToIndex(pThis,iIndex,nCount));
	if( pVal ){
		ph7_result_value(pCtx,pVal);
	}
	return PH7_OK;
}
static int vm_builtin_SplDll_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi64 nCount = DllCount(pVm,pThis);
	sxi64 iIndex = 0;
	ph7_hashmap *pMap;
	ph7_hashmap_node *pNode = 0;
	ph7_value sKey;
	sxi32 rc;
	if( nArg < 2 ){
		return PH7_OK;
	}
	if( apArg[0]->iFlags & MEMOBJ_NULL ){
		/* php: a null offset is `$dll[] = v`, which pushes. */
		ph7_value *apOne[1];
		apOne[0] = apArg[1];
		DllArrayCall(pCtx,ph7_hashmap_push,apOne,1);
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	rc = DllIndexArg(pCtx,"offsetSet",apArg[0],&iIndex);
	if( rc != SXRET_OK ){
		return rc;
	}
	if( iIndex < 0 || iIndex >= nCount ){
		return DllOutOfRange(pCtx,"offsetSet");
	}
	pMap = DllMap(pVm,pThis);
	PH7_MemObjInitFromInt(pVm,&sKey,DllOffsetToIndex(pThis,iIndex,nCount));
	if( pMap && PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK ){
		ph7_value *pDest = HashmapExtractNodeValue(pNode);
		if( pDest ){
			PH7_MemObjStore(apArg[1],pDest);
		}
	}
	PH7_MemObjRelease(&sKey);
	return PH7_OK;
}
static int vm_builtin_SplDll_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi64 nCount = DllCount(pVm,pThis);
	sxi64 iIndex = 0;
	ph7_value sOff,sLen,*apExtra[2];
	sxi32 rc;
	if( nArg < 1 ){
		return PH7_OK;
	}
	rc = DllIndexArg(pCtx,"offsetUnset",apArg[0],&iIndex);
	if( rc != SXRET_OK ){
		return rc;
	}
	if( iIndex < 0 || iIndex >= nCount ){
		return DllOutOfRange(pCtx,"offsetUnset");
	}
	PH7_MemObjInitFromInt(pVm,&sOff,DllOffsetToIndex(pThis,iIndex,nCount));
	PH7_MemObjInitFromInt(pVm,&sLen,1);
	apExtra[0] = &sOff;
	apExtra[1] = &sLen;
	DllArrayCall(pCtx,ph7_hashmap_splice,apExtra,2);
	PH7_MemObjRelease(&sOff);
	PH7_MemObjRelease(&sLen);
	ph7_result_null(pCtx);
	return PH7_OK;
}
/*
 * The cursor. php's traverse_position IS the list index in both directions — a
 * LIFO rewind seeds count-1 and counts down — so current() and key() need no mode
 * test at all. IT_DELETE is the exception: in FIFO order php consumes the head and
 * leaves the position alone, so every element of a consuming walk reports key 0.
 */
static int vm_builtin_SplDll_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 ){
		return PH7_OK;
	}
	PH7_NativeSetAttrInt(pVm,pThis,DLL_I,
		(DllFlags(pThis) & DLL_IT_LIFO) ? DllCount(pVm,pThis)-1 : 0);
	return PH7_OK;
}
static int vm_builtin_SplDll_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi64 iPos = pThis ? PH7_NativeAttrInt(pThis,DLL_I) : 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx,iPos >= 0 && iPos < DllCount(pCtx->pVm,pThis));
	return PH7_OK;
}
static int vm_builtin_SplDll_current(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pVal = pThis ? DllAt(pCtx->pVm,pThis,PH7_NativeAttrInt(pThis,DLL_I)) : 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pVal ){
		ph7_result_value(pCtx,pVal);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
static int vm_builtin_SplDll_key(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_int64(pCtx,pThis ? PH7_NativeAttrInt(pThis,DLL_I) : 0);
	return PH7_OK;
}
/* php's move_forward, with the direction flipped for prev() (its `flags ^ LIFO`). */
static int DllStep(ph7_context *pCtx,int bFlip)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	int iFlags;
	sxi64 iPos;
	if( pThis == 0 ){
		return PH7_OK;
	}
	iFlags = DllFlags(pThis);
	if( bFlip ){
		iFlags ^= DLL_IT_LIFO;
	}
	iPos = PH7_NativeAttrInt(pThis,DLL_I);
	if( iPos < 0 || iPos >= DllCount(pVm,pThis) ){
		/* php only steps a LIVE pointer; off the end nothing moves and nothing is
		 * consumed. The position still has to move for a plain walk, though, or
		 * prev() past the head could never come back. */
		if( (iFlags & DLL_IT_DELETE) == 0 ){
			PH7_NativeSetAttrInt(pVm,pThis,DLL_I,
				iPos + ((iFlags & DLL_IT_LIFO) ? -1 : 1));
		}
		return PH7_OK;
	}
	if( iFlags & DLL_IT_DELETE ){
		if( iFlags & DLL_IT_LIFO ){
			DllArrayCall(pCtx,ph7_hashmap_pop,0,0);
			PH7_NativeSetAttrInt(pVm,pThis,DLL_I,iPos-1);
		}else{
			/* php consumes the head and does NOT advance: the walk stays at 0. */
			DllArrayCall(pCtx,ph7_hashmap_shift,0,0);
		}
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_NativeSetAttrInt(pVm,pThis,DLL_I,iPos + ((iFlags & DLL_IT_LIFO) ? -1 : 1));
	return PH7_OK;
}
static int vm_builtin_SplDll_next(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return DllStep(pCtx,FALSE);
}
static int vm_builtin_SplDll_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return DllStep(pCtx,TRUE);
}
/*
 * php's get_debug_info: `flags` then `dllist`, and NOTHING for the (array) cast --
 * the same var_dump/cast disagreement WeakReference has, which is why xPresent is
 * told which surface is asking. `__debugInfo()` is the same array, reachable by
 * name because php declares it.
 */
static sxi32 DllFillDebug(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)
{
	ph7_value sKey,sVal,*pStore;
	PH7_MemObjInitFromString(pVm,&sKey,0);
	PH7_MemObjStringAppend(&sKey,"flags",sizeof("flags")-1);
	PH7_MemObjInitFromInt(pVm,&sVal,DllFlags(pThis));
	ph7_array_add_elem(pOut,&sKey,&sVal);
	PH7_MemObjRelease(&sKey);
	PH7_MemObjRelease(&sVal);
	pStore = DllSlot(pVm,pThis);
	PH7_MemObjInitFromString(pVm,&sKey,0);
	PH7_MemObjStringAppend(&sKey,"dllist",sizeof("dllist")-1);
	if( pStore ){
		ph7_array_add_elem(pOut,&sKey,pStore);
	}
	PH7_MemObjRelease(&sKey);
	return PH7_OK;
}
static sxi32 DllPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)
{
	if( !bDebug ){
		return PH7_OK;   /* php's (array) cast and var_export show nothing */
	}
	return DllFillDebug(pVm,pThis,pOut);
}
static int vm_builtin_SplDll_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value sOut;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	PH7_MemObjInit(pVm,&sOut);
	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){
		PH7_MemObjRelease(&sOut);
		return PH7_ContextMemoryError(pCtx);
	}
	DllFillDebug(pVm,pThis,&sOut);
	ph7_result_value(pCtx,&sOut);
	PH7_MemObjRelease(&sOut);
	return PH7_OK;
}
/*
 * php's __serialize(): [flags, elements, dynamic members]. This is what
 * serialize() actually uses -- the Serializable pair below exists because the
 * interface is still declared, and php words its own legacy format there.
 */
static int vm_builtin_SplDll_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value sOut,sVal,*pStore;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	PH7_MemObjInit(pVm,&sOut);
	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){
		PH7_MemObjRelease(&sOut);
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_MemObjInitFromInt(pVm,&sVal,DllFlags(pThis));
	ph7_array_add_elem(&sOut,0,&sVal);
	PH7_MemObjRelease(&sVal);
	pStore = DllSlot(pVm,pThis);
	if( pStore ){
		ph7_array_add_elem(&sOut,0,pStore);
	}
	/* The members slot: php hands back the dynamic properties, and a native class
	 * has none that are php-visible (every declared slot is hidden). */
	PH7_MemObjInit(pVm,&sVal);
	if( PH7_MemObjToHashmap(&sVal) == SXRET_OK ){
		ph7_array_add_elem(&sOut,0,&sVal);
	}
	PH7_MemObjRelease(&sVal);
	ph7_result_value(pCtx,&sOut);
	PH7_MemObjRelease(&sOut);
	return PH7_OK;
}
static int vm_builtin_SplDll_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_hashmap *pData;
	ph7_hashmap_node *pNode = 0;
	ph7_value *pFlags,*pStore,*pSlot;
	if( nArg < 1 || (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 || pThis == 0 ){
		return PH7_VmThrowException(pCtx,"UnexpectedValueException",
			"Incomplete or ill-typed serialization data");
	}
	pData = (ph7_hashmap *)apArg[0]->x.pOther;
	if( HashmapLookupIntKey(pData,0,&pNode) != SXRET_OK ){
		return PH7_VmThrowException(pCtx,"UnexpectedValueException",
			"Incomplete or ill-typed serialization data");
	}
	pFlags = HashmapExtractNodeValue(pNode);
	if( pFlags == 0 || (pFlags->iFlags & MEMOBJ_INT) == 0
	 || HashmapLookupIntKey(pData,1,&pNode) != SXRET_OK ){
		return PH7_VmThrowException(pCtx,"UnexpectedValueException",
			"Incomplete or ill-typed serialization data");
	}
	pStore = HashmapExtractNodeValue(pNode);
	if( pStore == 0 || (pStore->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return PH7_VmThrowException(pCtx,"UnexpectedValueException",
			"Incomplete or ill-typed serialization data");
	}
	PH7_NativeSetAttrInt(pVm,pThis,DLL_FL,ph7_value_to_int64(pFlags));
	pSlot = PH7_NativeAttr(pThis,DLL_Q);
	if( pSlot ){
		PH7_MemObjRelease(pSlot);
		PH7_MemObjStore(pStore,pSlot);
	}
	return PH7_OK;
}
/*
 * php's Serializable pair, kept because the interface is still declared: the
 * format is the serialized FLAGS followed by one ':' + serialized value per
 * element ("i:0;:i:1;:i:2;"), which nothing else in php produces or reads.
 */
/*
 * One serialized value, appended to a blob. The RESET is the point: the engine's
 * serialize() writes through ph7_value_string, which APPENDS to the context's
 * return slot rather than replacing it, so a loop that calls it per element
 * accumulates every previous answer into the next one.
 */
static void DllSerializeInto(ph7_context *pCtx,ph7_value **apCall,SyBlob *pOut)
{
	int nLen = 0;
	const char *zTxt;
	if( pCtx->pRet ){
		PH7_MemObjRelease(pCtx->pRet);
	}
	vm_builtin_serialize(pCtx,1,apCall);
	if( pCtx->pRet == 0 ){
		return;
	}
	zTxt = ph7_value_to_string(pCtx->pRet,&nLen);
	SyBlobAppend(pOut,zTxt,(sxu32)nLen);
}
static int vm_builtin_SplDll_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_hashmap *pMap = DllMap(pVm,pThis);
	ph7_hashmap_node *pNode;
	SyBlob sOut;
	ph7_value sFlags,*apCall[1];
	sxi64 n,nCount = pMap ? (sxi64)pMap->nEntry : 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	SyBlobInit(&sOut,&pVm->sAllocator);
	PH7_MemObjInitFromInt(pVm,&sFlags,DllFlags(pThis));
	apCall[0] = &sFlags;
	DllSerializeInto(pCtx,apCall,&sOut);
	PH7_MemObjRelease(&sFlags);
	for( n = 0 ; n < nCount ; ++n ){
		ph7_value *pVal;
		pNode = 0;
		if( HashmapLookupIntKey(pMap,n,&pNode) != SXRET_OK ){
			continue;
		}
		pVal = HashmapExtractNodeValue(pNode);
		if( pVal == 0 ){
			continue;
		}
		apCall[0] = pVal;
		SyBlobAppend(&sOut,":",1);
		DllSerializeInto(pCtx,apCall,&sOut);
	}
	/* ph7_result_string APPENDS too, and pRet still holds the LAST element's
	 * serialization from the loop above — drop it before writing the answer. */
	if( pCtx->pRet ){
		PH7_MemObjRelease(pCtx->pRet);
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
static int vm_builtin_SplDll_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zData,*zCur,*zEnd;
	int nData = 0;
	int bFirst = 1;
	ph7_value *pSlot;
	ph7_hashmap *pMap;
	if( nArg < 1 || pThis == 0 ){
		return PH7_OK;
	}
	zData = ph7_value_to_string(apArg[0],&nData);
	if( nData < 1 ){
		return PH7_OK;   /* php returns without touching the list */
	}
	pSlot = DllSlot(pVm,pThis);
	pMap = pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;
	if( pMap == 0 ){
		return PH7_OK;
	}
	/* php empties the list first, then reads the flags, then one ':'-prefixed
	 * value per element. A malformed tail is an UnexpectedValueException naming
	 * the byte offset — reproduced here from the same position arithmetic. */
	while( pMap->pFirst ){
		PH7_HashmapUnlinkNode(pMap->pFirst,TRUE);
	}
	zCur = zData;
	zEnd = &zData[nData];
	while( zCur < zEnd ){
		ph7_value sPart,sRes,*apCall[1];
		int nPart;
		const char *zStop = zCur;
		if( !bFirst ){
			if( zCur[0] != ':' ){
				break;
			}
			zCur++;
		}
		/* One serialized scalar reaches up to and including its ';'. */
		while( zStop < zEnd && zStop[0] != ';' ){
			zStop++;
		}
		if( zStop >= zEnd ){
			zStop = zEnd;
		}else{
			zStop++;
		}
		nPart = (int)(zStop - zCur);
		if( nPart <= 0 ){
			break;
		}
		PH7_MemObjInitFromString(pVm,&sPart,0);
		PH7_MemObjStringAppend(&sPart,zCur,(sxu32)nPart);
		apCall[0] = &sPart;
		PH7_MemObjInit(pVm,&sRes);
		if( pCtx->pRet ){
			PH7_MemObjRelease(pCtx->pRet);   /* see DllSerializeInto: pRet is appended to */
		}
		vm_builtin_unserialize(pCtx,1,apCall);
		if( pCtx->pRet ){
			PH7_MemObjStore(pCtx->pRet,&sRes);
		}
		if( bFirst ){
			PH7_NativeSetAttrInt(pVm,pThis,DLL_FL,ph7_value_to_int64(&sRes));
			bFirst = 0;
		}else{
			PH7_HashmapInsert(pMap,0,&sRes);
		}
		PH7_MemObjRelease(&sPart);
		PH7_MemObjRelease(&sRes);
		zCur = zStop;
	}
	ph7_result_null(pCtx);
	return PH7_OK;
}
/*
 * The declaration. Method ORDER is spl_dllist.stub.php's, php declares NO
 * constructor for any of the three, and the IT_FIX bit is a per-class DEFAULT on
 * the flags slot -- which is exactly how php does it (the create handler stamps
 * the flags; there is no constructor to run).
 */
static sxi32 VmInstallSplDllist(ph7_vm *pVm)
{
	static const PH7_NativeMethodDef aDllMethod[] = {
		{ "add",             PH7_MOD_PUBLIC, "int $index, mixed $value", "@void",
		  vm_builtin_SplDll_add },
		{ "pop",             PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_pop },
		{ "shift",           PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_shift },
		{ "push",            PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplDll_push },
		{ "unshift",         PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplDll_unshift },
		{ "top",             PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_top },
		{ "bottom",          PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_bottom },
		{ "__debugInfo",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplDll_debugInfo },
		{ "count",           PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplDll_count },
		{ "isEmpty",         PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplDll_isEmpty },
		{ "setIteratorMode", PH7_MOD_PUBLIC, "int $mode", "@int",
		  vm_builtin_SplDll_setIteratorMode },
		{ "getIteratorMode", PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplDll_getIteratorMode },
		/* php's stub leaves these four offsets UNTYPED (a `@param int` docblock, which
		 * Reflection does not print) while the ZPP enforces int -- so the signature says
		 * nothing and each body runs PH7_IntArgResolve itself. */
		{ "offsetExists",    PH7_MOD_PUBLIC, "$index", "@bool", vm_builtin_SplDll_offsetExists },
		{ "offsetGet",       PH7_MOD_PUBLIC, "$index", "@mixed", vm_builtin_SplDll_offsetGet },
		{ "offsetSet",       PH7_MOD_PUBLIC, "$index, mixed $value", "@void",
		  vm_builtin_SplDll_offsetSet },
		{ "offsetUnset",     PH7_MOD_PUBLIC, "$index", "@void", vm_builtin_SplDll_offsetUnset },
		{ "rewind",          PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplDll_rewind },
		{ "current",         PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_current },
		{ "key",             PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplDll_key },
		{ "prev",            PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplDll_prev },
		{ "next",            PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplDll_next },
		{ "valid",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplDll_valid },
		{ "unserialize",     PH7_MOD_PUBLIC, "string $data", "@void",
		  vm_builtin_SplDll_unserialize },
		{ "serialize",       PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplDll_serialize },
		{ "__serialize",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplDll_serializeMagic },
		{ "__unserialize",   PH7_MOD_PUBLIC, "array $data", "@void",
		  vm_builtin_SplDll_unserializeMagic },
	};
	static const PH7_NativeMethodDef aQueueMethod[] = {
		/* php's @implementation-alias: the same C bodies under the queue's names. */
		{ "enqueue", PH7_MOD_PUBLIC, "mixed $value", "@void", vm_builtin_SplDll_push },
		{ "dequeue", PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplDll_shift },
	};
	static const PH7_NativeConstDef aDllConst[] = {
		{ "IT_MODE_LIFO",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, DLL_IT_LIFO, 0, 0.0 },
		{ "IT_MODE_FIFO",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 0, 0, 0.0 },
		{ "IT_MODE_DELETE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, DLL_IT_DELETE, 0, 0.0 },
		{ "IT_MODE_KEEP",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 0, 0, 0.0 },
	};
	static const PH7_NativePropDef aDllProp[] = {
		{ DLL_Q,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ DLL_FL, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
		{ DLL_I,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
	};
	/* php's object handler stamps IT_FIX (and LIFO for a stack) at CREATION, which
	 * is why neither subclass declares a constructor and why the bit survives every
	 * setIteratorMode(). A per-class default on the flags slot says the same thing. */
	static const PH7_NativePropDef aQueueProp[] = {
		{ DLL_FL, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN,
		  { 0, 0, PH7_NATIVE_VAL_INT, DLL_IT_FIX, 0, 0.0 }, 0 },
	};
	static const PH7_NativePropDef aStackProp[] = {
		{ DLL_FL, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN,
		  { 0, 0, PH7_NATIVE_VAL_INT, DLL_IT_FIX|DLL_IT_LIFO, 0, 0.0 }, 0 },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "SplDoublyLinkedList", 0, "Iterator,Countable,ArrayAccess,Serializable", 0,
		  aDllMethod, SX_ARRAYSIZE(aDllMethod),
		  aDllConst, SX_ARRAYSIZE(aDllConst),
		  aDllProp, SX_ARRAYSIZE(aDllProp), 0, 0, DllPresent },
		{ "SplQueue", "SplDoublyLinkedList", 0, 0,
		  aQueueMethod, SX_ARRAYSIZE(aQueueMethod), 0, 0,
		  aQueueProp, SX_ARRAYSIZE(aQueueProp), 0, 0, DllPresent },
		{ "SplStack", "SplDoublyLinkedList", 0, 0,
		  0, 0, 0, 0,
		  aStackProp, SX_ARRAYSIZE(aStackProp), 0, 0, DllPresent },
	};
	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
}
/*
 * ---------------------------------------------------------------------------
 * SplHeap, SplMinHeap, SplMaxHeap and SplPriorityQueue.
 *
 * php's `spl_heap_object` is an array plus a FLAGS word, and the flags carry the
 * thing the chunk could not express at all: **SPL_HEAP_CORRUPTED**. php sets it
 * when an exception escapes the user's compare() mid-sift -- the heap invariant is
 * then unknown -- and every operation that DEPENDS on the invariant refuses with
 * "Heap is corrupted, heap properties are no longer ensured." until
 * recoverFromCorruption() clears it. The chunk hardcoded `isCorrupted()` to false
 * and `recoverFromCorruption()` to true, so a throwing comparator left a silently
 * mis-ordered heap that kept answering.
 *
 * Which operations refuse is not guessable and was mapped against the oracle:
 * insert, extract, top, next and __serialize/__unserialize refuse; count,
 * isEmpty, rewind, valid, current, key, isCorrupted, recoverFromCorruption and
 * __debugInfo all keep working. (A `foreach` refuses because it reaches next().)
 *
 * php's priority-queue node is exactly {data, priority} -- the chunk carried a
 * third field, a descending `__serial` it never compared with, which leaked into
 * serialize(), var_dump() and the (array) cast as a nonsense PHP_INT_MAX-relative
 * integer. It is gone; equal priorities keep the order php's strictly-greater
 * swap gives them.
 *
 * The other three the chunk lacked, the same three the SplDoublyLinkedList
 * conversion lacked: `__debugInfo()` (flags / isCorrupted / heap, and for the queue
 * the heap entries are rendered EXTR_BOTH-style whatever the extract flags say),
 * and the `__serialize()`/`__unserialize()` pair, whose payload is
 * [members, {flags, heap_elements}] and whose reader VALIDATES -- a plain heap
 * refuses a non-zero flags word, the queue refuses a zero one.
 */
#define HP_H  "__h"   /* the heap array, in heap order */
#define HP_FL "__fl"  /* php's intern->flags: the queue's EXTR bits, 0 for a heap */
#define HP_CR "__cr"  /* php's SPL_HEAP_CORRUPTED */

#define PQ_EXTR_DATA     1
#define PQ_EXTR_PRIORITY 2
#define PQ_EXTR_BOTH     3
#define PQ_EXTR_MASK     3

static ph7_value * HeapSlot(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,HP_H) : 0;
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
static ph7_hashmap * HeapMap(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_value *pSlot = HeapSlot(pVm,pThis);
	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;
}
static sxi64 HeapCount(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_hashmap *pMap = HeapMap(pVm,pThis);
	return pMap ? (sxi64)pMap->nEntry : 0;
}
/* Re-resolved on every use: any call into the user's compare() may have moved
 * pVm->aMemObj under us (rule 47). */
static ph7_value * HeapAt(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i)
{
	ph7_hashmap *pMap = HeapMap(pVm,pThis);
	ph7_hashmap_node *pNode = 0;
	if( pMap == 0 || HashmapLookupIntKey(pMap,i,&pNode) != SXRET_OK ){
		return 0;
	}
	return HashmapExtractNodeValue(pNode);
}
static void HeapPut(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i,ph7_value *pVal)
{
	ph7_hashmap *pMap = HeapMap(pVm,pThis);
	ph7_value sKey;
	if( pMap == 0 ){
		return;
	}
	PH7_MemObjInitFromInt(pVm,&sKey,i);
	PH7_HashmapInsert(pMap,&sKey,pVal);
	PH7_MemObjRelease(&sKey);
}
static void HeapSwap(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i,sxi64 j)
{
	ph7_value sI,sJ,*pV;
	PH7_MemObjInit(pVm,&sI);
	PH7_MemObjInit(pVm,&sJ);
	pV = HeapAt(pVm,pThis,i);
	if( pV ){
		PH7_MemObjStore(pV,&sI);
	}
	pV = HeapAt(pVm,pThis,j);
	if( pV ){
		PH7_MemObjStore(pV,&sJ);
	}
	HeapPut(pVm,pThis,i,&sJ);
	HeapPut(pVm,pThis,j,&sI);
	PH7_MemObjRelease(&sI);
	PH7_MemObjRelease(&sJ);
}
static int HeapCorrupted(ph7_class_instance *pThis)
{
	return pThis ? (int)PH7_NativeAttrInt(pThis,HP_CR) : 0;
}
/*
 * php's spl_heap_consistency_validations. Only the operations that DEPEND on the
 * heap invariant call it -- count()/current()/key() answer from the array and are
 * left alone, which is why a corrupted heap still reports its size.
 */
static sxi32 HeapCheck(ph7_context *pCtx)
{
	if( !HeapCorrupted(PH7_ContextThis(pCtx)) ){
		return SXRET_OK;
	}
	return PH7_VmThrowException(pCtx,"RuntimeException",
		"Heap is corrupted, heap properties are no longer ensured.");
}
/* A priority-queue node is php's {data, priority}: nothing else, and in that order. */
static int HeapIsPq(ph7_class_instance *pThis)
{
	ph7_class *pPq;
	if( pThis == 0 ){
		return FALSE;
	}
	pPq = PH7_VmExtractClass(pThis->pVm,"SplPriorityQueue",sizeof("SplPriorityQueue")-1,FALSE,0);
	return pPq && PH7_VmInstanceOf(pThis->pClass,pPq);
}
static ph7_value * HeapNodePart(ph7_value *pNode,const char *zKey)
{
	ph7_hashmap *pMap;
	ph7_hashmap_node *pEnt = 0;
	if( pNode == 0 || (pNode->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return 0;
	}
	pMap = (ph7_hashmap *)pNode->x.pOther;
	if( HashmapLookupBlobKey(pMap,zKey,(sxu32)SyStrlen(zKey),&pEnt) != SXRET_OK ){
		return 0;
	}
	return HashmapExtractNodeValue(pEnt);
}
static sxi32 HeapMakeNode(ph7_vm *pVm,ph7_value *pData,ph7_value *pPrio,ph7_value *pOut)
{
	ph7_value sKey;
	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){
		return SXERR_MEM;
	}
	PH7_MemObjInitFromString(pVm,&sKey,0);
	PH7_MemObjStringAppend(&sKey,"data",sizeof("data")-1);
	ph7_array_add_elem(pOut,&sKey,pData);
	PH7_MemObjRelease(&sKey);
	PH7_MemObjInitFromString(pVm,&sKey,0);
	PH7_MemObjStringAppend(&sKey,"priority",sizeof("priority")-1);
	ph7_array_add_elem(pOut,&sKey,pPrio);
	PH7_MemObjRelease(&sKey);
	return SXRET_OK;
}
/*
 * Run the user's compare(). For a queue php compares the PRIORITIES, so the node's
 * `priority` is what is handed over. A throw here is php's corruption trigger: the
 * bit is set, and the throw still propagates.
 */
static sxi32 HeapCompare(ph7_context *pCtx,sxi64 iA,sxi64 iB,int *piCmp)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_method *pMethod;
	ph7_value sA,sB,sRes,*apArg[2],*pV;
	int bPq = HeapIsPq(pThis);
	sxi32 rc;
	*piCmp = 0;
	pMethod = pThis ? PH7_ClassExtractMethod(pThis->pClass,"compare",sizeof("compare")-1) : 0;
	if( pMethod == 0 ){
		return SXRET_OK;
	}
	PH7_MemObjInit(pVm,&sA);
	PH7_MemObjInit(pVm,&sB);
	pV = HeapAt(pVm,pThis,iA);
	if( bPq ){
		pV = HeapNodePart(pV,"priority");
	}
	if( pV ){
		PH7_MemObjStore(pV,&sA);
	}
	pV = HeapAt(pVm,pThis,iB);
	if( bPq ){
		pV = HeapNodePart(pV,"priority");
	}
	if( pV ){
		PH7_MemObjStore(pV,&sB);
	}
	apArg[0] = &sA;
	apArg[1] = &sB;
	PH7_MemObjInit(pVm,&sRes);
	/* php dispatches through its cached fptr_cmp and never consults visibility --
	 * SplHeap::compare() is PROTECTED and is meant to be called by the heap. */
	rc = PH7_VmCallMethodUnchecked(pVm,pThis,pMethod,&sRes,2,apArg);
	if( rc == SXRET_OK ){
		sxi64 iVal = ph7_value_to_int64(&sRes);
		*piCmp = iVal < 0 ? -1 : (iVal > 0 ? 1 : 0);
	}else{
		/* php finishes the sift with the exception in flight and marks the heap
		 * CORRUPTED afterwards; the element it was placing still lands. */
		PH7_NativeSetAttrInt(pVm,pThis,HP_CR,1);
	}
	PH7_MemObjRelease(&sA);
	PH7_MemObjRelease(&sB);
	PH7_MemObjRelease(&sRes);
	return rc;
}
static sxi32 HeapSiftUp(ph7_context *pCtx,sxi64 i)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	while( i > 0 ){
		sxi64 p = (i - 1) / 2;
		int iCmp = 0;
		sxi32 rc = HeapCompare(pCtx,i,p,&iCmp);
		if( rc != SXRET_OK ){
			return rc;
		}
		if( iCmp <= 0 ){
			break;
		}
		HeapSwap(pVm,pThis,i,p);
		i = p;
	}
	return SXRET_OK;
}
static sxi32 HeapSiftDown(ph7_context *pCtx,sxi64 i)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	for(;;){
		sxi64 n = HeapCount(pVm,pThis);
		sxi64 l = 2*i + 1, r = l + 1, b = i;
		int iCmp = 0;
		sxi32 rc;
		if( l < n ){
			rc = HeapCompare(pCtx,l,b,&iCmp);
			if( rc != SXRET_OK ){
				return rc;
			}
			if( iCmp > 0 ){
				b = l;
			}
		}
		if( r < n ){
			rc = HeapCompare(pCtx,r,b,&iCmp);
			if( rc != SXRET_OK ){
				return rc;
			}
			if( iCmp > 0 ){
				b = r;
			}
		}
		if( b == i ){
			break;
		}
		HeapSwap(pVm,pThis,i,b);
		i = b;
	}
	return SXRET_OK;
}
/* php's spl_pqueue_extract_helper: BOTH wins over either single bit. */
static void HeapPqShape(ph7_context *pCtx,ph7_value *pNode)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	int iFlags = pThis ? (int)PH7_NativeAttrInt(pThis,HP_FL) : PQ_EXTR_DATA;
	ph7_value *pPart;
	if( (iFlags & PQ_EXTR_BOTH) == PQ_EXTR_BOTH ){
		ph7_result_value(pCtx,pNode);
		return;
	}
	pPart = HeapNodePart(pNode,(iFlags & PQ_EXTR_DATA) ? "data" : "priority");
	if( pPart ){
		ph7_result_value(pCtx,pPart);
	}else{
		ph7_result_null(pCtx);
	}
}
/* Hand back element 0 the way this class presents it. */
static void HeapResultTop(ph7_context *pCtx,ph7_value *pNode)
{
	if( HeapIsPq(PH7_ContextThis(pCtx)) ){
		HeapPqShape(pCtx,pNode);
	}else{
		ph7_result_value(pCtx,pNode);
	}
}
static int vm_builtin_SplHeap_insert(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_hashmap *pMap;
	sxi32 rc = HeapCheck(pCtx);
	if( rc != SXRET_OK || nArg < 1 ){
		return rc;
	}
	pMap = HeapMap(pVm,pThis);
	if( pMap == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( HeapIsPq(pThis) ){
		ph7_value sNode;
		if( nArg < 2 ){
			return PH7_OK;
		}
		PH7_MemObjInit(pVm,&sNode);
		if( HeapMakeNode(pVm,apArg[0],apArg[1],&sNode) != SXRET_OK ){
			PH7_MemObjRelease(&sNode);
			return PH7_ContextMemoryError(pCtx);
		}
		PH7_HashmapInsert(pMap,0,&sNode);
		PH7_MemObjRelease(&sNode);
	}else{
		PH7_HashmapInsert(pMap,0,apArg[0]);
	}
	rc = HeapSiftUp(pCtx,HeapCount(pVm,pThis)-1);
	if( rc != SXRET_OK ){
		return rc;
	}
	ph7_result_bool(pCtx,1);   /* php's `true` return type */
	return PH7_OK;
}
static int vm_builtin_SplHeap_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi64 n;
	ph7_value sTop,*pV;
	sxi32 rc = HeapCheck(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( rc != SXRET_OK ){
		return rc;
	}
	n = HeapCount(pVm,pThis);
	if( n == 0 ){
		return PH7_VmThrowException(pCtx,"RuntimeException","Can't extract from an empty heap");
	}
	PH7_MemObjInit(pVm,&sTop);
	pV = HeapAt(pVm,pThis,0);
	if( pV ){
		PH7_MemObjStore(pV,&sTop);
	}
	if( n > 1 ){
		ph7_value sLast;
		PH7_MemObjInit(pVm,&sLast);
		pV = HeapAt(pVm,pThis,n-1);
		if( pV ){
			PH7_MemObjStore(pV,&sLast);
		}
		HeapPut(pVm,pThis,0,&sLast);
		PH7_MemObjRelease(&sLast);
	}
	{
		ph7_hashmap *pMap = HeapMap(pVm,pThis);
		ph7_hashmap_node *pNode = 0;
		if( pMap && HashmapLookupIntKey(pMap,n-1,&pNode) == SXRET_OK ){
			PH7_HashmapUnlinkNode(pNode,TRUE);
		}
	}
	if( n > 1 ){
		rc = HeapSiftDown(pCtx,0);
		if( rc != SXRET_OK ){
			PH7_MemObjRelease(&sTop);
			return rc;
		}
	}
	HeapResultTop(pCtx,&sTop);
	PH7_MemObjRelease(&sTop);
	return PH7_OK;
}
static int vm_builtin_SplHeap_top(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pV;
	sxi32 rc = HeapCheck(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( rc != SXRET_OK ){
		return rc;
	}
	if( HeapCount(pCtx->pVm,pThis) == 0 ){
		return PH7_VmThrowException(pCtx,"RuntimeException","Can't peek at an empty heap");
	}
	pV = HeapAt(pCtx->pVm,pThis,0);
	if( pV ){
		HeapResultTop(pCtx,pV);
	}
	return PH7_OK;
}
static int vm_builtin_SplHeap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_int64(pCtx,HeapCount(pCtx->pVm,PH7_ContextThis(pCtx)));
	return PH7_OK;
}
static int vm_builtin_SplHeap_isEmpty(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx,HeapCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0);
	return PH7_OK;
}
static int vm_builtin_SplHeap_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	SXUNUSED(pCtx);
	return PH7_OK;   /* php's rewind is a no-op: a heap is walked by extraction */
}
static int vm_builtin_SplHeap_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx,HeapCount(pCtx->pVm,PH7_ContextThis(pCtx)) > 0);
	return PH7_OK;
}
static int vm_builtin_SplHeap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pV;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( HeapCount(pCtx->pVm,pThis) == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pV = HeapAt(pCtx->pVm,pThis,0);
	if( pV ){
		HeapResultTop(pCtx,pV);
	}
	return PH7_OK;
}
static int vm_builtin_SplHeap_key(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_int64(pCtx,HeapCount(pCtx->pVm,PH7_ContextThis(pCtx))-1);
	return PH7_OK;
}
static int vm_builtin_SplHeap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc = HeapCheck(pCtx);
	if( rc != SXRET_OK ){
		return rc;
	}
	if( HeapCount(pCtx->pVm,PH7_ContextThis(pCtx)) == 0 ){
		return PH7_OK;
	}
	rc = vm_builtin_SplHeap_extract(pCtx,nArg,apArg);
	ph7_result_null(pCtx);   /* php's next() is void; the extracted value is dropped */
	return rc;
}
static int vm_builtin_SplHeap_isCorrupted(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx,HeapCorrupted(PH7_ContextThis(pCtx)));
	return PH7_OK;
}
static int vm_builtin_SplHeap_recover(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	PH7_NativeSetAttrInt(pCtx->pVm,PH7_ContextThis(pCtx),HP_CR,0);
	ph7_result_bool(pCtx,1);   /* php's `true` return type */
	return PH7_OK;
}
static int vm_builtin_SplMinHeap_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	/* php: $value2 <=> $value1 — the SMALLEST value sits on top. */
	if( nArg < 2 ){
		return PH7_OK;
	}
	ph7_result_int64(pCtx,(ph7_int64)PH7_MemObjCmp(apArg[1],apArg[0],FALSE,0));
	return PH7_OK;
}
static int vm_builtin_SplMaxHeap_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 2 ){
		return PH7_OK;
	}
	ph7_result_int64(pCtx,(ph7_int64)PH7_MemObjCmp(apArg[0],apArg[1],FALSE,0));
	return PH7_OK;
}
static int vm_builtin_SplPq_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 2 ){
		return PH7_OK;
	}
	ph7_result_int64(pCtx,(ph7_int64)PH7_MemObjCmp(apArg[0],apArg[1],FALSE,0));
	return PH7_OK;
}
static int vm_builtin_SplPq_setExtractFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi64 iFlags = 0;
	sxi32 rc;
	if( nArg < 1 ){
		return PH7_OK;
	}
	rc = PH7_IntArgResolve(pCtx,apArg[0],"SplPriorityQueue::setExtractFlags",1,"$flags","int",&iFlags);
	if( rc != SXRET_OK ){
		return rc;
	}
	/* php masks to the two bits and then REFUSES an empty selection — a nonsense
	 * value is reduced, but asking for neither half is an error. */
	iFlags &= PQ_EXTR_MASK;
	if( iFlags == 0 ){
		return PH7_VmThrowException(pCtx,"RuntimeException","Must specify at least one extract flag");
	}
	PH7_NativeSetAttrInt(pVm,pThis,HP_FL,iFlags);
	ph7_result_int64(pCtx,(ph7_int64)iFlags);
	return PH7_OK;
}
static int vm_builtin_SplPq_getExtractFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),HP_FL));
	return PH7_OK;
}
/*
 * php's get_debug_info: flags, isCorrupted, heap. The QUEUE renders its entries
 * EXTR_BOTH-style whatever the extract flags say, because the debug view is of the
 * STORAGE rather than of what extract() would hand back.
 */
static sxi32 HeapFillDebug(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)
{
	ph7_value sKey,sVal,*pStore;
	PH7_MemObjInitFromString(pVm,&sKey,0);
	PH7_MemObjStringAppend(&sKey,"flags",sizeof("flags")-1);
	PH7_MemObjInitFromInt(pVm,&sVal,PH7_NativeAttrInt(pThis,HP_FL));
	ph7_array_add_elem(pOut,&sKey,&sVal);
	PH7_MemObjRelease(&sKey);
	PH7_MemObjRelease(&sVal);
	PH7_MemObjInitFromString(pVm,&sKey,0);
	PH7_MemObjStringAppend(&sKey,"isCorrupted",sizeof("isCorrupted")-1);
	PH7_MemObjInitFromBool(pVm,&sVal,HeapCorrupted(pThis));
	ph7_array_add_elem(pOut,&sKey,&sVal);
	PH7_MemObjRelease(&sKey);
	PH7_MemObjRelease(&sVal);
	PH7_MemObjInitFromString(pVm,&sKey,0);
	PH7_MemObjStringAppend(&sKey,"heap",sizeof("heap")-1);
	pStore = HeapSlot(pVm,pThis);
	if( pStore ){
		ph7_array_add_elem(pOut,&sKey,pStore);
	}
	PH7_MemObjRelease(&sKey);
	return PH7_OK;
}
static sxi32 HeapPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)
{
	if( !bDebug ){
		return PH7_OK;   /* php's (array) cast shows nothing */
	}
	return HeapFillDebug(pVm,pThis,pOut);
}
static int vm_builtin_SplHeap_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value sOut;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	PH7_MemObjInit(pVm,&sOut);
	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){
		PH7_MemObjRelease(&sOut);
		return PH7_ContextMemoryError(pCtx);
	}
	HeapFillDebug(pVm,PH7_ContextThis(pCtx),&sOut);
	ph7_result_value(pCtx,&sOut);
	PH7_MemObjRelease(&sOut);
	return PH7_OK;
}
/*
 * php's __serialize(): [members, {flags, heap_elements}]. Note the OUTER array is
 * a two-element list whose first entry is the dynamic-property table — a native
 * class has none that are php-visible, so it is always empty here.
 */
static int vm_builtin_SplHeap_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value sOut,sMembers,sState,sKey,sVal,*pStore;
	sxi32 rc = HeapCheck(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( rc != SXRET_OK ){
		return rc;
	}
	PH7_MemObjInit(pVm,&sOut);
	PH7_MemObjInit(pVm,&sMembers);
	PH7_MemObjInit(pVm,&sState);
	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK
	 || PH7_MemObjToHashmap(&sMembers) != SXRET_OK
	 || PH7_MemObjToHashmap(&sState) != SXRET_OK ){
		PH7_MemObjRelease(&sOut);
		PH7_MemObjRelease(&sMembers);
		PH7_MemObjRelease(&sState);
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_MemObjInitFromString(pVm,&sKey,0);
	PH7_MemObjStringAppend(&sKey,"flags",sizeof("flags")-1);
	PH7_MemObjInitFromInt(pVm,&sVal,PH7_NativeAttrInt(pThis,HP_FL));
	ph7_array_add_elem(&sState,&sKey,&sVal);
	PH7_MemObjRelease(&sKey);
	PH7_MemObjRelease(&sVal);
	PH7_MemObjInitFromString(pVm,&sKey,0);
	PH7_MemObjStringAppend(&sKey,"heap_elements",sizeof("heap_elements")-1);
	pStore = HeapSlot(pVm,pThis);
	if( pStore ){
		ph7_array_add_elem(&sState,&sKey,pStore);
	}
	PH7_MemObjRelease(&sKey);
	ph7_array_add_elem(&sOut,0,&sMembers);
	ph7_array_add_elem(&sOut,0,&sState);
	ph7_result_value(pCtx,&sOut);
	PH7_MemObjRelease(&sOut);
	PH7_MemObjRelease(&sMembers);
	PH7_MemObjRelease(&sState);
	return PH7_OK;
}
static sxi32 HeapUnserializeFail(ph7_context *pCtx)
{
	return PH7_VmThrowException(pCtx,"UnexpectedValueException",
		"Unexpected data found in serialization payload");
}
static int vm_builtin_SplHeap_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_hashmap *pData;
	ph7_hashmap_node *pNode = 0;
	ph7_value *pState,*pFlags,*pElems,*pSlot;
	sxi64 iFlags;
	int bPq;
	sxi32 rc = HeapCheck(pCtx);
	if( rc != SXRET_OK ){
		return rc;
	}
	if( nArg < 1 || (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 || pThis == 0 ){
		return HeapUnserializeFail(pCtx);
	}
	pData = (ph7_hashmap *)apArg[0]->x.pOther;
	if( HashmapLookupIntKey(pData,1,&pNode) != SXRET_OK ){
		return HeapUnserializeFail(pCtx);
	}
	pState = HashmapExtractNodeValue(pNode);
	if( pState == 0 || (pState->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return HeapUnserializeFail(pCtx);
	}
	pFlags = HeapNodePart(pState,"flags");
	pElems = HeapNodePart(pState,"heap_elements");
	if( pFlags == 0 || (pFlags->iFlags & MEMOBJ_INT) == 0
	 || pElems == 0 || (pElems->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return HeapUnserializeFail(pCtx);
	}
	/* php VALIDATES the flags against the class: a plain heap has no user-visible
	 * flags at all, the queue must name at least one half to extract. */
	iFlags = ph7_value_to_int64(pFlags);
	bPq = HeapIsPq(pThis);
	if( bPq ){
		iFlags &= PQ_EXTR_MASK;
		if( iFlags == 0 ){
			return HeapUnserializeFail(pCtx);
		}
	}else if( iFlags != 0 ){
		return HeapUnserializeFail(pCtx);
	}
	PH7_NativeSetAttrInt(pVm,pThis,HP_FL,iFlags);
	pSlot = PH7_NativeAttr(pThis,HP_H);
	if( pSlot ){
		PH7_MemObjRelease(pSlot);
		PH7_MemObjStore(pElems,pSlot);
	}
	return PH7_OK;
}
/*
 * The declaration. Method ORDER is spl_heap.stub.php's; php declares no
 * constructor for any of the four, SplHeap::compare is ABSTRACT PROTECTED (so
 * SplHeap itself cannot be instantiated) while the queue's is PUBLIC, and the
 * queue's default extract mode is EXTR_DATA, stamped as a property default the
 * way the DLL family's fix bit is.
 */
static sxi32 VmInstallSplHeap(ph7_vm *pVm)
{
	static const PH7_NativePropDef aHeapProp[] = {
		{ HP_H,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ HP_FL, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
		{ HP_CR, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativePropDef aPqProp[] = {
		{ HP_H,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ HP_FL, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN,
		  { 0, 0, PH7_NATIVE_VAL_INT, PQ_EXTR_DATA, 0, 0.0 }, 0 },
		{ HP_CR, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeMethodDef aHeapMethod[] = {
		{ "extract",               PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_extract },
		{ "insert",                PH7_MOD_PUBLIC, "mixed $value", "@true",
		  vm_builtin_SplHeap_insert },
		{ "top",                   PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_top },
		{ "count",                 PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplHeap_count },
		{ "isEmpty",               PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_isEmpty },
		{ "rewind",                PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplHeap_rewind },
		{ "current",               PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_current },
		{ "key",                   PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplHeap_key },
		{ "next",                  PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplHeap_next },
		{ "valid",                 PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_valid },
		{ "recoverFromCorruption", PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplHeap_recover },
		{ "compare",               PH7_MOD_PROTECTED|PH7_MOD_ABSTRACT,
		  "mixed $value1, mixed $value2", "@int", 0 },
		{ "isCorrupted",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_isCorrupted },
		{ "__debugInfo",           PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplHeap_debugInfo },
		{ "__serialize",           PH7_MOD_PUBLIC, "", "@array",
		  vm_builtin_SplHeap_serializeMagic },
		{ "__unserialize",         PH7_MOD_PUBLIC, "array $data", "@void",
		  vm_builtin_SplHeap_unserializeMagic },
	};
	static const PH7_NativeMethodDef aMinMethod[] = {
		{ "compare", PH7_MOD_PROTECTED, "mixed $value1, mixed $value2", "@int",
		  vm_builtin_SplMinHeap_compare },
	};
	static const PH7_NativeMethodDef aMaxMethod[] = {
		{ "compare", PH7_MOD_PROTECTED, "mixed $value1, mixed $value2", "@int",
		  vm_builtin_SplMaxHeap_compare },
	};
	static const PH7_NativeConstDef aPqConst[] = {
		{ "EXTR_BOTH",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PQ_EXTR_BOTH, 0, 0.0 },
		{ "EXTR_PRIORITY", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PQ_EXTR_PRIORITY, 0, 0.0 },
		{ "EXTR_DATA",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PQ_EXTR_DATA, 0, 0.0 },
	};
	static const PH7_NativeMethodDef aPqMethod[] = {
		{ "compare",               PH7_MOD_PUBLIC, "mixed $priority1, mixed $priority2", "@int",
		  vm_builtin_SplPq_compare },
		{ "insert",                PH7_MOD_PUBLIC, "mixed $value, mixed $priority", "@true",
		  vm_builtin_SplHeap_insert },
		{ "setExtractFlags",       PH7_MOD_PUBLIC, "int $flags", "@int",
		  vm_builtin_SplPq_setExtractFlags },
		{ "top",                   PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_top },
		{ "extract",               PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_extract },
		{ "count",                 PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplHeap_count },
		{ "isEmpty",               PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_isEmpty },
		{ "rewind",                PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplHeap_rewind },
		{ "current",               PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplHeap_current },
		{ "key",                   PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplHeap_key },
		{ "next",                  PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplHeap_next },
		{ "valid",                 PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_valid },
		{ "recoverFromCorruption", PH7_MOD_PUBLIC, "", "@true", vm_builtin_SplHeap_recover },
		{ "isCorrupted",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplHeap_isCorrupted },
		{ "getExtractFlags",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplPq_getExtractFlags },
		{ "__debugInfo",           PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplHeap_debugInfo },
		{ "__serialize",           PH7_MOD_PUBLIC, "", "@array",
		  vm_builtin_SplHeap_serializeMagic },
		{ "__unserialize",         PH7_MOD_PUBLIC, "array $data", "@void",
		  vm_builtin_SplHeap_unserializeMagic },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "SplPriorityQueue", 0, "Iterator,Countable", 0,
		  aPqMethod, SX_ARRAYSIZE(aPqMethod),
		  aPqConst, SX_ARRAYSIZE(aPqConst),
		  aPqProp, SX_ARRAYSIZE(aPqProp), 0, 0, HeapPresent },
		{ "SplHeap", 0, "Iterator,Countable", PH7_CLASS_ABSTRACT,
		  aHeapMethod, SX_ARRAYSIZE(aHeapMethod), 0, 0,
		  aHeapProp, SX_ARRAYSIZE(aHeapProp), 0, 0, HeapPresent },
		{ "SplMinHeap", "SplHeap", 0, 0,
		  aMinMethod, SX_ARRAYSIZE(aMinMethod), 0, 0, 0, 0, 0, 0, HeapPresent },
		{ "SplMaxHeap", "SplHeap", 0, 0,
		  aMaxMethod, SX_ARRAYSIZE(aMaxMethod), 0, 0, 0, 0, 0, 0, HeapPresent },
	};
	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
}
/*
 * ---------------------------------------------------------------------------
 * SplFixedArray.
 *
 * php PRESENTS this one as its own elements: `var_dump` shows
 * `object(SplFixedArray)#1 (3) { [0]=> … }`, the `(array)` cast yields the
 * elements with their integer keys, and `serialize()` writes them as INTEGER
 * property names (`O:13:"SplFixedArray":3:{i:0;…}`) because `__serialize()`
 * simply hands the element array back. The chunk exposed `__a`/`__n` on all
 * three surfaces instead.
 *
 * `getIterator()` answers php's **InternalIterator**, not a Generator. The chunk
 * yielded, which is one class name wrong on a php-visible surface and also the
 * thing rule 5's InternalIterator exists for — `pIterVtab` plus
 * `PH7_NativeIteratorNew()` is the whole implementation, and it gets php's
 * independent-cursor behaviour (two getIterator() calls, or nested foreach, walk
 * separately) for free.
 *
 * php's offset rule is its own: an int, a bool and an INTEGER-LIKE string are
 * accepted, everything else is `Cannot access offset of type %s on SplFixedArray`.
 * The chunk refused bools. A FLOAT offset stays refused here, which is not php's
 * answer (php truncates, with a precision deprecation when it is lossy) but IS
 * PHL's engine-wide one — `$a[1.5]` on a plain array raises the same TypeError,
 * so the class stays consistent with the engine it lives in rather than uniquely
 * permissive (§10).
 */
#define FA_A "__a"   /* the elements, 0..n-1 */
#define FA_N "__n"   /* php's size */

static ph7_value * FaSlot(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,FA_A) : 0;
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
static ph7_hashmap * FaMap(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_value *pSlot = FaSlot(pVm,pThis);
	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;
}
static sxi64 FaSize(ph7_class_instance *pThis)
{
	return pThis ? PH7_NativeAttrInt(pThis,FA_N) : 0;
}
static ph7_value * FaAt(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i)
{
	ph7_hashmap *pMap = FaMap(pVm,pThis);
	ph7_hashmap_node *pNode = 0;
	if( pMap == 0 || HashmapLookupIntKey(pMap,i,&pNode) != SXRET_OK ){
		return 0;
	}
	return HashmapExtractNodeValue(pNode);
}
static void FaPut(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 i,ph7_value *pVal)
{
	ph7_hashmap *pMap = FaMap(pVm,pThis);
	ph7_value sKey;
	if( pMap == 0 ){
		return;
	}
	PH7_MemObjInitFromInt(pVm,&sKey,i);
	PH7_HashmapInsert(pMap,&sKey,pVal);
	PH7_MemObjRelease(&sKey);
}
/*
 * php's offset decode. An INTEGER-LIKE string is accepted (php's own
 * `ZEND_HANDLE_NUMERIC_STRING`), a bool is its 0/1, and every other type is named
 * in the refusal. Returns 0 and leaves a TypeError raised when it cannot decode.
 */
static int FaOffset(ph7_context *pCtx,ph7_value *pArg,sxi64 *piOut,sxi32 *pRc)
{
	*pRc = PH7_OK;
	if( pArg == 0 ){
		*piOut = 0;
		return 1;
	}
	if( pArg->iFlags & MEMOBJ_INT ){
		*piOut = pArg->x.iVal;
		return 1;
	}
	if( pArg->iFlags & MEMOBJ_BOOL ){
		*piOut = pArg->x.iVal ? 1 : 0;
		return 1;
	}
	if( (pArg->iFlags & MEMOBJ_STRING) && PH7_MemObjStringIsNumeric(pArg) ){
		ph7_value sTmp;
		PH7_MemObjInit(pCtx->pVm,&sTmp);
		PH7_MemObjStore(pArg,&sTmp);
		PH7_MemObjToInteger(&sTmp);
		*piOut = sTmp.x.iVal;
		PH7_MemObjRelease(&sTmp);
		return 1;
	}
	*piOut = 0;
	*pRc = PH7_VmThrowException(pCtx,"TypeError",
		"Cannot access offset of type %s on SplFixedArray",ph7_type_name(pArg));
	return 0;
}
static sxi32 FaOutOfBounds(ph7_context *pCtx)
{
	return PH7_VmThrowException(pCtx,"OutOfBoundsException","Index invalid or out of range");
}
/* php's setSize: grow with nulls, shrink by dropping the tail, answer `true`. */
static sxi32 FaResize(ph7_vm *pVm,ph7_class_instance *pThis,sxi64 nNew)
{
	sxi64 nOld = FaSize(pThis);
	ph7_hashmap *pMap = FaMap(pVm,pThis);
	sxi64 i;
	if( pMap == 0 ){
		return SXERR_MEM;
	}
	for( i = nNew ; i < nOld ; ++i ){
		ph7_hashmap_node *pNode = 0;
		if( HashmapLookupIntKey(pMap,i,&pNode) == SXRET_OK ){
			PH7_HashmapUnlinkNode(pNode,TRUE);
		}
	}
	for( i = nOld ; i < nNew ; ++i ){
		ph7_value sNull;
		PH7_MemObjInit(pVm,&sNull);
		FaPut(pVm,pThis,i,&sNull);
		PH7_MemObjRelease(&sNull);
	}
	PH7_NativeSetAttrInt(pVm,pThis,FA_N,nNew);
	return SXRET_OK;
}
static int vm_builtin_SplFixedArray_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi64 nSize = 0;
	if( pThis == 0 ){
		return PH7_OK;
	}
	if( nArg > 0 ){
		sxi32 rc = PH7_IntArgResolve(pCtx,apArg[0],"SplFixedArray::__construct",1,"$size","int",&nSize);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	if( nSize < 0 ){
		/* php words this from __construct(), not from the setSize() it forwards to —
		 * which is what the chunk's `$this->setSize()` reported. */
		return PH7_VmThrowException(pCtx,"ValueError",
			"SplFixedArray::__construct(): Argument #1 ($size) must be greater than or equal to 0");
	}
	FaResize(pVm,pThis,nSize);
	return PH7_OK;
}
static int vm_builtin_SplFixedArray_getSize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_int64(pCtx,FaSize(PH7_ContextThis(pCtx)));
	return PH7_OK;
}
static int vm_builtin_SplFixedArray_setSize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi64 nSize = 0;
	sxi32 rc;
	if( nArg < 1 ){
		return PH7_OK;
	}
	rc = PH7_IntArgResolve(pCtx,apArg[0],"SplFixedArray::setSize",1,"$size","int",&nSize);
	if( rc != SXRET_OK ){
		return rc;
	}
	if( nSize < 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"SplFixedArray::setSize(): Argument #1 ($size) must be greater than or equal to 0");
	}
	FaResize(pCtx->pVm,PH7_ContextThis(pCtx),nSize);
	ph7_result_bool(pCtx,1);   /* php's `true` return type */
	return PH7_OK;
}
static int vm_builtin_SplFixedArray_count(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_int64(pCtx,FaSize(PH7_ContextThis(pCtx)));
	return PH7_OK;
}
static int vm_builtin_SplFixedArray_toArray(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pSlot = FaSlot(pCtx->pVm,PH7_ContextThis(pCtx));
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pSlot ){
		ph7_result_value(pCtx,pSlot);
	}
	return PH7_OK;
}
static int vm_builtin_SplFixedArray_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi64 iIdx = 0;
	sxi32 rc = PH7_OK;
	ph7_value *pVal;
	if( nArg < 1 ){
		return PH7_OK;
	}
	/* offsetExists RAISES for an undecodable offset exactly as the other three do —
	 * `isset($f['x'])` is a TypeError, not a false — and answers false only for a
	 * decodable index that is out of range or holds null. */
	if( !FaOffset(pCtx,apArg[0],&iIdx,&rc) ){
		return rc;
	}
	if( iIdx < 0 || iIdx >= FaSize(pThis) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* php's isset() semantics: an unset slot holds null and is NOT set. */
	pVal = FaAt(pCtx->pVm,pThis,iIdx);
	ph7_result_bool(pCtx,pVal != 0 && (pVal->iFlags & MEMOBJ_NULL) == 0);
	return PH7_OK;
}
static int vm_builtin_SplFixedArray_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi64 iIdx = 0;
	sxi32 rc = PH7_OK;
	ph7_value *pVal;
	if( nArg < 1 ){
		return PH7_OK;
	}
	if( !FaOffset(pCtx,apArg[0],&iIdx,&rc) ){
		return rc;
	}
	if( iIdx < 0 || iIdx >= FaSize(pThis) ){
		return FaOutOfBounds(pCtx);
	}
	pVal = FaAt(pCtx->pVm,pThis,iIdx);
	if( pVal ){
		ph7_result_value(pCtx,pVal);
	}
	return PH7_OK;
}
static int vm_builtin_SplFixedArray_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi64 iIdx = 0;
	sxi32 rc = PH7_OK;
	if( nArg < 2 ){
		return PH7_OK;
	}
	if( !FaOffset(pCtx,apArg[0],&iIdx,&rc) ){
		return rc;
	}
	if( iIdx < 0 || iIdx >= FaSize(pThis) ){
		return FaOutOfBounds(pCtx);
	}
	FaPut(pCtx->pVm,pThis,iIdx,apArg[1]);
	return PH7_OK;
}
static int vm_builtin_SplFixedArray_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi64 iIdx = 0;
	sxi32 rc = PH7_OK;
	ph7_value sNull;
	if( nArg < 1 ){
		return PH7_OK;
	}
	if( !FaOffset(pCtx,apArg[0],&iIdx,&rc) ){
		return rc;
	}
	if( iIdx < 0 || iIdx >= FaSize(pThis) ){
		return FaOutOfBounds(pCtx);
	}
	/* The slot survives at its index and becomes null: the array is FIXED. */
	PH7_MemObjInit(pVm,&sNull);
	FaPut(pVm,pThis,iIdx,&sNull);
	PH7_MemObjRelease(&sNull);
	return PH7_OK;
}
static int vm_builtin_SplFixedArray_fromArray(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pCls;
	ph7_class_instance *pNew;
	ph7_hashmap *pSrc;
	ph7_hashmap_node *pNode,*pPrev;
	int bPreserve = 1;
	sxi64 nMax = -1, nNext = 0;
	if( nArg < 1 || (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"SplFixedArray::fromArray(): Argument #1 ($array) must be of type array, %s given",
			nArg < 1 ? "none" : ph7_type_name(apArg[0]));
	}
	if( nArg > 1 ){
		bPreserve = ph7_value_to_bool(apArg[1]);
	}
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* php walks the keys FIRST and refuses the whole call before building anything. */
	if( bPreserve ){
		for( pNode = pSrc->pFirst ; pNode ; pNode = pPrev ){
			pPrev = pNode->pPrev;
			if( pNode->iType != HASHMAP_INT_NODE || pNode->xKey.iKey < 0 ){
				return PH7_VmThrowException(pCtx,"InvalidArgumentException",
					"array must contain only positive integer keys");
			}
			if( pNode->xKey.iKey > nMax ){
				nMax = pNode->xKey.iKey;
			}
			if( pNode == pSrc->pFirst && pPrev == 0 ){
				break;
			}
		}
	}
	pCls = PH7_VmExtractClass(pVm,"SplFixedArray",sizeof("SplFixedArray")-1,FALSE,0);
	if( pCls == 0 ){
		return PH7_OK;
	}
	pNew = PH7_NewClassInstance(pVm,pCls);
	if( pNew == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	pNew->iRef++;
	FaResize(pVm,pNew,bPreserve ? nMax + 1 : (sxi64)pSrc->nEntry);
	for( pNode = pSrc->pFirst ; pNode ; pNode = pPrev ){
		ph7_value *pVal = HashmapExtractNodeValue(pNode);
		pPrev = pNode->pPrev;
		if( pVal ){
			FaPut(pVm,pNew,bPreserve ? pNode->xKey.iKey : nNext,pVal);
		}
		nNext++;
		if( pPrev == 0 ){
			break;
		}
	}
	PH7_NativeResultObject(pCtx,pNew);
	PH7_ClassInstanceUnref(pNew);
	return PH7_OK;
}
/* php's getIterator() answers an InternalIterator over the elements — the same
 * machinery every native IteratorAggregate here uses, which is also what makes
 * two iterators over one array independent. */
static void FaIterSettle(ph7_vm *pVm,ph7_class_instance *pIt)
{
	ph7_class_instance *pSrc = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);
	sxi64 iPos = PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS);
	ph7_value *pVal;
	if( pSrc == 0 || iPos < 0 || iPos >= FaSize(pSrc) ){
		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);
		return;
	}
	pVal = FaAt(&(*pVm),pSrc,iPos);
	if( pVal ){
		PH7_NativeSetProp(&(*pVm),pIt,PH7_NATIVE_IT_CUR,
			(int)SyStrlen(PH7_NATIVE_IT_CUR),pVal);
	}
	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_KEY,iPos);
	PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,0);
}
static void FaIterRewind(ph7_vm *pVm,ph7_class_instance *pIt)
{
	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,0);
	FaIterSettle(&(*pVm),pIt);
}
static void FaIterNext(ph7_vm *pVm,ph7_class_instance *pIt)
{
	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,
		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS) + 1);
	FaIterSettle(&(*pVm),pIt);
}
static const PH7_NativeIterVtab sFaIterVtab = { FaIterRewind, FaIterNext };
static int vm_builtin_SplFixedArray_getIterator(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
static int vm_builtin_SplFixedArray_wakeup(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	SXUNUSED(pCtx);
	return PH7_OK;   /* php 8.4 keeps it, deprecated, doing nothing */
}
static int vm_builtin_SplFixedArray_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pSlot;
	if( nArg < 1 || (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 || pThis == 0 ){
		return PH7_OK;
	}
	pSlot = PH7_NativeAttr(pThis,FA_A);
	if( pSlot ){
		PH7_MemObjRelease(pSlot);
		PH7_MemObjStore(apArg[0],pSlot);
	}
	PH7_NativeSetAttrInt(pVm,pThis,FA_N,
		(sxi64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);
	return PH7_OK;
}
/*
 * php's get_properties: the ELEMENTS, keyed by index, on every surface —
 * var_dump, print_r, the (array) cast and (through __serialize) serialize(). This
 * is the one native class so far whose presentation is the same for the debug and
 * the cast form.
 */
static sxi32 FaPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)
{
	sxi64 n = FaSize(pThis), i;
	SXUNUSED(bDebug);
	for( i = 0 ; i < n ; ++i ){
		ph7_value sKey,*pVal = FaAt(&(*pVm),pThis,i);
		if( pVal == 0 ){
			continue;
		}
		PH7_MemObjInitFromInt(&(*pVm),&sKey,i);
		ph7_array_add_elem(pOut,&sKey,pVal);
		PH7_MemObjRelease(&sKey);
	}
	return PH7_OK;
}
/*
 * The declaration. Method ORDER and the interface list are spl_fixedarray.stub's;
 * note that __construct, __serialize, __unserialize, getIterator and jsonSerialize
 * are the FIVE methods php does NOT mark tentative here.
 */
static sxi32 VmInstallSplFixedArray(ph7_vm *pVm)
{
	static const PH7_NativePropDef aFaProp[] = {
		{ FA_A, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ FA_N, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeMethodDef aFaMethod[] = {
		{ "__construct",   PH7_MOD_PUBLIC, "int $size = 0", 0,
		  vm_builtin_SplFixedArray_construct },
		{ "__wakeup",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplFixedArray_wakeup },
		{ "__serialize",   PH7_MOD_PUBLIC, "", "array", vm_builtin_SplFixedArray_toArray },
		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void",
		  vm_builtin_SplFixedArray_unserializeMagic },
		{ "count",         PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplFixedArray_count },
		{ "toArray",       PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplFixedArray_toArray },
		{ "fromArray",     PH7_MOD_PUBLIC|PH7_MOD_STATIC,
		  "array $array, bool $preserveKeys = true", "@SplFixedArray",
		  vm_builtin_SplFixedArray_fromArray },
		{ "getSize",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplFixedArray_getSize },
		{ "setSize",       PH7_MOD_PUBLIC, "int $size", "@true", vm_builtin_SplFixedArray_setSize },
		/* php's stub leaves the four offsets UNTYPED and decodes them itself, the
		 * same shape SplDoublyLinkedList has — but a different rule and a different
		 * refusal, so FaOffset() rather than the DLL's PH7_IntArgResolve. */
		{ "offsetExists",  PH7_MOD_PUBLIC, "$index", "@bool",
		  vm_builtin_SplFixedArray_offsetExists },
		{ "offsetGet",     PH7_MOD_PUBLIC, "$index", "@mixed", vm_builtin_SplFixedArray_offsetGet },
		{ "offsetSet",     PH7_MOD_PUBLIC, "$index, mixed $value", "@void",
		  vm_builtin_SplFixedArray_offsetSet },
		{ "offsetUnset",   PH7_MOD_PUBLIC, "$index", "@void",
		  vm_builtin_SplFixedArray_offsetUnset },
		{ "getIterator",   PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_SplFixedArray_getIterator },
		{ "jsonSerialize", PH7_MOD_PUBLIC, "", "array", vm_builtin_SplFixedArray_toArray },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "SplFixedArray", 0, "IteratorAggregate,ArrayAccess,Countable,JsonSerializable", 0,
		  aFaMethod, SX_ARRAYSIZE(aFaMethod), 0, 0,
		  aFaProp, SX_ARRAYSIZE(aFaProp), 0, &sFaIterVtab, FaPresent },
	};
	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
}
static const char zSplLib[] =
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
	/* After the dual iterators: RecursiveIteratorIterator names OuterIterator and
	 * RecursiveIterator, both declared by that table. */
	rc = VmInstallSplRecursiveIt(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Also before the chunk: SplPriorityQueue and the heaps are still PHP and the
	 * compiler reads them after these three are declared. */
	rc = VmInstallSplDllist(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	rc = VmInstallSplHeap(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	rc = VmInstallSplFixedArray(&(*pVm));
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
