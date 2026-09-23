/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <errno.h>   /* getLinkTarget names the errno text its readlink failed with */
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
		/* php's WeakMap read_dimension answers the stored zval itself, so an
		 * indirect modification through it lands (PH7_CLASS_DIM_WRITABLE). */
		{ "WeakMap", 0, 0, PH7_CLASS_FINAL|PH7_CLASS_NOSERIALIZE|PH7_CLASS_DIM_WRITABLE,
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
 * ---------------------------------------------------------------------------
 * The MEMBERS slot every SPL container's `__serialize()` carries.
 *
 * php's payload for these classes is its own state plus a members array, and that
 * array is `zend_std_get_properties()` — the instance's REAL properties. For a bare
 * SplObjectStorage or SplStack there are none, which is why every one of these bodies
 * shipped with a hardcoded empty array; the moment a user SUBCLASSES one, its declared
 * properties belong in the payload and PHL dropped them. `serialize()` then round-
 * tripped a SplStack subclass back to its property DEFAULTS with nothing failing —
 * the same shape of silent wrong answer the date family's hidden slots had, one layer
 * up. These two are that walk, shared by every container below.
 *
 * The walk must skip `PH7_CLASS_ATTR_HIDDEN`: a native class's engine slots are
 * exactly what the pair exists to replace. The load side writes only slots the class
 * DECLARES, which is what the engine's own unserialize does with an unknown name
 * (PHL has no dynamic properties).
 * ---------------------------------------------------------------------------
 */
static void SplAddMembers(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)
{
	SyHashEntry *pEntry;
	if( pThis == 0 ){
		return;
	}
	SyHashResetLoopCursor(&pThis->hAttr);
	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){
		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;
		SyString *pName = &pVmAttr->pAttr->sName;
		ph7_value *pVal;
		ph7_value sKey;
		if( pVmAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT
			|PH7_CLASS_ATTR_HIDDEN|PH7_CLASS_ATTR_HOOK_VIRTUAL) ){
			continue;
		}
		pVal = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);
		if( pVal == 0 ){
			continue;
		}
		PH7_MemObjInitFromString(&(*pVm),&sKey,0);
		PH7_MemObjStringAppend(&sKey,pName->zString,pName->nByte);
		ph7_array_add_elem(pOut,&sKey,pVal);
		PH7_MemObjRelease(&sKey);
	}
}
/* The same walk as a standalone array, which is the shape most payloads want. */
static sxi32 SplMembersOf(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)
{
	PH7_MemObjInit(&(*pVm),pOut);
	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){
		return SXERR_MEM;
	}
	SplAddMembers(&(*pVm),pThis,pOut);
	return SXRET_OK;
}
static int SplMembersWalk(ph7_value *pKey,ph7_value *pVal,void *pUserData)
{
	ph7_class_instance *pThis = (ph7_class_instance *)pUserData;
	const char *zKey;
	int nKey;
	if( !ph7_value_is_string(pKey) ){
		return PH7_OK;
	}
	zKey = ph7_value_to_string(pKey,&nKey);
	PH7_NativeSetProp(pThis->pVm,pThis,zKey,(sxu32)nKey,pVal);
	return PH7_OK;
}
static void SplMembersLoad(ph7_class_instance *pThis,ph7_value *pMembers)
{
	if( pThis == 0 || pMembers == 0 || (pMembers->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return;
	}
	ph7_array_walk(pMembers,SplMembersWalk,pThis);
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
 * php's `~SPL_ARRAY_INT_MASK`: the flags word keeps only its low 16 bits, so
 * `setFlags(-1)` then `getFlags()` answers 65535 rather than -1. php masks on the
 * WRITE, which is why every reader — getFlags(), __serialize(), the ARRAY_AS_PROPS
 * test — sees the same masked value without asking.
 */
#define SPL_FLAG_MASK 0xFFFF
/* php's SPL_ARRAY_STD_PROP_LIST: the non-debug presentation surfaces answer the
 * ordinary property table instead of the storage. */
#define SPL_STD_PROP_LIST 0x0001
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
		PH7_NativeSetAttrInt(pCtx->pVm,pThis,SPL_F,
			ph7_value_to_int64(apArg[0]) & SPL_FLAG_MASK);
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
		PH7_NativeSetAttrInt(pVm,pThis,SPL_F,ph7_value_to_int64(apArg[1]) & SPL_FLAG_MASK);
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
		PH7_NativeSetAttrInt(pVm,pThis,SPL_F,ph7_value_to_int64(apArg[1]) & SPL_FLAG_MASK);
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
/*
 * ---------------------------------------------------------------------------
 * php's __serialize()/__unserialize() for the array store.
 *
 * php's payload is a four-element LIST -- [flags, storage, members, iterator class]
 * -- and nothing else can express it: the state lives in ext/spl's own struct, so
 * there are no properties to walk. PHL walked its HIDDEN slots instead and wrote
 * `O:11:"ArrayObject":3:{s:16:"\0ArrayObject\0__d";…}`, which round-tripped inside
 * PHL and could not read a byte string php produced (nor be read by php).
 *
 * Two details worth keeping. The last element is NULL when the class is the default
 * ArrayIterator, and it is always NULL for an ArrayIterator payload -- php shares one
 * C body between both classes, so ArrayIterator carries the slot it has no use for.
 * And the iterator-class check here is LOOSER than setIteratorClass()'s: restoring
 * accepts any `Iterator`, while the setter and the constructor demand a class derived
 * from ArrayIterator. php words the refusal with `ArrayObject` either way, even when
 * ArrayIterator is the receiver.
 * ---------------------------------------------------------------------------
 */
static sxi32 SplStoreIllTyped(ph7_context *pCtx)
{
	return PH7_VmThrowException(pCtx,"UnexpectedValueException",
		"Incomplete or ill-typed serialization data");
}
static int vm_builtin_SplStore_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value sOut,sVal,*pStore;
	const char *zIt = 0;
	int nIt = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	PH7_MemObjInit(pVm,&sOut);
	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){
		PH7_MemObjRelease(&sOut);
		return PH7_ContextMemoryError(pCtx);
	}
	/* [0] the flags word */
	PH7_MemObjInitFromInt(pVm,&sVal,pThis ? PH7_NativeAttrInt(pThis,SPL_F) : 0);
	ph7_array_add_elem(&sOut,0,&sVal);
	PH7_MemObjRelease(&sVal);
	/* [1] the storage */
	pStore = SplStoreSlot(pVm,pThis);
	if( pStore ){
		ph7_array_add_elem(&sOut,0,pStore);
	}
	/* [2] the instance's own properties */
	if( SplMembersOf(pVm,pThis,&sVal) != SXRET_OK ){
		PH7_MemObjRelease(&sOut);
		return PH7_ContextMemoryError(pCtx);
	}
	ph7_array_add_elem(&sOut,0,&sVal);
	PH7_MemObjRelease(&sVal);
	/* [3] the iterator class, NULL for the default one and for ArrayIterator */
	if( pThis ){
		PH7_NativeAttrStr(pThis,SPL_IT,&zIt,&nIt);
	}
	PH7_MemObjInit(pVm,&sVal);
	if( nIt > 0 && (nIt != (int)sizeof("ArrayIterator")-1
	 || SyMemcmp(zIt,"ArrayIterator",sizeof("ArrayIterator")-1) != 0) ){
		PH7_MemObjStringAppend(&sVal,zIt,(sxu32)nIt);
	}
	ph7_array_add_elem(&sOut,0,&sVal);
	PH7_MemObjRelease(&sVal);
	ph7_result_value(pCtx,&sOut);
	PH7_MemObjRelease(&sOut);
	return PH7_OK;
}
static int vm_builtin_SplStore_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_hashmap *pData;
	ph7_hashmap_node *pNode = 0;
	ph7_value *pFlags,*pStorage,*pMembers,*pIt = 0;
	sxi32 rc;
	if( nArg < 1 || (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){
		char zBuf[64];
		return PH7_VmThrowException(pCtx,"TypeError",
			"%s(): Argument #1 ($data) must be of type array, %s given",
			ph7_function_name(pCtx),
			nArg > 0 ? VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)) : "none");
	}
	if( pThis == 0 ){
		return PH7_OK;
	}
	pData = (ph7_hashmap *)apArg[0]->x.pOther;
	if( HashmapLookupIntKey(pData,0,&pNode) != SXRET_OK ){
		return SplStoreIllTyped(pCtx);
	}
	pFlags = HashmapExtractNodeValue(pNode);
	pNode = 0;
	if( HashmapLookupIntKey(pData,1,&pNode) != SXRET_OK ){
		return SplStoreIllTyped(pCtx);
	}
	pStorage = HashmapExtractNodeValue(pNode);
	pNode = 0;
	if( HashmapLookupIntKey(pData,2,&pNode) != SXRET_OK ){
		return SplStoreIllTyped(pCtx);
	}
	pMembers = HashmapExtractNodeValue(pNode);
	pNode = 0;
	if( HashmapLookupIntKey(pData,3,&pNode) == SXRET_OK ){
		pIt = HashmapExtractNodeValue(pNode);
	}
	if( pFlags == 0 || (pFlags->iFlags & MEMOBJ_INT) == 0
	 || pMembers == 0 || (pMembers->iFlags & MEMOBJ_HASHMAP) == 0
	 || (pIt != 0 && (pIt->iFlags & (MEMOBJ_NULL|MEMOBJ_STRING)) == 0) ){
		return SplStoreIllTyped(pCtx);
	}
	/* php's own wording, and its own exception CLASS, for the storage slot. */
	if( pStorage == 0 || (pStorage->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ)) == 0 ){
		return PH7_VmThrowException(pCtx,"InvalidArgumentException",
			"Passed variable is not an array or object");
	}
	if( pIt != 0 && (pIt->iFlags & MEMOBJ_STRING) != 0 && SyBlobLength(&pIt->sBlob) > 0 ){
		const char *zIt = (const char *)SyBlobData(&pIt->sBlob);
		int nIt = (int)SyBlobLength(&pIt->sBlob);
		ph7_class *pClass = PH7_VmExtractClass(pVm,zIt,(sxu32)nIt,FALSE,0);
		ph7_class *pIface = PH7_VmExtractClass(pVm,"Iterator",sizeof("Iterator")-1,FALSE,0);
		if( pClass == 0 ){
			return PH7_VmThrowException(pCtx,"UnexpectedValueException",
				"Cannot deserialize ArrayObject with iterator class '%.*s'; "
				"no such class exists",nIt,zIt);
		}
		if( pIface == 0 || !PH7_VmInstanceOf(pClass,pIface) ){
			return PH7_VmThrowException(pCtx,"UnexpectedValueException",
				"Cannot deserialize ArrayObject with iterator class '%.*s'; "
				"this class does not implement the Iterator interface",nIt,zIt);
		}
		if( PH7_NativeAttr(pThis,SPL_IT) ){
			PH7_NativeSetAttrStr(pVm,pThis,SPL_IT,zIt,nIt);
		}
	}
	PH7_NativeSetAttrInt(pVm,pThis,SPL_F,ph7_value_to_int64(pFlags) & SPL_FLAG_MASK);
	rc = SplInitStore(pCtx,pThis,pStorage,"ArrayObject::__unserialize");
	if( rc != SXRET_OK ){
		return rc;
	}
	SplMembersLoad(pThis,pMembers);
	return PH7_OK;
}
/*
 * ---------------------------------------------------------------------------
 * php's presentation for the array store (ph7_class::xPresent).
 *
 * php has two handlers here and they DISAGREE, which is the whole reason the hook
 * is told which is asking. `spl_array_get_debug_info` always shows ONE entry —
 * the storage under its MANGLED private name — after whatever real properties the
 * instance has; `spl_array_get_properties_for` answers the storage's ELEMENTS
 * directly for the var_export / (array) / json purposes, with no `storage` key at
 * all, and hands back the ordinary property table when STD_PROP_LIST is set. The
 * flag is therefore visible on one surface and invisible on the other: a
 * STD_PROP_LIST ArrayObject still var_dumps its storage.
 *
 * The mangled name always spells the ROOT class, never the receiver's: a
 * RecursiveArrayIterator shows `["storage":"ArrayIterator":private]`.
 * ---------------------------------------------------------------------------
 */
static int SplStoreMangledKey(ph7_class_instance *pThis,char *zBuf,int nBuf)
{
	const char *zRoot = "ArrayObject";
	ph7_class *pClass;
	int nRoot,nOut = 0;
	for( pClass = pThis->pClass ; pClass ; pClass = pClass->pBase ){
		if( pClass->sName.nByte == sizeof("ArrayIterator")-1
		 && SyMemcmp(pClass->sName.zString,"ArrayIterator",sizeof("ArrayIterator")-1) == 0 ){
			zRoot = "ArrayIterator";
			break;
		}
	}
	nRoot = (int)SyStrlen(zRoot);
	if( nRoot + (int)sizeof("\0\0storage") > nBuf ){
		return 0;
	}
	zBuf[nOut++] = 0;
	SyMemcpy(zRoot,&zBuf[nOut],(sxu32)nRoot);
	nOut += nRoot;
	zBuf[nOut++] = 0;
	SyMemcpy("storage",&zBuf[nOut],sizeof("storage")-1);
	nOut += (int)sizeof("storage")-1;
	return nOut;
}
static int SplPresentWalk(ph7_value *pKey,ph7_value *pVal,void *pUserData)
{
	ph7_array_add_elem((ph7_value *)pUserData,pKey,pVal);
	return PH7_OK;
}
static sxi32 SplStorePresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)
{
	ph7_value *pStore = pThis ? PH7_NativeAttr(pThis,SPL_D) : 0;
	if( bDebug ){
		ph7_value sKey;
		char zKey[64];
		int nKey;
		/* The instance's OWN properties come first — php's debug info starts from
		 * the standard table and appends the storage entry to it. */
		SplAddMembers(&(*pVm),pThis,pOut);
		nKey = SplStoreMangledKey(pThis,zKey,(int)sizeof(zKey));
		if( nKey > 0 && pStore ){
			PH7_MemObjInitFromString(&(*pVm),&sKey,0);
			PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKey);
			ph7_array_add_elem(pOut,&sKey,pStore);
			PH7_MemObjRelease(&sKey);
		}
		return SXRET_OK;
	}
	if( (PH7_NativeAttrInt(pThis,SPL_F) & SPL_STD_PROP_LIST) != 0 ){
		SplAddMembers(&(*pVm),pThis,pOut);
		return SXRET_OK;
	}
	if( pStore && (pStore->iFlags & MEMOBJ_HASHMAP) != 0 ){
		ph7_array_walk(pStore,SplPresentWalk,pOut);
	}
	return SXRET_OK;
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
		{ "__serialize",  PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplStore_serializeMagic },
		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "@void",
		  vm_builtin_SplStore_unserializeMagic },
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
		{ "__serialize",      PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplStore_serializeMagic },
		{ "__unserialize",    PH7_MOD_PUBLIC, "array $data", "@void",
		  vm_builtin_SplStore_unserializeMagic },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		/* `interface X extends Iterator` is a PARENT, not an implemented interface:
		 * the compiler puts it in pBase and Reflection walks pBase to answer which
		 * class DECLARED an inherited method. Naming it in zImplements instead made
		 * current()/key()/next()/rewind()/valid() report this interface as their
		 * declaring class where php reports Iterator. */
		{ "SeekableIterator", "Iterator", 0, PH7_CLASS_INTERFACE,
		  aSeekMethod, SX_ARRAYSIZE(aSeekMethod), 0, 0, 0, 0, 0, 0, 0 },
		/* PH7_CLASS_DIM_WRITABLE: php's spl_array read_dimension hands back the
		 * REAL element for a write fetch, so `$ao['k']['n'] = v` lands — unlike
		 * SplFixedArray / SplDoublyLinkedList / SplObjectStorage, which keep the
		 * standard handler and get php's indirect-modification notice. */
		{ "ArrayIterator", 0, "SeekableIterator,ArrayAccess,Countable", PH7_CLASS_DIM_WRITABLE,
		  aItMethod, SX_ARRAYSIZE(aItMethod), aConst, SX_ARRAYSIZE(aConst),
		  aItProp, SX_ARRAYSIZE(aItProp), 0, 0, SplStorePresent },
		{ "ArrayObject", 0, "IteratorAggregate,ArrayAccess,Countable", PH7_CLASS_DIM_WRITABLE,
		  aObjMethod, SX_ARRAYSIZE(aObjMethod), aConst, SX_ARRAYSIZE(aConst),
		  aObjProp, SX_ARRAYSIZE(aObjProp), 0, 0, SplStorePresent },
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
	/* The members slot: php's own properties — empty for a bare SplStack, a
	 * SUBCLASS's declared slots when there is one. */
	if( SplMembersOf(pVm,pThis,&sVal) == SXRET_OK ){
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
	if( HashmapLookupIntKey(pData,2,&pNode) == SXRET_OK ){
		SplMembersLoad(pThis,HashmapExtractNodeValue(pNode));
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
 * accumulates every previous answer into the next one. Shared with
 * SplObjectStorage's legacy format, which is built the same way.
 */
static void SplSerializeInto(ph7_context *pCtx,ph7_value **apCall,SyBlob *pOut)
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
	SplSerializeInto(pCtx,apCall,&sOut);
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
		SplSerializeInto(pCtx,apCall,&sOut);
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
 * a two-element list whose first entry is the instance's own property table —
 * empty for a bare heap, a SUBCLASS's declared slots when there is one.
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
	PH7_MemObjInit(pVm,&sState);
	if( SplMembersOf(pVm,pThis,&sMembers) != SXRET_OK
	 || PH7_MemObjToHashmap(&sOut) != SXRET_OK
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
	if( HashmapLookupIntKey(pData,0,&pNode) == SXRET_OK ){
		SplMembersLoad(pThis,HashmapExtractNodeValue(pNode));
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
/*
 * php's __serialize() here is NOT toArray(): the members ride in the SAME array as
 * the elements, told apart by their key — an INT key is an element and a STRING key
 * is a property. That is the whole reason the payload of a SplFixedArray subclass
 * reads `{i:0;N;i:1;N;s:1:"p";i:9;}` and not a nested pair.
 */
static int vm_builtin_SplFixedArray_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value sOut,*pSlot;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	pSlot = FaSlot(pVm,pThis);
	PH7_MemObjInit(pVm,&sOut);
	if( pSlot ){
		PH7_MemObjStore(pSlot,&sOut);
	}
	if( PH7_MemObjToHashmap(&sOut) != SXRET_OK ){
		PH7_MemObjRelease(&sOut);
		return PH7_ContextMemoryError(pCtx);
	}
	SplAddMembers(pVm,pThis,&sOut);   /* string keys, beside the int-keyed elements */
	ph7_result_value(pCtx,&sOut);
	PH7_MemObjRelease(&sOut);
	return PH7_OK;
}
/* Split the payload back apart: int keys rebuild the elements, string keys the
 * properties. The element count is what the INT half holds, not the whole array. */
typedef struct fa_unser_ctx fa_unser_ctx;
struct fa_unser_ctx
{
	ph7_class_instance *pThis;
	ph7_value *pElems;
	sxi64 nElem;
};
static int FaUnserWalk(ph7_value *pKey,ph7_value *pVal,void *pUserData)
{
	fa_unser_ctx *pFa = (fa_unser_ctx *)pUserData;
	if( ph7_value_is_string(pKey) ){
		int nKey;
		const char *zKey = ph7_value_to_string(pKey,&nKey);
		PH7_NativeSetProp(pFa->pThis->pVm,pFa->pThis,zKey,(sxu32)nKey,pVal);
		return PH7_OK;
	}
	ph7_array_add_elem(pFa->pElems,0,pVal);
	pFa->nElem++;
	return PH7_OK;
}
static int vm_builtin_SplFixedArray_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	fa_unser_ctx sFa;
	ph7_value sElems,*pSlot;
	if( nArg < 1 || (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 || pThis == 0 ){
		return PH7_OK;
	}
	PH7_MemObjInit(pVm,&sElems);
	if( PH7_MemObjToHashmap(&sElems) != SXRET_OK ){
		PH7_MemObjRelease(&sElems);
		return PH7_ContextMemoryError(pCtx);
	}
	sFa.pThis = pThis;
	sFa.pElems = &sElems;
	sFa.nElem = 0;
	ph7_array_walk(apArg[0],FaUnserWalk,&sFa);
	pSlot = PH7_NativeAttr(pThis,FA_A);
	if( pSlot ){
		PH7_MemObjRelease(pSlot);
		PH7_MemObjStore(&sElems,pSlot);
	}
	PH7_MemObjRelease(&sElems);
	PH7_NativeSetAttrInt(pVm,pThis,FA_N,sFa.nElem);
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
		{ "__serialize",   PH7_MOD_PUBLIC, "", "array", vm_builtin_SplFixedArray_serializeMagic },
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
/*
 * ---------------------------------------------------------------------------
 * SplObjectStorage, SplObserver and SplSubject.
 *
 * php's `spl_SplObjectStorage` is a hashtable of {obj, inf} pairs keyed by the
 * object HANDLE, plus TWO cursors that are not the same thing: `pos` walks the
 * table and `index` is the integer `key()` reports. Every method that changes the
 * membership resets one or both, and the chunk -- which kept a single integer
 * offset -- had none of that: `detach()` mid-walk left the walk where it was
 * (php restarts it), and `addAll()` left `key()` counting from wherever it stood.
 *
 * What the chunk did not have AT ALL, which is most of the class: `seek()` and the
 * `SeekableIterator` interface it comes from, `Serializable` with its
 * `serialize()`/`unserialize()` pair, the `__serialize()`/`__unserialize()` pair
 * php actually uses, and `__debugInfo()`. Six methods and two interfaces missing
 * from a 25-method class -- rule 53, and the reason a method-by-method reading is
 * not a conversion.
 *
 * Two more the model hides. **`current()` on an invalid iterator RAISES**
 * (`Called current() on invalid iterator`) where the chunk answered null, and
 * **an overridden `getHash()` is what keys the table** -- php looks the method up
 * once per instance (`fptr_get_hash`) and every attach/detach/contains goes
 * through it, so a subclass that hashes two distinct objects the same stores ONE
 * entry. The chunk called `spl_object_id()` directly and ignored its own
 * `getHash()`, so overriding it did nothing.
 *
 * php DEPRECATES attach/detach/contains since 8.5 and PHL says nothing, which is
 * the same non-deprecated-compatibility policy the chunk carried (the notice is
 * the only difference and no valid php depends on it).
 */
#define SOS_S "__s"   /* php's storage: key -> ['obj' => object, 'inf' => info] */
#define SOS_I "__i"   /* php's index: what key() reports, NOT a position */

/* The storage slot, separated for writing (every caller may mutate it). */
static ph7_value * SosSlot(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,SOS_S) : 0;
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
static ph7_hashmap * SosMap(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_value *pSlot = SosSlot(pVm,pThis);
	return pSlot ? (ph7_hashmap *)pSlot->x.pOther : 0;
}
/* One half of a stored pair: php's element->obj / element->inf. */
static ph7_value * SosPart(ph7_value *pPair,const char *zKey)
{
	ph7_hashmap_node *pNode = 0;
	if( pPair == 0 || (pPair->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return 0;
	}
	if( HashmapLookupBlobKey((ph7_hashmap *)pPair->x.pOther,zKey,
		(sxu32)SyStrlen(zKey),&pNode) != SXRET_OK ){
		return 0;
	}
	return HashmapExtractNodeValue(pNode);
}
/* Write one half of a pair; a NULL value is php's `ZVAL_NULL(&element->inf)`. */
static void SosSetPart(ph7_vm *pVm,ph7_value *pPair,const char *zKey,ph7_value *pVal)
{
	ph7_value sKey,sNull;
	PH7_MemObjInitFromString(pVm,&sKey,0);
	PH7_MemObjStringAppend(&sKey,zKey,(sxu32)SyStrlen(zKey));
	if( pVal ){
		ph7_array_add_elem(pPair,&sKey,pVal);
	}else{
		PH7_MemObjInit(pVm,&sNull);
		ph7_array_add_elem(pPair,&sKey,&sNull);
		PH7_MemObjRelease(&sNull);
	}
	PH7_MemObjRelease(&sKey);
}
/* The pair a node holds, separated: php mutates `element->inf` in place, and here
 * that is a NESTED array whose COW copy has to be broken first -- a pair handed
 * out by __serialize()/__debugInfo() would otherwise change with it. */
static ph7_value * SosPairForWrite(ph7_vm *pVm,ph7_hashmap_node *pNode)
{
	ph7_value *pPair = HashmapExtractNodeValue(pNode);
	if( pPair == 0 || (pPair->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return 0;
	}
	return PH7_HashmapCowSeparate(pVm,pPair) ? pPair : 0;
}
/*
 * php's `fptr_get_hash`: the class caches the method ONLY when a subclass declares
 * its own, and every keyed operation then runs it. The one native body is this
 * class's own, so a non-native getHash() IS the override.
 */
static ph7_class_method * SosUserHash(ph7_class_instance *pThis)
{
	ph7_class_method *pMethod;
	if( pThis == 0 ){
		return 0;
	}
	pMethod = PH7_ClassExtractMethod(pThis->pClass,"getHash",sizeof("getHash")-1);
	if( pMethod == 0 || (pMethod->sFunc.iFlags & VM_FUNC_NATIVE) ){
		return 0;
	}
	return pMethod;
}
/*
 * php's spl_object_storage_get_hash: the object HANDLE, or the STRING an
 * overridden getHash() answers. php checks the returned type itself (its own
 * return declaration would coerce first, so this only fires for an untyped
 * override) and names the RUNTIME class in the refusal.
 */
static sxi32 SosKey(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pObj,ph7_value *pKey)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_method *pHash = SosUserHash(pThis);
	ph7_value sRes,*apArg[1];
	sxi32 rc;
	if( pHash == 0 ){
		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;
		PH7_MemObjRelease(pKey);
		PH7_MemObjInitFromInt(pVm,pKey,(sxi64)pInst->nObjId);
		return PH7_OK;
	}
	PH7_MemObjInit(pVm,&sRes);
	apArg[0] = pObj;
	rc = PH7_VmCallClassMethod(pVm,pThis,pHash,&sRes,1,apArg);
	if( rc != SXRET_OK ){
		PH7_MemObjRelease(&sRes);
		return rc;
	}
	if( (sRes.iFlags & MEMOBJ_STRING) == 0 ){
		char zGiven[64];
		SyString *pName = &pThis->pClass->sName;
		PH7_MemObjRelease(&sRes);
		return PH7_VmThrowException(pCtx,"TypeError",
			"%z::getHash(): Return value must be of type string, %s returned",
			pName,VmValueGivenName(&sRes,zGiven,sizeof(zGiven)));
	}
	PH7_MemObjRelease(pKey);
	PH7_MemObjInit(pVm,pKey);
	PH7_MemObjStore(&sRes,pKey);
	PH7_MemObjRelease(&sRes);
	return PH7_OK;
}
/*
 * php's Z_PARAM_OBJ for the four ArrayAccess offsets: their stub leaves $object
 * UNTYPED (a `@param object` docblock, which Reflection does not print) while the
 * ZPP is an object, so the declared type says nothing and each body words the
 * refusal here -- SplDoublyLinkedList's $index has the same shape one type over.
 * The name is always this class's, even from a subclass (php's).
 */
static sxi32 SosObjectArg(ph7_context *pCtx,const char *zMethod,int nArg,ph7_value **apArg,
	ph7_value **ppObj)
{
	char zGiven[64];
	*ppObj = 0;
	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) && apArg[0]->x.pOther ){
		*ppObj = apArg[0];
		return PH7_OK;
	}
	return PH7_VmThrowException(pCtx,"TypeError",
		"SplObjectStorage::%s(): Argument #1 ($object) must be of type object, %s given",
		zMethod,nArg < 1 ? "none" : VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));
}
/* The other storage a set operation takes; php's ZPP already screened the class. */
static ph7_class_instance * SosOther(int nArg,ph7_value **apArg)
{
	if( nArg < 1 || (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){
		return 0;
	}
	return (ph7_class_instance *)apArg[0]->x.pOther;
}
/*
 * php's spl_object_storage_attach. The two values are COPIED first: computing the
 * key can run an overridden getHash(), and any call into user code moves every
 * ph7_value the caller is holding (rule 47).
 */
static sxi32 SosAttach(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pObj,ph7_value *pInf)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_hashmap *pMap;
	ph7_hashmap_node *pNode = 0;
	ph7_value sObj,sInf,sKey,sPair;
	sxi32 rc;
	PH7_MemObjInit(pVm,&sObj);
	PH7_MemObjInit(pVm,&sInf);
	PH7_MemObjInit(pVm,&sKey);
	PH7_MemObjStore(pObj,&sObj);
	if( pInf ){
		PH7_MemObjStore(pInf,&sInf);
	}
	rc = SosKey(pCtx,pThis,&sObj,&sKey);
	if( rc != PH7_OK ){
		goto done;
	}
	pMap = SosMap(pVm,pThis);
	if( pMap == 0 ){
		goto done;
	}
	if( PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK ){
		ph7_value *pPair = SosPairForWrite(pVm,pNode);
		if( pPair ){
			SosSetPart(pVm,pPair,"inf",&sInf);
		}
		goto done;
	}
	PH7_MemObjInit(pVm,&sPair);
	if( PH7_MemObjToHashmap(&sPair) != SXRET_OK ){
		PH7_MemObjRelease(&sPair);
		rc = PH7_ContextMemoryError(pCtx);
		goto done;
	}
	SosSetPart(pVm,&sPair,"obj",&sObj);
	SosSetPart(pVm,&sPair,"inf",&sInf);
	/* php's position is an INTEGER index into the bucket array, so a cursor that
	 * ran off the end is revived by the insert and the walk resumes on the new
	 * element -- what addAll() mid-iteration does there. */
	SplStoreInsert(pMap,&sKey,&sPair);
	PH7_MemObjRelease(&sPair);
done:
	PH7_MemObjRelease(&sObj);
	PH7_MemObjRelease(&sInf);
	PH7_MemObjRelease(&sKey);
	return rc;
}
/* php's spl_object_storage_detach: drop the entry, saying whether there was one. */
static sxi32 SosDetach(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pObj,int *pbGone)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_hashmap *pMap;
	ph7_hashmap_node *pNode = 0;
	ph7_value sObj,sKey;
	sxi32 rc;
	if( pbGone ){
		*pbGone = 0;
	}
	PH7_MemObjInit(pVm,&sObj);
	PH7_MemObjInit(pVm,&sKey);
	PH7_MemObjStore(pObj,&sObj);
	rc = SosKey(pCtx,pThis,&sObj,&sKey);
	if( rc == PH7_OK ){
		pMap = SosMap(pVm,pThis);
		if( pMap && PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK ){
			PH7_HashmapUnlinkNode(pNode,TRUE);
			if( pbGone ){
				*pbGone = 1;
			}
		}
	}
	PH7_MemObjRelease(&sObj);
	PH7_MemObjRelease(&sKey);
	return rc;
}
/* php's spl_object_storage_contains: an entry EXISTS, whatever its info holds. */
static sxi32 SosContains(ph7_context *pCtx,ph7_class_instance *pThis,ph7_value *pObj,int *pbFound)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_hashmap *pMap;
	ph7_hashmap_node *pNode = 0;
	ph7_value sObj,sKey;
	sxi32 rc;
	*pbFound = 0;
	PH7_MemObjInit(pVm,&sObj);
	PH7_MemObjInit(pVm,&sKey);
	PH7_MemObjStore(pObj,&sObj);
	rc = SosKey(pCtx,pThis,&sObj,&sKey);
	if( rc == PH7_OK ){
		pMap = SosMap(pVm,pThis);
		*pbFound = pMap && PH7_HashmapLookup(pMap,&sKey,&pNode) == SXRET_OK;
	}
	PH7_MemObjRelease(&sObj);
	PH7_MemObjRelease(&sKey);
	return rc;
}
/* php's `zend_hash_internal_pointer_reset_ex(&storage, &pos); index = 0`. */
static void SosRewind(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_hashmap *pMap = SosMap(pVm,pThis);
	if( pMap ){
		pMap->pCur = pMap->pFirst;
	}
	PH7_NativeSetAttrInt(pVm,pThis,SOS_I,0);
}
/* The pair the cursor is on, or 0 past the end. */
static ph7_value * SosCurrentPair(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_hashmap *pMap = SosMap(pVm,pThis);
	if( pMap == 0 || pMap->pCur == 0 ){
		return 0;
	}
	return HashmapExtractNodeValue(pMap->pCur);
}
/*
 * A SNAPSHOT of one storage's pairs as a plain list. Every set operation walks one
 * storage while writing to another -- and either walk can run an overridden
 * getHash(), which moves things (rule 47) and can even mutate the map being walked.
 * php's own SPL_SAFE_HASH_FOREACH_PTR is the same precaution one layer down.
 */
static sxi32 SosSnapshot(ph7_vm *pVm,ph7_class_instance *pFrom,ph7_value *pOut)
{
	ph7_hashmap *pMap = SosMap(pVm,pFrom);
	ph7_hashmap_node *pNode;
	sxu32 n;
	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){
		return SXERR_MEM;
	}
	if( pMap == 0 ){
		return SXRET_OK;
	}
	pNode = pMap->pFirst;
	for( n = 0 ; n < pMap->nEntry && pNode ; ++n ){
		ph7_value *pPair = HashmapExtractNodeValue(pNode);
		if( pPair ){
			ph7_array_add_elem(pOut,0,pPair);
		}
		pNode = pNode->pPrev;   /* insertion order: pFirst, then the pPrev chain */
	}
	return SXRET_OK;
}
/* One pair of a snapshot, re-resolved by index because a user call may have moved
 * every value in the pool since the last one. */
static ph7_value * SosSnapAt(ph7_value *pSnap,sxi64 i)
{
	ph7_hashmap_node *pNode = 0;
	if( (pSnap->iFlags & MEMOBJ_HASHMAP) == 0
	 || HashmapLookupIntKey((ph7_hashmap *)pSnap->x.pOther,i,&pNode) != SXRET_OK ){
		return 0;
	}
	return HashmapExtractNodeValue(pNode);
}
static int vm_builtin_SplObjectStorage_attach(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pObj;
	sxi32 rc = SosObjectArg(pCtx,"attach",nArg,apArg,&pObj);
	if( rc != PH7_OK ){
		return rc;
	}
	return SosAttach(pCtx,PH7_ContextThis(pCtx),pObj,nArg > 1 ? apArg[1] : 0);
}
/* php's offsetSet is an @implementation-alias of attach, and its refusal says so. */
static int vm_builtin_SplObjectStorage_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pObj;
	sxi32 rc = SosObjectArg(pCtx,"offsetSet",nArg,apArg,&pObj);
	if( rc != PH7_OK ){
		return rc;
	}
	return SosAttach(pCtx,PH7_ContextThis(pCtx),pObj,nArg > 1 ? apArg[1] : 0);
}
/* detach() RESTARTS the walk: php resets both the position and the index, whether
 * or not anything was removed. */
static sxi32 SosDetachMethod(ph7_context *pCtx,const char *zMethod,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pObj;
	sxi32 rc = SosObjectArg(pCtx,zMethod,nArg,apArg,&pObj);
	if( rc != PH7_OK ){
		return rc;
	}
	rc = SosDetach(pCtx,pThis,pObj,0);
	if( rc != PH7_OK ){
		return rc;
	}
	SosRewind(pCtx->pVm,pThis);
	return PH7_OK;
}
static int vm_builtin_SplObjectStorage_detach(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SosDetachMethod(pCtx,"detach",nArg,apArg);
}
static int vm_builtin_SplObjectStorage_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SosDetachMethod(pCtx,"offsetUnset",nArg,apArg);
}
static sxi32 SosContainsMethod(ph7_context *pCtx,const char *zMethod,int nArg,ph7_value **apArg)
{
	ph7_value *pObj;
	int bFound = 0;
	sxi32 rc = SosObjectArg(pCtx,zMethod,nArg,apArg,&pObj);
	if( rc != PH7_OK ){
		return rc;
	}
	rc = SosContains(pCtx,PH7_ContextThis(pCtx),pObj,&bFound);
	if( rc != PH7_OK ){
		return rc;
	}
	ph7_result_bool(pCtx,bFound);
	return PH7_OK;
}
static int vm_builtin_SplObjectStorage_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SosContainsMethod(pCtx,"contains",nArg,apArg);
}
static int vm_builtin_SplObjectStorage_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SosContainsMethod(pCtx,"offsetExists",nArg,apArg);
}
/* php's offsetGet: the info, or `Object not found` -- NOT null, and not false. */
static int vm_builtin_SplObjectStorage_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_hashmap *pMap;
	ph7_hashmap_node *pNode = 0;
	ph7_value sObj,sKey,*pObj,*pInf;
	sxi32 rc = SosObjectArg(pCtx,"offsetGet",nArg,apArg,&pObj);
	if( rc != PH7_OK ){
		return rc;
	}
	PH7_MemObjInit(pVm,&sObj);
	PH7_MemObjInit(pVm,&sKey);
	PH7_MemObjStore(pObj,&sObj);
	rc = SosKey(pCtx,pThis,&sObj,&sKey);
	if( rc == PH7_OK ){
		pMap = SosMap(pVm,pThis);
		if( pMap == 0 || PH7_HashmapLookup(pMap,&sKey,&pNode) != SXRET_OK ){
			rc = PH7_VmThrowException(pCtx,"UnexpectedValueException","Object not found");
		}else{
			pInf = SosPart(HashmapExtractNodeValue(pNode),"inf");
			if( pInf ){
				ph7_result_value(pCtx,pInf);
			}else{
				ph7_result_null(pCtx);
			}
		}
	}
	PH7_MemObjRelease(&sObj);
	PH7_MemObjRelease(&sKey);
	return rc;
}
/*
 * php's addAll: attach every pair of the other storage, then reset the INDEX only
 * -- the position is deliberately left where it stood, which is why an insert can
 * revive a walk that had run out.
 */
static int vm_builtin_SplObjectStorage_addAll(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pOther = SosOther(nArg,apArg);
	ph7_hashmap *pMap;
	ph7_value sSnap;
	sxi64 i,n;
	sxi32 rc = PH7_OK;
	if( pOther == 0 || pThis == 0 ){
		return PH7_OK;
	}
	PH7_MemObjInit(pVm,&sSnap);
	if( SosSnapshot(pVm,pOther,&sSnap) != SXRET_OK ){
		PH7_MemObjRelease(&sSnap);
		return PH7_ContextMemoryError(pCtx);
	}
	n = (sxi64)((ph7_hashmap *)sSnap.x.pOther)->nEntry;
	for( i = 0 ; i < n && rc == PH7_OK ; ++i ){
		ph7_value *pPair = SosSnapAt(&sSnap,i);
		ph7_value *pObj = SosPart(pPair,"obj");
		ph7_value *pInf = SosPart(pPair,"inf");
		if( pObj ){
			rc = SosAttach(pCtx,pThis,pObj,pInf);
		}
	}
	PH7_MemObjRelease(&sSnap);
	if( rc != PH7_OK ){
		return rc;
	}
	PH7_NativeSetAttrInt(pVm,pThis,SOS_I,0);
	pMap = SosMap(pVm,pThis);
	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);
	return PH7_OK;
}
/* php's removeAll: detach everything the other storage holds, then RESTART the walk. */
static int vm_builtin_SplObjectStorage_removeAll(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pOther = SosOther(nArg,apArg);
	ph7_hashmap *pMap;
	ph7_value sSnap;
	sxi64 i,n;
	sxi32 rc = PH7_OK;
	if( pOther == 0 || pThis == 0 ){
		return PH7_OK;
	}
	PH7_MemObjInit(pVm,&sSnap);
	if( SosSnapshot(pVm,pOther,&sSnap) != SXRET_OK ){
		PH7_MemObjRelease(&sSnap);
		return PH7_ContextMemoryError(pCtx);
	}
	n = (sxi64)((ph7_hashmap *)sSnap.x.pOther)->nEntry;
	for( i = 0 ; i < n && rc == PH7_OK ; ++i ){
		ph7_value *pObj = SosPart(SosSnapAt(&sSnap,i),"obj");
		if( pObj ){
			rc = SosDetach(pCtx,pThis,pObj,0);
		}
	}
	PH7_MemObjRelease(&sSnap);
	if( rc != PH7_OK ){
		return rc;
	}
	SosRewind(pVm,pThis);
	pMap = SosMap(pVm,pThis);
	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);
	return PH7_OK;
}
/* php's removeAllExcept: the INTERSECTION, walked over this storage's own pairs. */
static int vm_builtin_SplObjectStorage_removeAllExcept(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pOther = SosOther(nArg,apArg);
	ph7_hashmap *pMap;
	ph7_value sSnap;
	sxi64 i,n;
	sxi32 rc = PH7_OK;
	if( pOther == 0 || pThis == 0 ){
		return PH7_OK;
	}
	PH7_MemObjInit(pVm,&sSnap);
	if( SosSnapshot(pVm,pThis,&sSnap) != SXRET_OK ){
		PH7_MemObjRelease(&sSnap);
		return PH7_ContextMemoryError(pCtx);
	}
	n = (sxi64)((ph7_hashmap *)sSnap.x.pOther)->nEntry;
	for( i = 0 ; i < n && rc == PH7_OK ; ++i ){
		ph7_value *pObj = SosPart(SosSnapAt(&sSnap,i),"obj");
		int bFound = 0;
		if( pObj == 0 ){
			continue;
		}
		rc = SosContains(pCtx,pOther,pObj,&bFound);
		if( rc == PH7_OK && !bFound ){
			pObj = SosPart(SosSnapAt(&sSnap,i),"obj");   /* re-resolved: getHash may have run */
			if( pObj ){
				rc = SosDetach(pCtx,pThis,pObj,0);
			}
		}
	}
	PH7_MemObjRelease(&sSnap);
	if( rc != PH7_OK ){
		return rc;
	}
	SosRewind(pVm,pThis);
	pMap = SosMap(pVm,pThis);
	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);
	return PH7_OK;
}
/*
 * php's count(): COUNT_RECURSIVE is accepted and changes nothing -- the storage
 * holds C structs rather than zvals there, so nothing recurses.
 */
static int vm_builtin_SplObjectStorage_count(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap = SosMap(pCtx->pVm,PH7_ContextThis(pCtx));
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_int64(pCtx,pMap ? (ph7_int64)pMap->nEntry : 0);
	return PH7_OK;
}
static int vm_builtin_SplObjectStorage_getHash(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pObj;
	sxi32 rc = SosObjectArg(pCtx,"getHash",nArg,apArg,&pObj);
	if( rc != PH7_OK ){
		return rc;
	}
	/* php's getHash() IS php_spl_object_hash(), the same one the function answers. */
	return vm_builtin_spl_object_hash(pCtx,nArg,apArg);
}
static int vm_builtin_SplObjectStorage_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	SosRewind(pCtx->pVm,PH7_ContextThis(pCtx));
	return PH7_OK;
}
static int vm_builtin_SplObjectStorage_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap = SosMap(pCtx->pVm,PH7_ContextThis(pCtx));
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx,pMap && pMap->pCur ? 1 : 0);
	return PH7_OK;
}
/* php's key() is the INDEX, a counter of its own: next() advances it past the end
 * too, and only rewind()/detach()/addAll() and friends put it back to zero. */
static int vm_builtin_SplObjectStorage_key(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),SOS_I));
	return PH7_OK;
}
/* php RAISES here rather than answering null: the chunk's null was a wrong answer
 * every `foreach` hid, because a foreach never asks past valid(). */
static int vm_builtin_SplObjectStorage_current(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pObj = SosPart(SosCurrentPair(pCtx->pVm,PH7_ContextThis(pCtx)),"obj");
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pObj == 0 ){
		return PH7_VmThrowException(pCtx,"RuntimeException",
			"Called current() on invalid iterator");
	}
	ph7_result_value(pCtx,pObj);
	return PH7_OK;
}
static int vm_builtin_SplObjectStorage_next(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_hashmap *pMap = SosMap(pVm,pThis);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pMap && pMap->pCur ){
		pMap->pCur = pMap->pCur->pPrev;
	}
	PH7_NativeSetAttrInt(pVm,pThis,SOS_I,PH7_NativeAttrInt(pThis,SOS_I) + 1);
	return PH7_OK;
}
/* php's getInfo(): null past the end, where current() raises. */
static int vm_builtin_SplObjectStorage_getInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pInf = SosPart(SosCurrentPair(pCtx->pVm,PH7_ContextThis(pCtx)),"inf");
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pInf ){
		ph7_result_value(pCtx,pInf);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
static int vm_builtin_SplObjectStorage_setInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_hashmap *pMap = SosMap(pVm,PH7_ContextThis(pCtx));
	ph7_value *pPair;
	if( pMap == 0 || pMap->pCur == 0 || nArg < 1 ){
		return PH7_OK;   /* php returns without touching anything */
	}
	pPair = SosPairForWrite(pVm,pMap->pCur);
	if( pPair ){
		SosSetPart(pVm,pPair,"inf",apArg[0]);
	}
	return PH7_OK;
}
/*
 * php's seek(): a position outside the storage is an OutOfBoundsException, and
 * the index follows the position exactly (php walks its hash cursor either way
 * and counts; the destination is the same).
 */
static int vm_builtin_SplObjectStorage_seek(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_hashmap *pMap = SosMap(pVm,pThis);
	ph7_int64 iPos = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;
	ph7_int64 i;
	if( pMap == 0 ){
		return PH7_OK;
	}
	if( iPos < 0 || iPos >= (ph7_int64)pMap->nEntry ){
		return PH7_VmThrowException(pCtx,"OutOfBoundsException",
			"Seek position %qd is out of range",iPos);
	}
	pMap->pCur = pMap->pFirst;
	for( i = 0 ; i < iPos && pMap->pCur ; ++i ){
		pMap->pCur = pMap->pCur->pPrev;
	}
	PH7_NativeSetAttrInt(pVm,pThis,SOS_I,iPos);
	return PH7_OK;
}
/*
 * php's get_debug_info: ONE entry, the storage, under its own MANGLED private key
 * -- `["storage":"SplObjectStorage":private]` on screen. The pairs are re-indexed
 * from zero and shown as {obj, inf}, which is the shape stored here already.
 * `__debugInfo()` is the same array, reachable by name because php declares it.
 */
static sxi32 SosFillDebug(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)
{
	ph7_hashmap *pMap = SosMap(pVm,pThis);
	ph7_hashmap_node *pNode;
	ph7_value sKey,sList;
	sxu32 n;
	PH7_MemObjInit(pVm,&sList);
	if( PH7_MemObjToHashmap(&sList) != SXRET_OK ){
		PH7_MemObjRelease(&sList);
		return SXERR_MEM;
	}
	if( pMap ){
		pNode = pMap->pFirst;
		for( n = 0 ; n < pMap->nEntry && pNode ; ++n ){
			ph7_value *pPair = HashmapExtractNodeValue(pNode);
			if( pPair ){
				ph7_array_add_elem(&sList,0,pPair);
			}
			pNode = pNode->pPrev;
		}
	}
	PH7_MemObjInitFromString(pVm,&sKey,0);
	PH7_MemObjStringAppend(&sKey,"\0SplObjectStorage\0storage",
		sizeof("\0SplObjectStorage\0storage")-1);
	ph7_array_add_elem(pOut,&sKey,&sList);
	PH7_MemObjRelease(&sKey);
	PH7_MemObjRelease(&sList);
	return PH7_OK;
}
static sxi32 SosPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)
{
	if( !bDebug ){
		return PH7_OK;   /* php's (array) cast and var_export show nothing */
	}
	return SosFillDebug(pVm,pThis,pOut);
}
static int vm_builtin_SplObjectStorage_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
	SosFillDebug(pVm,PH7_ContextThis(pCtx),&sOut);
	ph7_result_value(pCtx,&sOut);
	PH7_MemObjRelease(&sOut);
	return PH7_OK;
}
/*
 * php's __serialize(): [[obj, inf, obj, inf, …], members]. This is what serialize()
 * actually uses; the Serializable pair below is the legacy format nothing else in
 * php reads or writes. The members slot is the instance's own properties — empty for
 * a bare SplObjectStorage, a SUBCLASS's declared slots when there is one.
 */
static int vm_builtin_SplObjectStorage_serializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_hashmap *pMap = SosMap(pVm,pThis);
	ph7_hashmap_node *pNode;
	ph7_value sOut,sFlat,sMembers;
	sxu32 n;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	PH7_MemObjInit(pVm,&sOut);
	PH7_MemObjInit(pVm,&sFlat);
	if( SplMembersOf(pVm,pThis,&sMembers) != SXRET_OK
	 || PH7_MemObjToHashmap(&sOut) != SXRET_OK
	 || PH7_MemObjToHashmap(&sFlat) != SXRET_OK ){
		PH7_MemObjRelease(&sOut);
		PH7_MemObjRelease(&sFlat);
		PH7_MemObjRelease(&sMembers);
		return PH7_ContextMemoryError(pCtx);
	}
	if( pMap ){
		pNode = pMap->pFirst;
		for( n = 0 ; n < pMap->nEntry && pNode ; ++n ){
			ph7_value *pPair = HashmapExtractNodeValue(pNode);
			ph7_value *pObj = SosPart(pPair,"obj");
			ph7_value *pInf = SosPart(pPair,"inf");
			if( pObj ){
				ph7_array_add_elem(&sFlat,0,pObj);
				if( pInf ){
					ph7_array_add_elem(&sFlat,0,pInf);
				}
			}
			pNode = pNode->pPrev;
		}
	}
	ph7_array_add_elem(&sOut,0,&sFlat);
	ph7_array_add_elem(&sOut,0,&sMembers);
	ph7_result_value(pCtx,&sOut);
	PH7_MemObjRelease(&sOut);
	PH7_MemObjRelease(&sFlat);
	PH7_MemObjRelease(&sMembers);
	return PH7_OK;
}
static sxi32 SosIllTyped(ph7_context *pCtx)
{
	return PH7_VmThrowException(pCtx,"UnexpectedValueException",
		"Incomplete or ill-typed serialization data");
}
static int vm_builtin_SplObjectStorage_unserializeMagic(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_hashmap *pFlat;
	ph7_hashmap_node *pNode = 0;
	ph7_value *pStorage,*pMembers;
	sxi64 i,n;
	sxi32 rc = PH7_OK;
	if( nArg < 1 || (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 || pThis == 0 ){
		return SosIllTyped(pCtx);
	}
	if( HashmapLookupIntKey((ph7_hashmap *)apArg[0]->x.pOther,0,&pNode) != SXRET_OK ){
		return SosIllTyped(pCtx);
	}
	pStorage = HashmapExtractNodeValue(pNode);
	pNode = 0;
	if( HashmapLookupIntKey((ph7_hashmap *)apArg[0]->x.pOther,1,&pNode) != SXRET_OK ){
		return SosIllTyped(pCtx);
	}
	pMembers = HashmapExtractNodeValue(pNode);
	if( pStorage == 0 || (pStorage->iFlags & MEMOBJ_HASHMAP) == 0
	 || pMembers == 0 || (pMembers->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return SosIllTyped(pCtx);
	}
	pFlat = (ph7_hashmap *)pStorage->x.pOther;
	n = (sxi64)pFlat->nEntry;
	if( n % 2 != 0 ){
		return PH7_VmThrowException(pCtx,"UnexpectedValueException","Odd number of elements");
	}
	for( i = 0 ; i < n && rc == PH7_OK ; i += 2 ){
		ph7_value *pObj,*pInf;
		pNode = 0;
		if( HashmapLookupIntKey(pFlat,i,&pNode) != SXRET_OK ){
			return SosIllTyped(pCtx);
		}
		pObj = HashmapExtractNodeValue(pNode);
		if( pObj == 0 || (pObj->iFlags & MEMOBJ_OBJ) == 0 ){
			return PH7_VmThrowException(pCtx,"UnexpectedValueException","Non-object key");
		}
		pNode = 0;
		pInf = HashmapLookupIntKey(pFlat,i+1,&pNode) == SXRET_OK
			? HashmapExtractNodeValue(pNode) : 0;
		rc = SosAttach(pCtx,pThis,pObj,pInf);
	}
	if( rc == PH7_OK ){
		SplMembersLoad(pThis,pMembers);
	}
	return rc;
}
/*
 * php's Serializable pair, kept because the interface is still declared. The
 * format is `x:` + the serialized COUNT, then one `<obj>,<inf>;` per element, then
 * `m:` + the serialized members -- and php writes it through ONE serializer state,
 * so an object that appears twice becomes an `r:` back-reference there and a
 * second copy here (§10; the DLL's legacy pair has the same shape).
 */
static int vm_builtin_SplObjectStorage_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_hashmap *pMap = SosMap(pVm,PH7_ContextThis(pCtx));
	ph7_hashmap_node *pNode;
	SyBlob sOut;
	ph7_value sVal,*apCall[1];
	sxu32 n;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	SyBlobInit(&sOut,&pVm->sAllocator);
	SyBlobAppend(&sOut,"x:",sizeof("x:")-1);
	PH7_MemObjInitFromInt(pVm,&sVal,pMap ? (sxi64)pMap->nEntry : 0);
	apCall[0] = &sVal;
	SplSerializeInto(pCtx,apCall,&sOut);
	PH7_MemObjRelease(&sVal);
	if( pMap ){
		pNode = pMap->pFirst;
		for( n = 0 ; n < pMap->nEntry && pNode ; ++n ){
			ph7_value *pPair = HashmapExtractNodeValue(pNode);
			ph7_value *pObj = SosPart(pPair,"obj");
			ph7_value *pInf = SosPart(pPair,"inf");
			ph7_value sNull;
			if( pObj ){
				apCall[0] = pObj;
				SplSerializeInto(pCtx,apCall,&sOut);
				SyBlobAppend(&sOut,",",1);
				PH7_MemObjInit(pVm,&sNull);
				apCall[0] = pInf ? pInf : &sNull;
				SplSerializeInto(pCtx,apCall,&sOut);
				PH7_MemObjRelease(&sNull);
				SyBlobAppend(&sOut,";",1);
			}
			pNode = pNode->pPrev;
		}
	}
	SyBlobAppend(&sOut,"m:",sizeof("m:")-1);
	PH7_MemObjInit(pVm,&sVal);
	if( PH7_MemObjToHashmap(&sVal) == SXRET_OK ){
		apCall[0] = &sVal;
		SplSerializeInto(pCtx,apCall,&sOut);
	}
	PH7_MemObjRelease(&sVal);
	/* ph7_result_string APPENDS too, and pRet still holds the last nested answer
	 * (rule 54) -- drop it before writing this one. */
	if( pCtx->pRet ){
		PH7_MemObjRelease(pCtx->pRet);
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
/* php reports WHERE its parse gave up, in bytes, and every failure below is that
 * one exception. */
static sxi32 SosOffsetErr(ph7_context *pCtx,int nAt,int nTotal)
{
	return PH7_VmThrowException(pCtx,"UnexpectedValueException",
		"Error at offset %d of %d bytes",nAt,nTotal);
}
static int vm_builtin_SplObjectStorage_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zData;
	int nData = 0,nAt = 0,nRead = 0;
	ph7_value sVal;
	sxi64 nCount,i;
	sxi32 rc;
	if( nArg < 1 || pThis == 0 ){
		return PH7_OK;
	}
	zData = ph7_value_to_string(apArg[0],&nData);
	if( nData < 1 ){
		return PH7_OK;   /* php returns without touching the storage */
	}
	if( nData < 2 || zData[0] != 'x' || zData[1] != ':' ){
		return SosOffsetErr(pCtx,zData[0] == 'x' ? 1 : 0,nData);
	}
	nAt = 2;
	PH7_MemObjInit(pVm,&sVal);
	rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sVal);
	if( rc == PH7_EXCEPTION ){
		PH7_MemObjRelease(&sVal);
		return rc;
	}
	if( rc != SXRET_OK || (sVal.iFlags & MEMOBJ_INT) == 0 ){
		/* php reports where its parser STOPPED, which for a well-formed value of
		 * the wrong type is the byte after it. */
		PH7_MemObjRelease(&sVal);
		return SosOffsetErr(pCtx,nAt + nRead,nData);
	}
	nCount = ph7_value_to_int64(&sVal);
	PH7_MemObjRelease(&sVal);
	nAt += nRead - 1;   /* php steps back onto the ';' that ends the count */
	if( nCount < 0 ){
		return SosOffsetErr(pCtx,nAt,nData);
	}
	for( i = 0 ; i < nCount ; ++i ){
		ph7_value sObj,sInf;
		if( nAt >= nData || zData[nAt] != ';' ){
			return SosOffsetErr(pCtx,nAt,nData);
		}
		nAt++;
		if( nAt >= nData || (zData[nAt] != 'O' && zData[nAt] != 'C' && zData[nAt] != 'r') ){
			return SosOffsetErr(pCtx,nAt,nData);
		}
		PH7_MemObjInit(pVm,&sObj);
		PH7_MemObjInit(pVm,&sInf);
		rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sObj);
		if( rc == PH7_EXCEPTION ){
			PH7_MemObjRelease(&sObj);
			PH7_MemObjRelease(&sInf);
			return rc;
		}
		if( rc != SXRET_OK || (sObj.iFlags & MEMOBJ_OBJ) == 0 ){
			PH7_MemObjRelease(&sObj);
			PH7_MemObjRelease(&sInf);
			return SosOffsetErr(pCtx,nAt + nRead,nData);
		}
		nAt += nRead;
		if( nAt < nData && zData[nAt] == ',' ){
			nAt++;
			rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sInf);
			if( rc == PH7_EXCEPTION ){
				PH7_MemObjRelease(&sObj);
				PH7_MemObjRelease(&sInf);
				return rc;
			}
			if( rc != SXRET_OK ){
				PH7_MemObjRelease(&sObj);
				PH7_MemObjRelease(&sInf);
				return SosOffsetErr(pCtx,nAt + nRead,nData);
			}
			nAt += nRead;
		}
		rc = SosAttach(pCtx,pThis,&sObj,&sInf);
		PH7_MemObjRelease(&sObj);
		PH7_MemObjRelease(&sInf);
		if( rc != PH7_OK ){
			return rc;
		}
	}
	if( nAt >= nData || zData[nAt] != ';' ){
		return SosOffsetErr(pCtx,nAt,nData);
	}
	nAt++;
	if( nAt + 1 >= nData || zData[nAt] != 'm' || zData[nAt+1] != ':' ){
		return SosOffsetErr(pCtx,nAt < nData && zData[nAt] == 'm' ? nAt + 1 : nAt,nData);
	}
	nAt += 2;
	PH7_MemObjInit(pVm,&sVal);
	rc = PH7_VmUnserializeOne(pCtx,&zData[nAt],nData - nAt,&nRead,&sVal);
	if( rc == PH7_EXCEPTION ){
		PH7_MemObjRelease(&sVal);
		return rc;
	}
	if( rc != SXRET_OK || (sVal.iFlags & MEMOBJ_HASHMAP) == 0 ){
		PH7_MemObjRelease(&sVal);
		return SosOffsetErr(pCtx,nAt + nRead,nData);
	}
	/* php loads the members onto the object here; a native class declares none
	 * that a payload could name and PHL has no dynamic properties to create. */
	PH7_MemObjRelease(&sVal);
	return PH7_OK;
}
/*
 * The declaration. Method ORDER is spl_observer.stub.php's, the two observer
 * interfaces are methodless-but-typed contracts php declares beside it, and
 * seek() is the ONE method php does not mark tentative.
 */
static sxi32 VmInstallSplObjectStorage(ph7_vm *pVm)
{
	static const PH7_NativeMethodDef aObserverMethod[] = {
		{ "update", PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "SplSubject $subject", "@void", 0 },
	};
	static const PH7_NativeMethodDef aSubjectMethod[] = {
		{ "attach", PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "SplObserver $observer", "@void", 0 },
		{ "detach", PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "SplObserver $observer", "@void", 0 },
		{ "notify", PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "@void", 0 },
	};
	static const PH7_NativePropDef aSosProp[] = {
		{ SOS_S, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ SOS_I, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeMethodDef aSosMethod[] = {
		{ "attach",          PH7_MOD_PUBLIC, "object $object, mixed $info = null", "@void",
		  vm_builtin_SplObjectStorage_attach },
		{ "detach",          PH7_MOD_PUBLIC, "object $object", "@void",
		  vm_builtin_SplObjectStorage_detach },
		{ "contains",        PH7_MOD_PUBLIC, "object $object", "@bool",
		  vm_builtin_SplObjectStorage_contains },
		{ "addAll",          PH7_MOD_PUBLIC, "SplObjectStorage $storage", "@int",
		  vm_builtin_SplObjectStorage_addAll },
		{ "removeAll",       PH7_MOD_PUBLIC, "SplObjectStorage $storage", "@int",
		  vm_builtin_SplObjectStorage_removeAll },
		{ "removeAllExcept", PH7_MOD_PUBLIC, "SplObjectStorage $storage", "@int",
		  vm_builtin_SplObjectStorage_removeAllExcept },
		{ "getInfo",         PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_SplObjectStorage_getInfo },
		{ "setInfo",         PH7_MOD_PUBLIC, "mixed $info", "@void",
		  vm_builtin_SplObjectStorage_setInfo },
		{ "count",           PH7_MOD_PUBLIC, "int $mode = 0", "@int",
		  vm_builtin_SplObjectStorage_count },
		{ "rewind",          PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplObjectStorage_rewind },
		{ "valid",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplObjectStorage_valid },
		{ "key",             PH7_MOD_PUBLIC, "", "@int", vm_builtin_SplObjectStorage_key },
		{ "current",         PH7_MOD_PUBLIC, "", "@object", vm_builtin_SplObjectStorage_current },
		{ "next",            PH7_MOD_PUBLIC, "", "@void", vm_builtin_SplObjectStorage_next },
		{ "seek",            PH7_MOD_PUBLIC, "int $offset", "void",
		  vm_builtin_SplObjectStorage_seek },
		{ "unserialize",     PH7_MOD_PUBLIC, "string $data", "@void",
		  vm_builtin_SplObjectStorage_unserialize },
		{ "serialize",       PH7_MOD_PUBLIC, "", "@string",
		  vm_builtin_SplObjectStorage_serialize },
		/* php's stub leaves these four offsets UNTYPED (a `@param object` docblock
		 * Reflection does not print) while the ZPP takes an object -- so the
		 * signature says nothing and each body words its own refusal. */
		{ "offsetExists",    PH7_MOD_PUBLIC, "$object", "@bool",
		  vm_builtin_SplObjectStorage_offsetExists },
		{ "offsetGet",       PH7_MOD_PUBLIC, "$object", "@mixed",
		  vm_builtin_SplObjectStorage_offsetGet },
		{ "offsetSet",       PH7_MOD_PUBLIC, "$object, mixed $info = null", "@void",
		  vm_builtin_SplObjectStorage_offsetSet },
		{ "offsetUnset",     PH7_MOD_PUBLIC, "$object", "@void",
		  vm_builtin_SplObjectStorage_offsetUnset },
		{ "getHash",         PH7_MOD_PUBLIC, "object $object", "@string",
		  vm_builtin_SplObjectStorage_getHash },
		{ "__serialize",     PH7_MOD_PUBLIC, "", "@array",
		  vm_builtin_SplObjectStorage_serializeMagic },
		{ "__unserialize",   PH7_MOD_PUBLIC, "array $data", "@void",
		  vm_builtin_SplObjectStorage_unserializeMagic },
		{ "__debugInfo",     PH7_MOD_PUBLIC, "", "@array",
		  vm_builtin_SplObjectStorage_debugInfo },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "SplObserver", 0, 0, PH7_CLASS_INTERFACE,
		  aObserverMethod, SX_ARRAYSIZE(aObserverMethod), 0, 0, 0, 0, 0, 0, 0 },
		{ "SplSubject", 0, 0, PH7_CLASS_INTERFACE,
		  aSubjectMethod, SX_ARRAYSIZE(aSubjectMethod), 0, 0, 0, 0, 0, 0, 0 },
		{ "SplObjectStorage", 0, "Countable,SeekableIterator,Serializable,ArrayAccess", 0,
		  aSosMethod, SX_ARRAYSIZE(aSosMethod), 0, 0,
		  aSosProp, SX_ARRAYSIZE(aSosProp), 0, 0, SosPresent },
	};
	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
}
/*
 * ---------------------------------------------------------------------------
 * SplFileInfo.
 *
 * php's `spl_filesystem_object` keeps TWO strings for a path, and which one a
 * method reads is the whole model: `file_name` is the pathname with any trailing
 * slashes stripped, and `path` is everything before the LAST slash of it -- which
 * is EMPTY when the name has no slash before its last component, so
 * `(new SplFileInfo('/a.txt'))->getPath()` is `''` and `getFilename()` answers the
 * whole `/a.txt`. Every accessor is a slice of that pair (php's own
 * `spl_filesystem_info_set_filename`), and the chunk, which called `basename()` and
 * `dirname()` per method instead, disagreed on all of it.
 *
 * The stat family is php's `FileInfoFunction` macro: `php_stat()` with the error
 * handler REPLACED, so the warning a failed stat would print becomes a
 * `RuntimeException` instead -- `getSize()` on a missing file RAISES there and
 * warned-then-answered-false here. Two of the fifteen lstat rather than stat
 * (`getType`, `isLink`), which is php's IS_LINK_OPERATION set.
 *
 * The two slots are PRIVATE and PRESENTED: php declares no properties at all
 * (`getProperties()`, the `(array)` cast and `get_object_vars()` are empty) while
 * `var_dump` shows `pathName`/`fileName` under their mangled private keys, and
 * `__debugInfo()` hands back that same array. The class is `@not-serializable`.
 *
 * NOT converted, because they need a class PHL does not have: `openFile()` and
 * `setFileClass()` answer an `SplFileObject`. `setInfoClass()` and the `?string
 * $class` argument of `getFileInfo()`/`getPathInfo()` are here in full -- they
 * only ever name a class derived from this one.
 */
#define SFI_N  "__n"   /* php's file_name: the pathname, trailing slashes stripped */
#define SFI_P  "__p"   /* php's path: everything before its last slash */
#define SFI_IC "__ic"  /* php's info_class */
/* The directory-iterator half of php's struct, on the same instance: its `u.dir`
 * arm minus the handle, which cannot live in a php-visible slot (see VmDirHandle).
 * Declared by DirectoryIterator, so `SplDirIs()` is what tells the two apart. */
#define SDI_E  "__e"   /* php's u.dir.entry.d_name; "" once the walk has run out */
#define SDI_I  "__i"   /* php's u.dir.index: what key() answers */
#define SDI_F  "__f"   /* php's flags */
#define SDI_S  "__s"   /* php's u.dir.sub_path (RecursiveDirectoryIterator) */

/* php's IS_SLASH is PLATFORM-dependent: a backslash separates on Windows and is an
 * ordinary filename byte everywhere else, which is why `new SplFileInfo('C:\\x\\y')`
 * has an empty path on unix. PH7_ExtractDirName draws the same line. */
#ifdef __WINNT__
# define SFI_IS_SLASH(c) ((c) == '/' || (c) == '\\')
#else
# define SFI_IS_SLASH(c) ((c) == '/')
#endif

/*
 * The directory-iterator half of this family, declared up here because php's
 * SplFileInfo bodies BRANCH on `spl_filesystem_object::type`: a DIR instance
 * keeps its pathname lazily (path + slash + the current entry, rebuilt after
 * every read) and answers nothing at all once the walk has run out. Exactly
 * five accessors below ask, which is the same five php branches in.
 */
static int SplDirIs(ph7_vm *pVm,ph7_class_instance *pThis);
static const char * SplDirName(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen);
static int SplDirAtEnd(ph7_class_instance *pThis);
static VmDirHandle * SplDirState(ph7_vm *pVm,ph7_class_instance *pThis);
/* One of the two path slots, as bytes. */
static const char * SfiStr(ph7_class_instance *pThis,const char *zSlot,int *pnLen)
{
	ph7_value *pVal = pThis ? PH7_NativeAttr(pThis,zSlot) : 0;
	*pnLen = 0;
	if( pVal == 0 ){
		return "";
	}
	return ph7_value_to_string(pVal,pnLen);
}
/*
 * php's spl_filesystem_info_set_filename: strip the trailing slashes (never the
 * only character), then cut the path at the last slash of what is left. A name
 * with no slash before its final component keeps an EMPTY path, which is what
 * makes getFilename() answer the whole thing.
 */
static void SfiSetName(ph7_vm *pVm,ph7_class_instance *pThis,const char *zPath,int nPath)
{
	int nFile = nPath;
	int nDir;
	if( nFile > 1 && SFI_IS_SLASH(zPath[nFile-1]) ){
		do{
			nFile--;
		}while( nFile > 1 && SFI_IS_SLASH(zPath[nFile-1]) );
	}
	nDir = nFile;
	while( nDir > 1 && !SFI_IS_SLASH(zPath[nDir-1]) ){
		nDir--;
	}
	if( nDir > 0 ){
		nDir--;
	}
	PH7_NativeSetAttrStr(pVm,pThis,SFI_N,zPath,nFile);
	PH7_NativeSetAttrStr(pVm,pThis,SFI_P,zPath,nDir);
}
/*
 * php's `file_name`: the slot for a plain SplFileInfo, and the lazily rebuilt
 * path+slash+entry for a directory iterator. Every accessor that works on the
 * whole pathname goes through here.
 */
static const char * SfiName(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen)
{
	if( SplDirIs(pVm,pThis) ){
		return SplDirName(pVm,pThis,pnLen);
	}
	return SfiStr(pThis,SFI_N,pnLen);
}
/*
 * php's "the file name without the path": the slice after `path` + its slash when
 * the path is a real prefix, and the whole name otherwise. getFilename(),
 * getBasename() and getExtension() all start here.
 */
static const char * SfiTail(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen)
{
	int nName = 0,nPath = 0;
	const char *zName = SfiName(pVm,pThis,&nName);
	SfiStr(pThis,SFI_P,&nPath);
	if( nPath > 0 && nPath < nName ){
		*pnLen = nName - (nPath + 1);
		return &zName[nPath + 1];
	}
	*pnLen = nName;
	return zName;
}
/* The path this instance stands for, as a NUL-terminated buffer the VFS can take. */
static sxi32 SfiPathBuf(ph7_vm *pVm,ph7_class_instance *pThis,char *zBuf,int nBuf)
{
	int nName = 0;
	const char *zName = SfiName(pVm,pThis,&nName);
	if( nName < 1 || nName >= nBuf ){
		return SXERR_INVALID;
	}
	SyMemcpy(zName,zBuf,(sxu32)nName);
	zBuf[nName] = 0;
	return SXRET_OK;
}
/*
 * php's get_file_name() ahead of an accessor that needs a path: an object whose
 * parent constructor never ran has no name AT ALL and raises Error rather than
 * failing a stat -- which for a directory iterator is the case where the open
 * never happened. Answers 0 when the caller must return *pRc.
 */
static int SfiDirReady(ph7_context *pCtx,sxi32 *pRc)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	*pRc = PH7_OK;
	if( SplDirIs(pVm,pThis) && SplDirState(pVm,pThis) == 0 ){
		*pRc = PH7_VmThrowException(pCtx,"Error","Object not initialized");
		return 0;
	}
	return 1;
}
/*
 * php's FileInfoFunction: the stat that backs one accessor, with the failure
 * promoted to a RuntimeException carrying the WARNING php would otherwise print.
 * The two lstat users say "Lstat failed" there, which is php's own text.
 */
static sxi32 SfiStat(ph7_context *pCtx,const char *zMethod,int bLstat,ph7_value *pOut)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const ph7_vfs *pVfs = pVm->pEngine->pVfs;
	ph7_value sWorker;
	char zPath[4096];
	int rc = -1;
	sxi32 rcReady;
	if( !SfiDirReady(pCtx,&rcReady) ){
		return rcReady;
	}
	if( PH7_MemObjToHashmap(pOut) != SXRET_OK ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_MemObjInit(pVm,&sWorker);
	if( SfiPathBuf(pVm,pThis,zPath,(int)sizeof(zPath)) == SXRET_OK && pVfs ){
		if( bLstat ){
			rc = pVfs->xlStat ? pVfs->xlStat(zPath,pOut,&sWorker) : -1;
		}else{
			rc = pVfs->xStat ? pVfs->xStat(zPath,pOut,&sWorker) : -1;
		}
	}
	PH7_MemObjRelease(&sWorker);
	if( rc != PH7_OK ){
		int nName = 0;
		const char *zName = SfiName(pVm,pThis,&nName);
		return PH7_VmThrowException(pCtx,"RuntimeException",
			"SplFileInfo::%s(): %s failed for %.*s",zMethod,bLstat ? "Lstat" : "stat",
			nName,zName);
	}
	return PH7_OK;
}
/* One field of a stat array, as php's int. */
static int SfiStatField(ph7_context *pCtx,const char *zMethod,int bLstat,const char *zField,
	sxi64 *piOut)
{
	ph7_value sStat,*pField;
	sxi32 rc;
	*piOut = 0;
	PH7_MemObjInit(pCtx->pVm,&sStat);
	rc = SfiStat(pCtx,zMethod,bLstat,&sStat);
	if( rc != PH7_OK ){
		PH7_MemObjRelease(&sStat);
		return rc;
	}
	pField = ph7_array_fetch(&sStat,zField,(int)SyStrlen(zField));
	if( pField ){
		*piOut = ph7_value_to_int64(pField);
	}
	PH7_MemObjRelease(&sStat);
	return PH7_OK;
}
/* The eight stat accessors that answer an int, all with the same body. */
static int SfiStatInt(ph7_context *pCtx,const char *zMethod,const char *zField)
{
	sxi64 iVal = 0;
	sxi32 rc = SfiStatField(pCtx,zMethod,FALSE,zField,&iVal);
	if( rc != PH7_OK ){
		return rc;
	}
	ph7_result_int64(pCtx,iVal);
	return PH7_OK;
}
static int vm_builtin_SplFileInfo_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zPath;
	int nPath = 0;
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	zPath = ph7_value_to_string(apArg[0],&nPath);
	SfiSetName(pVm,pThis,zPath,nPath);
	return PH7_OK;
}
static int vm_builtin_SplFileInfo_getPath(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int nPath = 0;
	const char *zPath = SfiStr(PH7_ContextThis(pCtx),SFI_P,&nPath);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_string(pCtx,zPath,nPath);
	return PH7_OK;
}
/*
 * php's getPathname() is `spl_filesystem_object_get_pathname`, and for a DIR it
 * answers NOTHING once the walk has run out — the empty string, without
 * materializing the lazy name the stat family would still build (`getSize()`
 * past the end stats the directory itself, and `var_dump` shows the difference).
 */
static int vm_builtin_SplFileInfo_getPathname(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	int nName = 0;
	const char *zName;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( SplDirIs(pCtx->pVm,pThis) && SplDirAtEnd(pThis) ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	zName = SfiName(pCtx->pVm,pThis,&nName);
	ph7_result_string(pCtx,zName,nName);
	return PH7_OK;
}
static int vm_builtin_SplFileInfo_getFilename(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int nTail = 0;
	const char *zTail = SfiTail(pCtx->pVm,PH7_ContextThis(pCtx),&nTail);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_string(pCtx,zTail,nTail);
	return PH7_OK;
}
/* php's getBasename(): php_basename() of the tail, suffix rule included. */
static int vm_builtin_SplFileInfo_getBasename(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int nTail = 0,nBase = 0;
	const char *zTail = SfiTail(pCtx->pVm,PH7_ContextThis(pCtx),&nTail);
	const char *zBase = PH7_ExtractBaseName(zTail,nTail,&nBase);
	if( nArg > 0 ){
		int nSuffix = 0;
		const char *zSuffix = ph7_value_to_string(apArg[0],&nSuffix);
		if( nSuffix > 0 && nSuffix < nBase
		 && SyMemcmp(&zBase[nBase - nSuffix],zSuffix,(sxu32)nSuffix) == 0 ){
			nBase -= nSuffix;
		}
	}
	ph7_result_string(pCtx,zBase,nBase);
	return PH7_OK;
}
/* php's getExtension(): everything after the LAST dot of the basename, and the
 * empty string when there is none -- a leading dot counts, so '.hidden' has the
 * extension 'hidden'. */
static int vm_builtin_SplFileInfo_getExtension(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int nTail = 0,nBase = 0,i;
	const char *zTail = SfiTail(pCtx->pVm,PH7_ContextThis(pCtx),&nTail);
	const char *zBase = PH7_ExtractBaseName(zTail,nTail,&nBase);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	for( i = nBase - 1 ; i >= 0 ; --i ){
		if( zBase[i] == '.' ){
			ph7_result_string(pCtx,&zBase[i+1],nBase - i - 1);
			return PH7_OK;
		}
	}
	ph7_result_string(pCtx,"",0);
	return PH7_OK;
}
static int vm_builtin_SplFileInfo_getPerms(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	return SfiStatInt(pCtx,"getPerms","mode");
}
static int vm_builtin_SplFileInfo_getInode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	return SfiStatInt(pCtx,"getInode","ino");
}
static int vm_builtin_SplFileInfo_getSize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	return SfiStatInt(pCtx,"getSize","size");
}
static int vm_builtin_SplFileInfo_getOwner(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	return SfiStatInt(pCtx,"getOwner","uid");
}
static int vm_builtin_SplFileInfo_getGroup(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	return SfiStatInt(pCtx,"getGroup","gid");
}
static int vm_builtin_SplFileInfo_getATime(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	return SfiStatInt(pCtx,"getATime","atime");
}
static int vm_builtin_SplFileInfo_getMTime(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	return SfiStatInt(pCtx,"getMTime","mtime");
}
static int vm_builtin_SplFileInfo_getCTime(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	return SfiStatInt(pCtx,"getCTime","ctime");
}
/*
 * php's getType() is FS_TYPE: an LSTAT, so a symlink answers "link" rather than
 * what it points at. The VFS's own xFiletype IS that question -- decoding a stat
 * mode here instead would have answered "unknown" on Windows, where the mode
 * field is not filled and the attributes are what carry the answer.
 */
static int vm_builtin_SplFileInfo_getType(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	char zPath[4096];
	int rc = -1;
	sxi32 rcReady;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !SfiDirReady(pCtx,&rcReady) ){
		return rcReady;
	}
	if( pVfs && pVfs->xFiletype
	 && SfiPathBuf(pCtx->pVm,pThis,zPath,(int)sizeof(zPath)) == SXRET_OK ){
		rc = pVfs->xFiletype(zPath,pCtx);
	}
	if( rc != PH7_OK ){
		int nName = 0;
		const char *zName = SfiName(pCtx->pVm,pThis,&nName);
		if( pCtx->pRet ){
			PH7_MemObjRelease(pCtx->pRet);   /* xFiletype wrote "unknown" (rule 54) */
		}
		return PH7_VmThrowException(pCtx,"RuntimeException",
			"SplFileInfo::getType(): Lstat failed for %.*s",nName,zName);
	}
	return PH7_OK;
}
/* The six predicates: a VFS question each, and never a diagnostic -- php answers
 * false for a path that does not exist. */
static int SfiPredicate(ph7_context *pCtx,int (*xTest)(const char *))
{
	char zPath[4096];
	int bYes = 0;
	sxi32 rcReady;
	if( !SfiDirReady(pCtx,&rcReady) ){
		return rcReady;
	}
	if( xTest
	 && SfiPathBuf(pCtx->pVm,PH7_ContextThis(pCtx),zPath,(int)sizeof(zPath)) == SXRET_OK ){
		bYes = xTest(zPath) == PH7_OK;
	}
	ph7_result_bool(pCtx,bYes);
	return PH7_OK;
}
static int vm_builtin_SplFileInfo_isWritable(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xWritable : 0);
}
static int vm_builtin_SplFileInfo_isReadable(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xReadable : 0);
}
static int vm_builtin_SplFileInfo_isExecutable(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xExecutable : 0);
}
static int vm_builtin_SplFileInfo_isFile(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xIsfile : 0);
}
static int vm_builtin_SplFileInfo_isDir(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xIsdir : 0);
}
static int vm_builtin_SplFileInfo_isLink(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	return SfiPredicate(pCtx,pCtx->pVm->pEngine->pVfs ? pCtx->pVm->pEngine->pVfs->xIslink : 0);
}
/* php's getLinkTarget(): readlink(), and a RuntimeException naming the errno text
 * when it fails -- which includes asking a plain file for its target. */
static int vm_builtin_SplFileInfo_getLinkTarget(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	char zPath[4096];
	int rc = -1;
	sxi32 rcReady;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !SfiDirReady(pCtx,&rcReady) ){
		return rcReady;
	}
	if( pVfs && pVfs->xReadlink
	 && SfiPathBuf(pCtx->pVm,pThis,zPath,(int)sizeof(zPath)) == SXRET_OK ){
		rc = pVfs->xReadlink(zPath,pCtx);
	}
	if( rc != PH7_OK ){
		int nName = 0;
		const char *zName = SfiName(pCtx->pVm,pThis,&nName);
		return PH7_VmThrowException(pCtx,"RuntimeException",
			"Unable to read link %.*s, error: %s",nName,zName,VfsStrerror(errno));
	}
	return PH7_OK;
}
static int vm_builtin_SplFileInfo_getRealPath(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;
	char zPath[4096];
	int rc = -1;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pVfs && pVfs->xRealpath
	 && SfiPathBuf(pCtx->pVm,PH7_ContextThis(pCtx),zPath,(int)sizeof(zPath)) == SXRET_OK ){
		rc = pVfs->xRealpath(zPath,pCtx);
	}
	if( rc != PH7_OK ){
		ph7_result_bool(pCtx,0);   /* php answers false, with no diagnostic */
	}
	return PH7_OK;
}
/*
 * The class getFileInfo()/getPathInfo() build with: the argument when it names
 * one, this instance's info_class otherwise. php refuses anything not derived
 * from SplFileInfo, and words the refusal from the ARGUMENT position.
 */
static sxi32 SfiInfoClass(ph7_context *pCtx,const char *zMethod,ph7_value *pArg,
	ph7_class **ppOut)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class *pBase = PH7_VmExtractClass(pVm,"SplFileInfo",sizeof("SplFileInfo")-1,FALSE,0);
	ph7_class *pClass = 0;
	const char *zName;
	int nName = 0;
	if( pArg && (pArg->iFlags & MEMOBJ_NULL) == 0 ){
		zName = ph7_value_to_string(pArg,&nName);
		pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,TRUE,0);
		if( pClass == 0 || pBase == 0 || !PH7_VmInstanceOf(pClass,pBase) ){
			return PH7_VmThrowException(pCtx,"TypeError",
				"SplFileInfo::%s(): Argument #1 ($class) must be a class name derived "
				"from SplFileInfo or null, %.*s given",zMethod,nName,zName);
		}
	}else{
		int nCur = 0;
		const char *zCur = SfiStr(pThis,SFI_IC,&nCur);
		pClass = PH7_VmExtractClass(pVm,zCur,(sxu32)nCur,TRUE,0);
	}
	if( pClass == 0 ){
		pClass = pBase;
	}
	*ppOut = pClass;
	return pClass ? PH7_OK : PH7_ContextMemoryError(pCtx);
}
/*
 * Build one of these for a path. php calls the CONSTRUCTOR when the class
 * declares its own (a subclass may want it) and fills the slots directly when it
 * does not -- reproduced here, because a subclass constructor is user code and
 * skipping it would be visible.
 */
static sxi32 SfiMakeInfoEx(ph7_context *pCtx,ph7_class *pClass,const char *zPath,int nPath,
	const char *zDir,int nDir)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pNew;
	ph7_class_method *pCons;
	sxi32 rc = SXRET_OK;
	pNew = PH7_NewClassInstance(pVm,pClass);
	if( pNew == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	pNew->iRef++;
	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);
	if( pCons && (pCons->sFunc.iFlags & VM_FUNC_NATIVE) == 0 ){
		ph7_value sArg,*apArg[1];
		PH7_MemObjInitFromString(pVm,&sArg,0);
		PH7_MemObjStringAppend(&sArg,zPath,(sxu32)nPath);
		apArg[0] = &sArg;
		rc = PH7_VmCallClassMethod(pVm,pNew,pCons,0,1,apArg);
		PH7_MemObjRelease(&sArg);
	}else if( zDir ){
		/* php's create_type for a DIR source hands the child BOTH strings rather
		 * than re-deriving the second: the path is the directory being walked, so
		 * `new DirectoryIterator('/')`'s entry keeps the path `/` and the name
		 * `//x` that the walk itself produced. */
		PH7_NativeSetAttrStr(pVm,pNew,SFI_N,zPath,nPath);
		PH7_NativeSetAttrStr(pVm,pNew,SFI_P,zDir,nDir);
	}else{
		SfiSetName(pVm,pNew,zPath,nPath);
	}
	if( rc != SXRET_OK ){
		PH7_ClassInstanceUnref(pNew);
		return rc;
	}
	PH7_NativeResultObject(pCtx,pNew);
	PH7_ClassInstanceUnref(pNew);
	return PH7_OK;
}
static sxi32 SfiMakeInfo(ph7_context *pCtx,ph7_class *pClass,const char *zPath,int nPath)
{
	return SfiMakeInfoEx(pCtx,pClass,zPath,nPath,0,0);
}
static int vm_builtin_SplFileInfo_getFileInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class *pClass = 0;
	int nName = 0,nDir = 0;
	const char *zName,*zDir = 0;
	sxi32 rc = SfiInfoClass(pCtx,"getFileInfo",nArg > 0 ? apArg[0] : 0,&pClass);
	if( rc != PH7_OK ){
		return rc;
	}
	if( SplDirIs(pVm,pThis) ){
		if( SplDirState(pVm,pThis) == 0 ){
			return PH7_VmThrowException(pCtx,"Error","Object not initialized");
		}
		/* php's create_type refuses to describe an entry that is not there —
		 * the same RuntimeException a FilesystemIterator::current() past the end
		 * raises, because it goes through this. */
		if( SplDirAtEnd(pThis) ){
			return PH7_VmThrowException(pCtx,"RuntimeException","Could not open file");
		}
		zDir = SfiStr(pThis,SFI_P,&nDir);
	}
	zName = SfiName(pVm,pThis,&nName);
	return SfiMakeInfoEx(pCtx,pClass,zName,nName,zDir,nDir);
}
/* php's getPathInfo(): the DIRNAME of the pathname, and nothing at all (null) for
 * an empty one — which for a directory iterator includes one that has run out. */
static int vm_builtin_SplFileInfo_getPathInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class *pClass = 0;
	int nName = 0,nDir = 0;
	const char *zName,*zDir;
	sxi32 rc = SfiInfoClass(pCtx,"getPathInfo",nArg > 0 ? apArg[0] : 0,&pClass);
	if( rc != PH7_OK ){
		return rc;
	}
	if( SplDirIs(pVm,pThis) && SplDirAtEnd(pThis) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	zName = SfiName(pVm,pThis,&nName);
	if( nName < 1 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	zDir = PH7_ExtractDirName(zName,nName,&nDir);
	return SfiMakeInfo(pCtx,pClass,zDir,nDir);
}
static int vm_builtin_SplFileInfo_setInfoClass(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class *pBase = PH7_VmExtractClass(pVm,"SplFileInfo",sizeof("SplFileInfo")-1,FALSE,0);
	ph7_class *pClass;
	const char *zName = "SplFileInfo";
	int nName = (int)sizeof("SplFileInfo")-1;
	if( nArg > 0 ){
		zName = ph7_value_to_string(apArg[0],&nName);
	}
	pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,TRUE,0);
	if( pClass == 0 || pBase == 0 || !PH7_VmInstanceOf(pClass,pBase) ){
		/* php words this one WITHOUT the "or null" half getFileInfo() has: the
		 * parameter is not nullable here. */
		return PH7_VmThrowException(pCtx,"TypeError",
			"SplFileInfo::setInfoClass(): Argument #1 ($class) must be a class name "
			"derived from SplFileInfo, %.*s given",nName,zName);
	}
	PH7_NativeSetAttrStr(pVm,pThis,SFI_IC,zName,nName);
	return PH7_OK;
}
/* One `"\0Class\0member" => <string>` entry of a debug array. */
static void SfiDebugStr(ph7_vm *pVm,ph7_value *pOut,const char *zKey,int nKey,
	const char *zVal,int nVal)
{
	ph7_value sKey,sVal;
	PH7_MemObjInitFromString(pVm,&sKey,0);
	PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKey);
	PH7_MemObjInitFromString(pVm,&sVal,0);
	PH7_MemObjStringAppend(&sVal,zVal,(sxu32)nVal);
	ph7_array_add_elem(pOut,&sKey,&sVal);
	PH7_MemObjRelease(&sKey);
	PH7_MemObjRelease(&sVal);
}
/*
 * php's get_debug_info: the two slots under their MANGLED private names, which is
 * how a class with no declared properties still shows something. __debugInfo()
 * hands back the same array.
 *
 * A DIRECTORY iterator shows two more (`glob`, always false here — PHL has no
 * GlobIterator — and `subPathName`), and shows `fileName` only if the pathname
 * has been MATERIALIZED: php's `if (intern->file_name)` is the lazy name's
 * presence, so an exhausted iterator has one key fewer until something asks it
 * for a path.
 */
static sxi32 SfiFillDebug(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut)
{
	/* php's test is `type == SPL_FS_DIR`, which an object whose constructor never
	 * ran does NOT satisfy: it shows the single `pathName` key a bare SplFileInfo
	 * would, and neither of the two directory ones. */
	int bDir = SplDirIs(pVm,pThis) && SplDirState(pVm,pThis) != 0;
	int nName = 0,nTail = 0,nSub = 0;
	const char *zName;
	int bLive = bDir ? !SplDirAtEnd(pThis) : !SplDirIs(pVm,pThis);
	if( bLive ){
		zName = SfiName(pVm,pThis,&nName);
	}else{
		zName = SfiStr(pThis,SFI_N,&nName);   /* whatever a stat left behind, or "" */
		nName = 0;
	}
	SfiDebugStr(pVm,pOut,"\0SplFileInfo\0pathName",
		(int)sizeof("\0SplFileInfo\0pathName")-1,zName,nName);
	/* Re-read: the append above may have moved the slot the first read borrowed. */
	SfiStr(pThis,SFI_N,&nName);
	if( bLive || (bDir && nName > 0) ){
		const char *zTail = SfiTail(pVm,pThis,&nTail);
		SfiDebugStr(pVm,pOut,"\0SplFileInfo\0fileName",
			(int)sizeof("\0SplFileInfo\0fileName")-1,zTail,nTail);
	}
	if( bDir ){
		ph7_value sKey,sVal;
		const char *zSub;
		PH7_MemObjInitFromString(pVm,&sKey,0);
		PH7_MemObjStringAppend(&sKey,"\0DirectoryIterator\0glob",
			sizeof("\0DirectoryIterator\0glob")-1);
		PH7_MemObjInitFromBool(pVm,&sVal,0);
		ph7_array_add_elem(pOut,&sKey,&sVal);
		PH7_MemObjRelease(&sKey);
		PH7_MemObjRelease(&sVal);
		zSub = SfiStr(pThis,SDI_S,&nSub);
		SfiDebugStr(pVm,pOut,"\0RecursiveDirectoryIterator\0subPathName",
			(int)sizeof("\0RecursiveDirectoryIterator\0subPathName")-1,zSub,nSub);
	}
	return PH7_OK;
}
static sxi32 SfiPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)
{
	if( !bDebug ){
		return PH7_OK;   /* php's (array) cast and var_export show nothing */
	}
	return SfiFillDebug(pVm,pThis,pOut);
}
static int vm_builtin_SplFileInfo_debugInfo(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
	SfiFillDebug(pVm,PH7_ContextThis(pCtx),&sOut);
	ph7_result_value(pCtx,&sOut);
	PH7_MemObjRelease(&sOut);
	return PH7_OK;
}
/* php's own escape hatch for a subclass that forgot to call parent::__construct.
 * It exists to be THROWN, and php marks it deprecated rather than removing it. */
static int vm_builtin_SplFileInfo_badState(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return PH7_VmThrowException(pCtx,"Error",
		"The parent constructor was not called: the object is in an invalid state");
}
/*
 * The declaration. Method ORDER is spl_directory.stub.php's; openFile() and
 * setFileClass() are absent because SplFileObject is (§7), and everything else is
 * php's, tentative return types included.
 */
static sxi32 VmInstallSplFileInfo(ph7_vm *pVm)
{
	static const PH7_NativePropDef aSfiProp[] = {
		{ SFI_N,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },
		{ SFI_P,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },
		{ SFI_IC, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN,
		  { 0, 0, PH7_NATIVE_VAL_STRING, 0, "SplFileInfo", 0.0 }, 0 },
	};
	static const PH7_NativeMethodDef aSfiMethod[] = {
		{ "__construct",   PH7_MOD_PUBLIC, "string $filename", 0,
		  vm_builtin_SplFileInfo_construct },
		{ "getPath",       PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileInfo_getPath },
		{ "getFilename",   PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileInfo_getFilename },
		{ "getExtension",  PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileInfo_getExtension },
		{ "getBasename",   PH7_MOD_PUBLIC, "string $suffix = \"\"", "@string",
		  vm_builtin_SplFileInfo_getBasename },
		{ "getPathname",   PH7_MOD_PUBLIC, "", "@string", vm_builtin_SplFileInfo_getPathname },
		{ "getPerms",      PH7_MOD_PUBLIC, "", "@int|false", vm_builtin_SplFileInfo_getPerms },
		{ "getInode",      PH7_MOD_PUBLIC, "", "@int|false", vm_builtin_SplFileInfo_getInode },
		{ "getSize",       PH7_MOD_PUBLIC, "", "@int|false", vm_builtin_SplFileInfo_getSize },
		{ "getOwner",      PH7_MOD_PUBLIC, "", "@int|false", vm_builtin_SplFileInfo_getOwner },
		{ "getGroup",      PH7_MOD_PUBLIC, "", "@int|false", vm_builtin_SplFileInfo_getGroup },
		{ "getATime",      PH7_MOD_PUBLIC, "", "@int|false", vm_builtin_SplFileInfo_getATime },
		{ "getMTime",      PH7_MOD_PUBLIC, "", "@int|false", vm_builtin_SplFileInfo_getMTime },
		{ "getCTime",      PH7_MOD_PUBLIC, "", "@int|false", vm_builtin_SplFileInfo_getCTime },
		{ "getType",       PH7_MOD_PUBLIC, "", "@string|false", vm_builtin_SplFileInfo_getType },
		{ "isWritable",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isWritable },
		{ "isReadable",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isReadable },
		{ "isExecutable",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isExecutable },
		{ "isFile",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isFile },
		{ "isDir",         PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isDir },
		{ "isLink",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SplFileInfo_isLink },
		{ "getLinkTarget", PH7_MOD_PUBLIC, "", "@string|false",
		  vm_builtin_SplFileInfo_getLinkTarget },
		{ "getRealPath",   PH7_MOD_PUBLIC, "", "@string|false",
		  vm_builtin_SplFileInfo_getRealPath },
		{ "getFileInfo",   PH7_MOD_PUBLIC, "?string $class = null", "@SplFileInfo",
		  vm_builtin_SplFileInfo_getFileInfo },
		{ "getPathInfo",   PH7_MOD_PUBLIC, "?string $class = null", "@?SplFileInfo",
		  vm_builtin_SplFileInfo_getPathInfo },
		{ "setInfoClass",  PH7_MOD_PUBLIC, "string $class = SplFileInfo::class", "@void",
		  vm_builtin_SplFileInfo_setInfoClass },
		{ "__toString",    PH7_MOD_PUBLIC, "", "string", vm_builtin_SplFileInfo_getPathname },
		{ "__debugInfo",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_SplFileInfo_debugInfo },
		/* php does NOT mark this one tentative -- it is the only method here that
		 * prints `Return [ void ]` rather than `Tentative return [ void ]`. */
		{ "_bad_state_ex", PH7_MOD_PUBLIC|PH7_MOD_FINAL, "", "void",
		  vm_builtin_SplFileInfo_badState },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "SplFileInfo", 0, "Stringable", PH7_CLASS_NOSERIALIZE,
		  aSfiMethod, SX_ARRAYSIZE(aSfiMethod), 0, 0,
		  aSfiProp, SX_ARRAYSIZE(aSfiProp), 0, 0, SfiPresent },
	};
	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
}
/*
 * ---------------------------------------------------------------------------
 * DirectoryIterator, FilesystemIterator and RecursiveDirectoryIterator.
 *
 * php's `spl_filesystem_object` holds an OPEN directory stream and ONE entry at
 * a time (`u.dir.dirp`, `u.dir.entry`, `u.dir.index`); the chunk read the whole
 * directory into an array at construction, and every difference followed from
 * that one choice (rule 52). php's `rewind()` re-opens the directory and SEES A
 * FILE CREATED SINCE, its `key()` is the read index rather than an array offset,
 * its `seek()` walks FORWARD through the object's own valid()/next() — so a
 * subclass overriding either is obeyed — and a `clone` opens the directory again
 * and reads forward to the same index rather than sharing a cursor.
 *
 * The handle cannot live in a property slot, because CLONE copies slots: two
 * objects would share one directory stream and close it twice. It lives in
 * `pVm->hDirHandle` keyed by the instance, with the class's xRelease closing it,
 * and a clone — finding no entry of its own — re-opens on first use, which IS
 * php's clone handler, deferred. The one thing that deferral costs is a clone
 * whose directory is removed before it is first used: php has the stream open
 * already and answers, PHL raises "Object not initialized" (§7).
 *
 * `file_name` is LAZY here as it is in php: the path, a slash and the current
 * entry, invalidated by every read and rebuilt on demand. That is php-visible
 * twice over -- `getPathname()` answers "" past the end while `getSize()` stats
 * the DIRECTORY (the join with an empty entry), and `var_dump` shows one key
 * fewer until something has asked.
 *
 * The chunk had also INVENTED `DirectoryIterator::getFlags()` (php has no such
 * method; only FilesystemIterator does), inherited SplFileInfo's `__toString()`
 * where php aliases getFilename(), and mis-stated two constants:
 * FOLLOW_SYMLINKS is 16384 (it said 512, colliding with nothing but reading as
 * false for every real flags value) and OTHER_MODE_MASK is 28672.
 * ---------------------------------------------------------------------------
 */
/* php's spl_directory.h flag set, verbatim -- the values the class constants
 * publish and the masks its accessors compare with. */
#define SDI_CURRENT_AS_FILEINFO 0x0000
#define SDI_CURRENT_AS_SELF     0x0010
#define SDI_CURRENT_AS_PATHNAME 0x0020
#define SDI_CURRENT_MODE_MASK   0x00F0
#define SDI_KEY_AS_PATHNAME     0x0000
#define SDI_KEY_AS_FILENAME     0x0100
#define SDI_KEY_MODE_MASK       0x0F00
#define SDI_SKIPDOTS            0x1000
#define SDI_UNIXPATHS           0x2000
#define SDI_FOLLOW_SYMLINKS     0x4000
#define SDI_OTHERS_MASK         0x7000
#define SDI_FLAGS_MASK (SDI_KEY_MODE_MASK|SDI_CURRENT_MODE_MASK|SDI_OTHERS_MASK)

/* php's DEFAULT_SLASH, and the UNIX_PATHS flag that overrides it. */
static char SplDirSlash(sxi64 iFlags)
{
#ifdef __WINNT__
	return (iFlags & SDI_UNIXPATHS) ? '/' : '\\';
#else
	SXUNUSED(iFlags);
	return '/';
#endif
}
/* php's spl_filesystem_is_dot. */
static int SplDirIsDot(const char *zName,int nName)
{
	return (nName == 1 && zName[0] == '.')
		|| (nName == 2 && zName[0] == '.' && zName[1] == '.');
}
/* Does this instance carry php's `u.dir` arm? Asked by the five SplFileInfo
 * bodies that branch on the object TYPE, so it has to be the class question and
 * not "does it have a __e slot" — a user class may declare anything. */
static int SplDirIs(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_class *pDir;
	if( pThis == 0 ){
		return 0;
	}
	pDir = PH7_VmExtractClass(pVm,"DirectoryIterator",sizeof("DirectoryIterator")-1,FALSE,0);
	return pDir && PH7_VmInstanceOf(pThis->pClass,pDir);
}
/* php's `!intern->u.dir.entry.d_name[0]`: the walk has nothing to describe. */
static int SplDirAtEnd(ph7_class_instance *pThis)
{
	int nEntry = 0;
	SfiStr(pThis,SDI_E,&nEntry);
	return nEntry < 1;
}
/* The registry entry for this instance, or 0. */
static VmDirHandle * SplDirFind(ph7_vm *pVm,ph7_class_instance *pThis)
{
	SyHashEntry *pEntry;
	if( pThis == 0 || SyHashTotalEntry(&pVm->hDirHandle) < 1 ){
		return 0;
	}
	pEntry = SyHashGet(&pVm->hDirHandle,(const void *)&pThis,sizeof(void *));
	return pEntry ? (VmDirHandle *)pEntry->pUserData : 0;
}
/* Close the handle this instance owns, if any. The class's xRelease, and the
 * first half of a re-open. */
static void SplDirClose(ph7_vm *pVm,ph7_class_instance *pThis)
{
	void *pData = 0;
	if( SyHashDeleteEntry(&pVm->hDirHandle,(const void *)&pThis,sizeof(void *),&pData) == SXRET_OK
	 && pData ){
		VmDirHandle *pH = (VmDirHandle *)pData;
		if( pH->pStream && pH->pStream->xCloseDir ){
			pH->pStream->xCloseDir(pH->pHandle);
		}
		SyMemBackendFree(&pVm->sAllocator,pH);
	}
}
/*
 * php's spl_filesystem_dir_read: invalidate the lazy name, then take ONE entry
 * from the stream; running out leaves the entry empty, which is what valid()
 * reports. The read goes through a scratch call context because the VFS reports
 * a name by writing a RESULT -- borrowing the method's own return slot would
 * append to whatever the body is about to answer (rule 54).
 */
static void SplDirRead(ph7_vm *pVm,ph7_class_instance *pThis,VmDirHandle *pH)
{
	ph7_context sCtx;
	ph7_value sOut;
	int rc = -1;
	PH7_NativeSetAttrStr(pVm,pThis,SFI_N,"",0);
	PH7_MemObjInit(pVm,&sOut);
	VmInitCallContext(&sCtx,pVm,0,&sOut,0);
	if( pH && pH->pStream && pH->pStream->xReadDir ){
		rc = pH->pStream->xReadDir(pH->pHandle,&sCtx);
	}
	if( rc == PH7_OK ){
		int nName = 0;
		const char *zName = ph7_value_to_string(&sOut,&nName);
		PH7_NativeSetAttrStr(pVm,pThis,SDI_E,zName,nName);
	}else{
		PH7_NativeSetAttrStr(pVm,pThis,SDI_E,"",0);
	}
	VmReleaseCallContext(&sCtx);
	PH7_MemObjRelease(&sOut);
}
/* php's read loop: one entry, then more while SKIP_DOTS and this is a dot. */
static void SplDirReadSkip(ph7_vm *pVm,ph7_class_instance *pThis,VmDirHandle *pH)
{
	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);
	for(;;){
		int nEntry = 0;
		const char *zEntry;
		SplDirRead(pVm,pThis,pH);
		if( (iFlags & SDI_SKIPDOTS) == 0 ){
			return;
		}
		zEntry = SfiStr(pThis,SDI_E,&nEntry);
		if( !SplDirIsDot(zEntry,nEntry) ){
			return;
		}
	}
}
/*
 * php's spl_filesystem_dir_open: open the directory, remember it under the path
 * MINUS one trailing slash, and read the first entry. Answers 0 when the open
 * failed, having still written the path (php sets it either way, so a caught
 * constructor failure leaves the same shape behind).
 */
static VmDirHandle * SplDirOpen(ph7_vm *pVm,ph7_class_instance *pThis,
	const char *zPath,int nPath)
{
	const ph7_io_stream *pStream;
	const char *zDevice;
	VmDirHandle *pH;
	char zBuf[4096];
	void *pHandle = 0;
	int nKeep = nPath;
	if( nPath < 1 || nPath >= (int)sizeof(zBuf) ){
		return 0;
	}
	SyMemcpy(zPath,zBuf,(sxu32)nPath);
	zBuf[nPath] = 0;
	zDevice = zBuf;
	pStream = PH7_VmGetStreamDevice(pVm,&zDevice,nPath);
	if( nKeep > 1 && SFI_IS_SLASH(zPath[nKeep-1]) ){
		nKeep--;
	}
	PH7_NativeSetAttrStr(pVm,pThis,SFI_P,zPath,nKeep);
	PH7_NativeSetAttrInt(pVm,pThis,SDI_I,0);
	PH7_NativeSetAttrStr(pVm,pThis,SDI_E,"",0);
	PH7_NativeSetAttrStr(pVm,pThis,SFI_N,"",0);
	if( pStream == 0 || pStream->xOpenDir == 0
	 || pStream->xOpenDir(zDevice,0,&pHandle) != PH7_OK ){
		return 0;
	}
	pH = (VmDirHandle *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmDirHandle));
	if( pH == 0 ){
		if( pStream->xCloseDir ){
			pStream->xCloseDir(pHandle);
		}
		return 0;
	}
	pH->pStream = pStream;
	pH->pHandle = pHandle;
	pH->pThis = pThis;
	/* SyHashInsert BORROWS the key bytes: key off the record's own field, which
	 * lives exactly as long as the entry does (rule 22). */
	if( SyHashInsert(&pVm->hDirHandle,(const void *)&pH->pThis,sizeof(void *),pH) != SXRET_OK ){
		if( pStream->xCloseDir ){
			pStream->xCloseDir(pHandle);
		}
		SyMemBackendFree(&pVm->sAllocator,pH);
		return 0;
	}
	return pH;
}
/*
 * The open handle behind this instance, RE-OPENING it for a fresh clone.
 *
 * php's clone handler opens the directory again and reads forward to the
 * source's index, because a directory stream cannot be duplicated; PHL does the
 * same work on first use instead, which is what keeps the handle out of every
 * php-visible surface — a property slot carrying it would make `$a == clone $a`
 * false, and php says true.
 */
static VmDirHandle * SplDirState(ph7_vm *pVm,ph7_class_instance *pThis)
{
	VmDirHandle *pH = SplDirFind(pVm,pThis);
	sxi64 iIndex;
	int nPath = 0;
	const char *zPath;
	SyBlob sPath;
	if( pH ){
		return pH;
	}
	zPath = SfiStr(pThis,SFI_P,&nPath);
	if( nPath < 1 ){
		return 0;   /* never constructed: php's "Object not initialized" */
	}
	/* The path slot is about to be rewritten by the open, so copy it out first. */
	SyBlobInit(&sPath,&pVm->sAllocator);
	SyBlobAppend(&sPath,zPath,(sxu32)nPath);
	iIndex = PH7_NativeAttrInt(pThis,SDI_I);
	pH = SplDirOpen(pVm,pThis,(const char *)SyBlobData(&sPath),(int)SyBlobLength(&sPath));
	SyBlobRelease(&sPath);
	if( pH == 0 ){
		return 0;
	}
	SplDirReadSkip(pVm,pThis,pH);
	{
		sxi64 iAt = iIndex;
		while( iAt-- > 0 ){
			SplDirReadSkip(pVm,pThis,pH);
		}
	}
	/* The open above reset the index; the clone stands where the source stood. */
	PH7_NativeSetAttrInt(pVm,pThis,SDI_I,iIndex);
	return pH;
}
/*
 * php's spl_filesystem_object_get_file_name for a DIR: the path, a slash and the
 * current entry, cached until the next read drops it. Called through SfiName(),
 * so every SplFileInfo accessor sees the same lazy value php's do.
 */
static const char * SplDirName(ph7_vm *pVm,ph7_class_instance *pThis,int *pnLen)
{
	int nName = 0,nPath = 0,nEntry = 0;
	const char *zName = SfiStr(pThis,SFI_N,&nName);
	const char *zPath,*zEntry;
	SyBlob sName;
	if( nName > 0 ){
		*pnLen = nName;
		return zName;
	}
	zPath = SfiStr(pThis,SFI_P,&nPath);
	if( nPath < 1 ){
		*pnLen = 0;
		return "";
	}
	SyBlobInit(&sName,&pVm->sAllocator);
	SyBlobAppend(&sName,zPath,(sxu32)nPath);
	{
		char cSlash = SplDirSlash(PH7_NativeAttrInt(pThis,SDI_F));
		SyBlobAppend(&sName,(const void *)&cSlash,sizeof(char));
	}
	zEntry = SfiStr(pThis,SDI_E,&nEntry);
	SyBlobAppend(&sName,zEntry,(sxu32)nEntry);
	PH7_NativeSetAttrStr(pVm,pThis,SFI_N,
		(const char *)SyBlobData(&sName),(int)SyBlobLength(&sName));
	SyBlobRelease(&sName);
	return SfiStr(pThis,SFI_N,pnLen);
}
/* php's CHECK_DIRECTORY_ITERATOR_IS_INITIALIZED: every DirectoryIterator method
 * refuses an object whose parent constructor never ran. */
static VmDirHandle * SplDirChecked(ph7_context *pCtx,sxi32 *pRc)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	VmDirHandle *pH = SplDirState(pCtx->pVm,pThis);
	*pRc = PH7_OK;
	if( pH == 0 ){
		*pRc = PH7_VmThrowException(pCtx,"Error","Object not initialized");
	}
	return pH;
}
/*
 * The shared constructor: php's spl_filesystem_object_construct, whose two
 * refusals are a ValueError for an empty path and an UnexpectedValueException
 * carrying the OPEN's own errno text (php promotes the opendir warning, so the
 * message is the warning's, prefixed with the constructor that raised it).
 */
static int SplDirConstruct(ph7_context *pCtx,const char *zClass,int nArg,ph7_value **apArg,
	sxi64 iFlags)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zPath;
	int nPath = 0;
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	zPath = ph7_value_to_string(apArg[0],&nPath);
	if( nPath < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s::__construct(): Argument #1 ($directory) must not be empty",zClass);
	}
	if( SplDirFind(pVm,pThis) ){
		return PH7_VmThrowException(pCtx,"Error","Directory object is already initialized");
	}
	PH7_NativeSetAttrInt(pVm,pThis,SDI_F,iFlags);
	if( SplDirOpen(pVm,pThis,zPath,nPath) == 0 ){
		return PH7_VmThrowException(pCtx,"UnexpectedValueException",
			"%s::__construct(%.*s): Failed to open directory: %s",zClass,nPath,zPath,
			VfsStrerror(errno));
	}
	SplDirReadSkip(pVm,pThis,SplDirFind(pVm,pThis));
	return PH7_OK;
}
/* DirectoryIterator::__construct(string $directory) — php's flags for this one
 * are KEY_AS_PATHNAME|CURRENT_AS_SELF, and it takes no flags argument. */
static int vm_builtin_DirectoryIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SplDirConstruct(pCtx,"DirectoryIterator",nArg,apArg,
		SDI_KEY_AS_PATHNAME|SDI_CURRENT_AS_SELF);
}
/* The flags argument the two subclasses share: php's ZPP overwrites the whole
 * default when one is given, so SKIP_DOTS is NOT implied by passing flags. */
static sxi64 SplDirFlagArg(int nArg,ph7_value **apArg,sxi64 iDefault)
{
	return nArg > 1 ? ph7_value_to_int64(apArg[1]) : iDefault;
}
static int vm_builtin_FilesystemIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SplDirConstruct(pCtx,"FilesystemIterator",nArg,apArg,
		SplDirFlagArg(nArg,apArg,SDI_KEY_AS_PATHNAME|SDI_CURRENT_AS_FILEINFO|SDI_SKIPDOTS));
}
static int vm_builtin_RecursiveDirectoryIterator_construct(ph7_context *pCtx,int nArg,
	ph7_value **apArg)
{
	return SplDirConstruct(pCtx,"RecursiveDirectoryIterator",nArg,apArg,
		SplDirFlagArg(nArg,apArg,SDI_KEY_AS_PATHNAME|SDI_CURRENT_AS_FILEINFO));
}
/* DirectoryIterator::rewind(): php re-opens nothing — it rewinds the STREAM and
 * takes one entry, with no dot skipping at this level. */
static int vm_builtin_DirectoryIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	VmDirHandle *pH = SplDirChecked(pCtx,&rc);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pH == 0 ){
		return rc;
	}
	PH7_NativeSetAttrInt(pCtx->pVm,PH7_ContextThis(pCtx),SDI_I,0);
	if( pH->pStream->xRewindDir ){
		pH->pStream->xRewindDir(pH->pHandle);
	}
	SplDirRead(pCtx->pVm,PH7_ContextThis(pCtx),pH);
	return PH7_OK;
}
/* FilesystemIterator::rewind(): the same, plus the dot skipping, and php does
 * NOT check the handle here (an uninitialized object simply rewinds to nothing). */
static int vm_builtin_FilesystemIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	VmDirHandle *pH = SplDirState(pVm,pThis);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	PH7_NativeSetAttrInt(pVm,pThis,SDI_I,0);
	if( pH && pH->pStream->xRewindDir ){
		pH->pStream->xRewindDir(pH->pHandle);
	}
	SplDirReadSkip(pVm,pThis,pH);
	return PH7_OK;
}
static int vm_builtin_DirectoryIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi32 rc;
	VmDirHandle *pH = SplDirChecked(pCtx,&rc);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pH == 0 ){
		return rc;
	}
	/* php advances the index PAST the end too, which is why key() keeps counting
	 * once valid() is false. */
	PH7_NativeSetAttrInt(pVm,pThis,SDI_I,PH7_NativeAttrInt(pThis,SDI_I) + 1);
	SplDirReadSkip(pVm,pThis,pH);
	return PH7_OK;
}
static int vm_builtin_DirectoryIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	VmDirHandle *pH = SplDirChecked(pCtx,&rc);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pH == 0 ){
		return rc;
	}
	ph7_result_bool(pCtx,!SplDirAtEnd(PH7_ContextThis(pCtx)));
	return PH7_OK;
}
static int vm_builtin_DirectoryIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	VmDirHandle *pH = SplDirChecked(pCtx,&rc);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pH == 0 ){
		return rc;
	}
	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),SDI_I));
	return PH7_OK;
}
static int vm_builtin_DirectoryIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	VmDirHandle *pH = SplDirChecked(pCtx,&rc);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pH == 0 ){
		return rc;
	}
	SplResultBorrowed(pCtx,PH7_ContextThis(pCtx));
	return PH7_OK;
}
static int vm_builtin_DirectoryIterator_isDot(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int nEntry = 0;
	const char *zEntry;
	sxi32 rc;
	VmDirHandle *pH = SplDirChecked(pCtx,&rc);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pH == 0 ){
		return rc;
	}
	zEntry = SfiStr(PH7_ContextThis(pCtx),SDI_E,&nEntry);
	ph7_result_bool(pCtx,SplDirIsDot(zEntry,nEntry));
	return PH7_OK;
}
/*
 * php's seek(): rewind if the target is behind us, then walk forward through
 * the OBJECT's own valid()/next() — a subclass overriding either is obeyed, and
 * running out raises php's OutOfBoundsException with the iterator left standing
 * where the walk stopped.
 */
static int vm_builtin_DirectoryIterator_seek(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_method *pMethod;
	sxi64 iPos;
	sxi32 rc;
	VmDirHandle *pH = SplDirChecked(pCtx,&rc);
	if( pH == 0 ){
		return rc;
	}
	iPos = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;
	if( PH7_NativeAttrInt(pThis,SDI_I) > iPos ){
		pMethod = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);
		if( pMethod ){
			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,0,0,0);
			if( rc != SXRET_OK ){
				return rc;
			}
		}
	}
	while( PH7_NativeAttrInt(pThis,SDI_I) < iPos ){
		ph7_value sRet;
		int bValid;
		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);
		if( pMethod == 0 ){
			break;
		}
		PH7_MemObjInit(pVm,&sRet);
		rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sRet,0,0);
		bValid = rc == SXRET_OK && ph7_value_to_bool(&sRet);
		PH7_MemObjRelease(&sRet);
		if( rc != SXRET_OK ){
			return rc;
		}
		if( !bValid ){
			return PH7_VmThrowException(pCtx,"OutOfBoundsException",
				"Seek position %qd is out of range",iPos);
		}
		pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);
		if( pMethod == 0 ){
			break;
		}
		rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,0,0,0);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	return PH7_OK;
}
/* DirectoryIterator's three name accessors read the ENTRY, not the pathname —
 * which is why `getFilename()` answers `..` where SplFileInfo's would answer the
 * whole path, and why `__toString()` is aliased to this one rather than to
 * getPathname(). */
static int vm_builtin_DirectoryIterator_getFilename(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int nEntry = 0;
	const char *zEntry;
	sxi32 rc;
	VmDirHandle *pH = SplDirChecked(pCtx,&rc);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pH == 0 ){
		return rc;
	}
	zEntry = SfiStr(PH7_ContextThis(pCtx),SDI_E,&nEntry);
	ph7_result_string(pCtx,zEntry,nEntry);
	return PH7_OK;
}
static int vm_builtin_DirectoryIterator_getBasename(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int nEntry = 0,nBase = 0;
	const char *zEntry,*zBase;
	sxi32 rc;
	VmDirHandle *pH = SplDirChecked(pCtx,&rc);
	if( pH == 0 ){
		return rc;
	}
	zEntry = SfiStr(PH7_ContextThis(pCtx),SDI_E,&nEntry);
	zBase = PH7_ExtractBaseName(zEntry,nEntry,&nBase);
	if( nArg > 0 ){
		int nSuffix = 0;
		const char *zSuffix = ph7_value_to_string(apArg[0],&nSuffix);
		if( nSuffix > 0 && nSuffix < nBase
		 && SyMemcmp(&zBase[nBase - nSuffix],zSuffix,(sxu32)nSuffix) == 0 ){
			nBase -= nSuffix;
		}
	}
	ph7_result_string(pCtx,zBase,nBase);
	return PH7_OK;
}
static int vm_builtin_DirectoryIterator_getExtension(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int nEntry = 0,nBase = 0,i;
	const char *zEntry,*zBase;
	sxi32 rc;
	VmDirHandle *pH = SplDirChecked(pCtx,&rc);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pH == 0 ){
		return rc;
	}
	zEntry = SfiStr(PH7_ContextThis(pCtx),SDI_E,&nEntry);
	zBase = PH7_ExtractBaseName(zEntry,nEntry,&nBase);
	for( i = nBase - 1 ; i >= 0 ; --i ){
		if( zBase[i] == '.' ){
			ph7_result_string(pCtx,&zBase[i+1],nBase - i - 1);
			return PH7_OK;
		}
	}
	ph7_result_string(pCtx,"",0);
	return PH7_OK;
}
/*
 * FilesystemIterator::key()/current(): php compares the flag against its MASK
 * (`(flags & MODE_MASK) == mode`) rather than testing a bit, so a stray bit in
 * another field cannot change either answer — which the chunk's `& KEY_AS_FILENAME`
 * and `=== CURRENT_AS_PATHNAME` both got wrong in one direction or the other.
 */
static int vm_builtin_FilesystemIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);
	int nOut = 0;
	const char *zOut;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( (iFlags & SDI_KEY_MODE_MASK) == SDI_KEY_AS_FILENAME ){
		zOut = SfiStr(pThis,SDI_E,&nOut);
		ph7_result_string(pCtx,zOut,nOut);
		return PH7_OK;
	}
	if( SplDirState(pVm,pThis) == 0 ){
		return PH7_VmThrowException(pCtx,"Error","Object not initialized");
	}
	zOut = SfiName(pVm,pThis,&nOut);
	ph7_result_string(pCtx,zOut,nOut);
	return PH7_OK;
}
static int vm_builtin_FilesystemIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi64 iMode = PH7_NativeAttrInt(pThis,SDI_F) & SDI_CURRENT_MODE_MASK;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( iMode == SDI_CURRENT_AS_PATHNAME || iMode == SDI_CURRENT_AS_FILEINFO ){
		if( SplDirState(pVm,pThis) == 0 ){
			return PH7_VmThrowException(pCtx,"Error","Object not initialized");
		}
	}
	if( iMode == SDI_CURRENT_AS_PATHNAME ){
		int nName = 0;
		const char *zName = SfiName(pVm,pThis,&nName);
		ph7_result_string(pCtx,zName,nName);
		return PH7_OK;
	}
	if( iMode == SDI_CURRENT_AS_FILEINFO ){
		ph7_class *pClass = 0;
		int nName = 0,nDir = 0;
		const char *zName,*zDir;
		sxi32 rc;
		if( SplDirAtEnd(pThis) ){
			/* php's create_type again: there is no entry to describe. */
			return PH7_VmThrowException(pCtx,"RuntimeException","Could not open file");
		}
		rc = SfiInfoClass(pCtx,"current",0,&pClass);
		if( rc != PH7_OK ){
			return rc;
		}
		zDir = SfiStr(pThis,SFI_P,&nDir);
		zName = SfiName(pVm,pThis,&nName);
		return SfiMakeInfoEx(pCtx,pClass,zName,nName,zDir,nDir);
	}
	SplResultBorrowed(pCtx,pThis);
	return PH7_OK;
}
/* php's getFlags()/setFlags() answer and accept only the three mode fields;
 * everything else in the word is engine state the class keeps to itself. */
static int vm_builtin_FilesystemIterator_getFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_int64(pCtx,PH7_NativeAttrInt(PH7_ContextThis(pCtx),SDI_F) & SDI_FLAGS_MASK);
	return PH7_OK;
}
static int vm_builtin_FilesystemIterator_setFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);
	sxi64 iNew = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;
	PH7_NativeSetAttrInt(pCtx->pVm,pThis,SDI_F,(iFlags & ~(sxi64)SDI_FLAGS_MASK)
		| (iNew & (sxi64)SDI_FLAGS_MASK));
	return PH7_OK;
}
/*
 * RecursiveDirectoryIterator::hasChildren(bool $allowLinks = false).
 *
 * php lstats the entry and then asks two separate questions of it: a plain
 * directory has children, and a SYMLINK has them only when the walk was told to
 * follow links. Asked of the VFS rather than of a mode word, because the mode is
 * not filled on Windows (the same lesson getType() learned).
 */
static int vm_builtin_RecursiveDirectoryIterator_hasChildren(ph7_context *pCtx,int nArg,
	ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const ph7_vfs *pVfs = pVm->pEngine->pVfs;
	int nEntry = 0;
	const char *zEntry = SfiStr(pThis,SDI_E,&nEntry);
	char zPath[4096];
	int bAllow = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 0;
	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);
	if( nEntry < 1 || SplDirIsDot(zEntry,nEntry) || pVfs == 0
	 || SfiPathBuf(pVm,pThis,zPath,(int)sizeof(zPath)) != SXRET_OK ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( pVfs->xIslink && pVfs->xIslink(zPath) == PH7_OK
	 && !bAllow && (iFlags & SDI_FOLLOW_SYMLINKS) == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_bool(pCtx,pVfs->xIsdir && pVfs->xIsdir(zPath) == PH7_OK);
	return PH7_OK;
}
/*
 * getChildren(): php builds an instance of the RUNTIME class through its
 * constructor with (pathname, flags), then hands it the sub path — which is what
 * makes getSubPathname() name the whole nested route rather than just the entry
 * (the chunk answered `''` and the filename, wrong at every depth below one).
 */
static int vm_builtin_RecursiveDirectoryIterator_getChildren(ph7_context *pCtx,int nArg,
	ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pNew;
	ph7_class_method *pCons;
	ph7_value sPath,sFlags,*apCall[2];
	int nName = 0,nSub = 0,nEntry = 0;
	const char *zName;
	sxi64 iFlags = PH7_NativeAttrInt(pThis,SDI_F);
	sxi32 rc;
	SyBlob sSub;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( SplDirState(pVm,pThis) == 0 ){
		return PH7_VmThrowException(pCtx,"Error","Object not initialized");
	}
	pNew = PH7_NewClassInstance(pVm,pThis->pClass);
	if( pNew == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	pNew->iRef++;
	zName = SfiName(pVm,pThis,&nName);
	PH7_MemObjInitFromString(pVm,&sPath,0);
	PH7_MemObjStringAppend(&sPath,zName,(sxu32)nName);
	PH7_MemObjInitFromInt(pVm,&sFlags,iFlags);
	apCall[0] = &sPath;
	apCall[1] = &sFlags;
	pCons = PH7_ClassExtractMethod(pThis->pClass,"__construct",sizeof("__construct")-1);
	rc = pCons ? PH7_VmCallClassMethod(pVm,pNew,pCons,0,2,apCall) : SXRET_OK;
	PH7_MemObjRelease(&sPath);
	PH7_MemObjRelease(&sFlags);
	if( rc != SXRET_OK ){
		PH7_ClassInstanceUnref(pNew);
		return rc;
	}
	/* php's sub_path: the parent's, this entry appended. */
	SyBlobInit(&sSub,&pVm->sAllocator);
	{
		const char *zSub = SfiStr(pThis,SDI_S,&nSub);
		SyBlobAppend(&sSub,zSub,(sxu32)nSub);
	}
	if( nSub > 0 ){
		char cSlash = SplDirSlash(iFlags);
		SyBlobAppend(&sSub,(const void *)&cSlash,sizeof(char));
	}
	{
		const char *zEntry = SfiStr(pThis,SDI_E,&nEntry);
		SyBlobAppend(&sSub,zEntry,(sxu32)nEntry);
	}
	PH7_NativeSetAttrStr(pVm,pNew,SDI_S,
		(const char *)SyBlobData(&sSub),(int)SyBlobLength(&sSub));
	SyBlobRelease(&sSub);
	{
		int nInfo = 0;
		const char *zInfo = SfiStr(pThis,SFI_IC,&nInfo);
		PH7_NativeSetAttrStr(pVm,pNew,SFI_IC,zInfo,nInfo);
	}
	PH7_NativeResultObject(pCtx,pNew);
	PH7_ClassInstanceUnref(pNew);
	return PH7_OK;
}
static int vm_builtin_RecursiveDirectoryIterator_getSubPath(ph7_context *pCtx,int nArg,
	ph7_value **apArg)
{
	int nSub = 0;
	const char *zSub = SfiStr(PH7_ContextThis(pCtx),SDI_S,&nSub);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_string(pCtx,zSub,nSub);
	return PH7_OK;
}
static int vm_builtin_RecursiveDirectoryIterator_getSubPathname(ph7_context *pCtx,int nArg,
	ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	int nSub = 0,nEntry = 0;
	const char *zSub = SfiStr(pThis,SDI_S,&nSub);
	SyBlob sOut;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( nSub < 1 ){
		const char *zEntry = SfiStr(pThis,SDI_E,&nEntry);
		ph7_result_string(pCtx,zEntry,nEntry);
		return PH7_OK;
	}
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	SyBlobAppend(&sOut,zSub,(sxu32)nSub);
	{
		char cSlash = SplDirSlash(PH7_NativeAttrInt(pThis,SDI_F));
		SyBlobAppend(&sOut,(const void *)&cSlash,sizeof(char));
	}
	{
		const char *zEntry = SfiStr(pThis,SDI_E,&nEntry);
		SyBlobAppend(&sOut,zEntry,(sxu32)nEntry);
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
/*
 * The three declarations. Method ORDER, signatures and tentative return types
 * are spl_directory.stub.php's; the four slots are php's `u.dir` arm and carry
 * PH7_MOD_HIDDEN because php declares no property at all here. Each class
 * restates NOSERIALIZE and the presentation hook: a native subclass inherits
 * neither (rule 29).
 */
static sxi32 VmInstallSplDirIterators(ph7_vm *pVm)
{
	static const PH7_NativePropDef aDirProp[] = {
		{ SDI_E, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },
		{ SDI_I, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
		{ SDI_F, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
		{ SDI_S, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },
	};
	static const PH7_NativeMethodDef aDirMethod[] = {
		{ "__construct",  PH7_MOD_PUBLIC, "string $directory", 0,
		  vm_builtin_DirectoryIterator_construct },
		{ "getFilename",  PH7_MOD_PUBLIC, "", "@string",
		  vm_builtin_DirectoryIterator_getFilename },
		{ "getExtension", PH7_MOD_PUBLIC, "", "@string",
		  vm_builtin_DirectoryIterator_getExtension },
		{ "getBasename",  PH7_MOD_PUBLIC, "string $suffix = \"\"", "@string",
		  vm_builtin_DirectoryIterator_getBasename },
		{ "isDot",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DirectoryIterator_isDot },
		{ "rewind",       PH7_MOD_PUBLIC, "", "@void", vm_builtin_DirectoryIterator_rewind },
		{ "valid",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DirectoryIterator_valid },
		{ "key",          PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_DirectoryIterator_key },
		{ "current",      PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_DirectoryIterator_current },
		{ "next",         PH7_MOD_PUBLIC, "", "@void", vm_builtin_DirectoryIterator_next },
		{ "seek",         PH7_MOD_PUBLIC, "int $offset", "@void",
		  vm_builtin_DirectoryIterator_seek },
		/* php aliases this one to getFilename(), so `echo $it` prints the ENTRY where
		 * SplFileInfo's __toString prints the whole pathname. Not tentative. */
		{ "__toString",   PH7_MOD_PUBLIC, "", "string",
		  vm_builtin_DirectoryIterator_getFilename },
	};
	static const PH7_NativeConstDef aFsConst[] = {
		{ "CURRENT_MODE_MASK",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_CURRENT_MODE_MASK, 0, 0.0 },
		{ "CURRENT_AS_PATHNAME", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_CURRENT_AS_PATHNAME, 0, 0.0 },
		{ "CURRENT_AS_FILEINFO", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_CURRENT_AS_FILEINFO, 0, 0.0 },
		{ "CURRENT_AS_SELF",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_CURRENT_AS_SELF, 0, 0.0 },
		{ "KEY_MODE_MASK",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_KEY_MODE_MASK, 0, 0.0 },
		{ "KEY_AS_PATHNAME",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_KEY_AS_PATHNAME, 0, 0.0 },
		{ "FOLLOW_SYMLINKS",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_FOLLOW_SYMLINKS, 0, 0.0 },
		{ "KEY_AS_FILENAME",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_KEY_AS_FILENAME, 0, 0.0 },
		{ "NEW_CURRENT_AND_KEY", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_KEY_AS_FILENAME|SDI_CURRENT_AS_FILEINFO, 0, 0.0 },
		{ "OTHER_MODE_MASK",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_OTHERS_MASK, 0, 0.0 },
		{ "SKIP_DOTS",           PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_SKIPDOTS, 0, 0.0 },
		{ "UNIX_PATHS",          PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, SDI_UNIXPATHS, 0, 0.0 },
	};
	static const PH7_NativeMethodDef aFsMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC,
		  "string $directory, int $flags = FilesystemIterator::KEY_AS_PATHNAME | "
		  "FilesystemIterator::CURRENT_AS_FILEINFO | FilesystemIterator::SKIP_DOTS", 0,
		  vm_builtin_FilesystemIterator_construct },
		{ "rewind",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_FilesystemIterator_rewind },
		{ "key",         PH7_MOD_PUBLIC, "", "@string", vm_builtin_FilesystemIterator_key },
		{ "current",     PH7_MOD_PUBLIC, "", "@SplFileInfo|FilesystemIterator|string",
		  vm_builtin_FilesystemIterator_current },
		{ "getFlags",    PH7_MOD_PUBLIC, "", "@int", vm_builtin_FilesystemIterator_getFlags },
		{ "setFlags",    PH7_MOD_PUBLIC, "int $flags", "@void",
		  vm_builtin_FilesystemIterator_setFlags },
	};
	static const PH7_NativeMethodDef aRdiMethod[] = {
		{ "__construct",    PH7_MOD_PUBLIC,
		  "string $directory, int $flags = FilesystemIterator::KEY_AS_PATHNAME | "
		  "FilesystemIterator::CURRENT_AS_FILEINFO", 0,
		  vm_builtin_RecursiveDirectoryIterator_construct },
		{ "hasChildren",    PH7_MOD_PUBLIC, "bool $allowLinks = false", "@bool",
		  vm_builtin_RecursiveDirectoryIterator_hasChildren },
		{ "getChildren",    PH7_MOD_PUBLIC, "", "@RecursiveDirectoryIterator",
		  vm_builtin_RecursiveDirectoryIterator_getChildren },
		{ "getSubPath",     PH7_MOD_PUBLIC, "", "@string",
		  vm_builtin_RecursiveDirectoryIterator_getSubPath },
		{ "getSubPathname", PH7_MOD_PUBLIC, "", "@string",
		  vm_builtin_RecursiveDirectoryIterator_getSubPathname },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "DirectoryIterator", "SplFileInfo", "SeekableIterator", PH7_CLASS_NOSERIALIZE,
		  aDirMethod, SX_ARRAYSIZE(aDirMethod), 0, 0,
		  aDirProp, SX_ARRAYSIZE(aDirProp), SplDirClose, 0, SfiPresent },
		{ "FilesystemIterator", "DirectoryIterator", 0, PH7_CLASS_NOSERIALIZE,
		  aFsMethod, SX_ARRAYSIZE(aFsMethod), aFsConst, SX_ARRAYSIZE(aFsConst),
		  0, 0, SplDirClose, 0, SfiPresent },
		{ "RecursiveDirectoryIterator", "FilesystemIterator", "RecursiveIterator",
		  PH7_CLASS_NOSERIALIZE,
		  aRdiMethod, SX_ARRAYSIZE(aRdiMethod), 0, 0,
		  0, 0, SplDirClose, 0, SfiPresent },
	};
	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
}
PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm)
{
	sxi32 rc = VmInstallWeak(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Ordering, now that zSplLib is gone: the remaining PHP in this subsystem is
	 * the tokenizer chunk's, so these only have to satisfy each OTHER. */
	rc = VmInstallSplStore(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
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
	rc = VmInstallSplObjectStorage(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	rc = VmInstallSplFileInfo(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	/* After SplFileInfo: DirectoryIterator extends it, and PH7_ClassInherit copies
	 * the base's methods DOWN (rule 14). */
	return VmInstallSplDirIterators(&(*pVm));
}

#endif /* PH7_DISABLE_BUILTIN_FUNC */

#ifdef PH7_DISABLE_BUILTIN_FUNC
/* Tiny build: no SPL (builtin layer disabled) */
PH7_PRIVATE sxi32 PH7_VmInstallSpl(ph7_vm *pVm){ (void)pVm; return SXRET_OK; }
#endif
