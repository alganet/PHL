/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * The cycle collector.
 *
 * PHL frees a value when the last reference to it goes, which is exact for
 * everything except a CYCLE: two objects that hold each other, an array holding
 * itself, a closure capturing the object that holds the closure. Nothing ever
 * drops those to zero, so a long-running program grew without bound where php
 * holds flat -- phpcs over one project spent ~4.9 MB per file and reached
 * 3976 MB against php's 50.
 *
 * The algorithm is php's, and Bacon & Rajan's before it: TRIAL DELETION. When a
 * container's refcount drops WITHOUT reaching zero it becomes a possible root and
 * is buffered -- that is the only event that can strand a cycle, which is what
 * lets this work with no root set at all (this engine has no registry of live
 * objects, so a mark-and-sweep was never available). A collection then runs over
 * the buffered subgraph only:
 *
 *   mark    -- subtract every reference that comes from INSIDE the subgraph;
 *   scan    -- whatever still has a count is held from outside, so put its
 *              references back and blacken everything under it;
 *   collect -- what counted zero is a cycle nothing outside holds.
 *
 * WHAT AN EDGE IS. A container's refcount is the number of ph7_value structs
 * pointing at it, and every value a container owns is a slot in the VM's aMemObj
 * set (a hashmap node's nValIdx, an object property's VmClassAttr::nIdx). One
 * owned slot is one edge. A slot can be SHARED, which is what a PHP reference is,
 * and subtracting an edge somebody else also holds would free a live value.
 * VmGcSlotOwned is the guard: it subtracts only what it can prove this container
 * holds alone and treats anything it cannot prove as an outside hold. That costs
 * a cycle it cannot collect; it never costs correctness.
 *
 * WHEN IT RUNS. Only from the VM's fetch-point safe check, between two
 * instructions, where the operand stack is consistent and no C builtin holds a
 * raw ph7_value* across it -- never from inside the refcount drop that buffered
 * the root, which is the middle of an opcode's C body.
 *
 * WHAT PROVES IT. Before anything is freed the collector re-derives the answer:
 * it subtracts the edges INSIDE the dead set and requires every member to fall to
 * exactly the one reference the collector itself is holding. A destructor that
 * resurrected something, or an edge this file cannot see, shows up as a member
 * that does not, and the whole round is abandoned with every count restored. A
 * round that collects nothing is a missed reclaim; a round that collects
 * something live is a crash, so the check is unconditional.
 */
#include "ph7int.h"

/* Buffered roots before the VM is asked to collect. php's own starting point is
 * 10001, and like php's it MOVES: a program with no cycles in it buffers a root on
 * every refcount drop that does not reach zero -- which is most of them -- and
 * would otherwise pay a full mark-and-scan over ten thousand live containers, over
 * and over, to find nothing. A run that reclaims little doubles the threshold; one
 * that reclaims a real share of what it looked at puts it back. */
#define VM_GC_THRESHOLD_MIN 10000
#define VM_GC_THRESHOLD_MAX 1000000

/*
 * Is this slot held by nothing except the container that owns it?
 *
 * The reference table records the holders it can NAME -- frame variables and
 * array nodes -- plus a count of the ones it cannot (a static's storage, a
 * `use (&$x)` capture, a reference-bound property). A slot with NO record has
 * never been shared with anything, which is exactly the ordinary declared
 * property and the ordinary array element nobody aliased.
 *
 * pNode is the node the walk arrived through, or 0 for an object property (which
 * files no row of its own, so any node row at all means somebody else holds it).
 */
static int VmGcSlotOwned(ph7_vm *pVm,sxu32 nIdx,ph7_hashmap_node *pNode)
{
	VmRefObj *pRef;
	if( nIdx == SXU32_HIGH ){
		return 0;
	}
	pRef = VmRefObjExtract(&(*pVm),nIdx);
	if( pRef == 0 ){
		return 1;
	}
	if( pRef->nPin > 0 ){
		return 0; /* counted pin: the slot is BOUND to somebody else (`$o->p =& $x`) */
	}
	if( PH7_VmRefEntryCount(pRef) > 0 ){
		return 0; /* a NAME holds it too */
	}
	if( pNode == 0 ){
		/* A property. Its slot is pinned VM_REF_IDX_KEEP from the moment the object
		 * is built -- that pin IS the property's own hold, which is why it is not
		 * disqualifying here and is for an element. Any node row on top of it is
		 * somebody else pointing at the same slot. */
		return PH7_VmRefNodeCount(pRef,nIdx) == 0;
	}
	if( (pRef->iFlags & VM_REF_IDX_KEEP) != 0 ){
		return 0; /* an element pinned past its frame: a by-ref return, a capture */
	}
	return pRef->pNode0 == pNode && PH7_VmRefNodeCount(pRef,nIdx) == 1;
}
/*
 * May the collector touch this container's refcount at all?
 *
 * $GLOBALS is never collected -- the engine holds it directly and its release
 * path refuses. Neither is a container with a walk in flight over it: a `foreach`
 * step or an array_walk holds a cursor into its table that no refcount names, so
 * freeing it under one leaves the walk on a dead node. Both are simply invisible
 * to the traversal: no decrement, no restore, nothing pushed. They act as outside
 * roots, which is what they are.
 */
static int VmGcCollectable(ph7_vm *pVm,void *pCont,int bMap)
{
	if( bMap ){
		ph7_hashmap *pMap = (ph7_hashmap *)pCont;
		return pMap != pVm->pGlobal && pMap->pActiveSteps == 0;
	}else{
		ph7_class_instance *pThis = (ph7_class_instance *)pCont;
		/* DESTROYED: the object is already mid-release and its table is being torn
		 * down under us. */
		return pThis->pActiveIters == 0
			&& (pThis->iFlags & CLASS_INSTANCE_DESTROYED) == 0;
	}
}
static sxu8 VmGcColor(void *pCont,int bMap)
{
	return bMap ? ((ph7_hashmap *)pCont)->iGcColor
	            : ((ph7_class_instance *)pCont)->iGcColor;
}
static void VmGcSetColor(void *pCont,int bMap,sxu8 iColor)
{
	if( bMap ){
		((ph7_hashmap *)pCont)->iGcColor = iColor;
	}else{
		((ph7_class_instance *)pCont)->iGcColor = iColor;
	}
}
static sxi32 VmGcRefCount(void *pCont,int bMap)
{
	return bMap ? ((ph7_hashmap *)pCont)->iRef : ((ph7_class_instance *)pCont)->iRef;
}
static void VmGcAddRef(void *pCont,int bMap,sxi32 iDelta)
{
	if( bMap ){
		((ph7_hashmap *)pCont)->iRef += iDelta;
	}else{
		((ph7_class_instance *)pCont)->iRef += iDelta;
	}
}
/* The container a value holds, or 0 when it holds none. */
static void * VmGcValueTarget(ph7_value *pVal,int *pbMap)
{
	if( pVal == 0 || pVal->x.pOther == 0 ){
		return 0;
	}
	if( pVal->iFlags & MEMOBJ_HASHMAP ){
		*pbMap = 1;
		return pVal->x.pOther;
	}
	if( pVal->iFlags & MEMOBJ_OBJ ){
		*pbMap = 0;
		return pVal->x.pOther;
	}
	return 0;
}
/*
 * Walk every container this one holds through a slot it owns outright.
 *
 * The instance side walks hAttr through SyHashFirstEntry/SyHashEntryNext rather
 * than the table's embedded loop cursor: that cursor belongs to whoever else may
 * be walking, and a collection has to leave every other walk where it found it.
 */
typedef struct VmGcCtx VmGcCtx;
struct VmGcCtx
{
	ph7_vm *pVm;
	SySet *pWork;  /* the worklist this phase is draining */
	SySet *pDead;  /* where the collect phase files what it found */
};
typedef void (*ProcGcVisit)(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal);

static void VmGcWalkChildren(VmGcCtx *pCtx,void *pCont,int bMap,ProcGcVisit xVisit)
{
	ph7_vm *pVm = pCtx->pVm;
	int bChildMap = 0;
	void *pChild;
	if( bMap ){
		ph7_hashmap *pMap = (ph7_hashmap *)pCont;
		ph7_hashmap_node *pNode = pMap->pFirst;
		sxu32 n = pMap->nEntry;
		while( n > 0 && pNode ){
			ph7_hashmap_node *pNext = pNode->pPrev; /* reverse link -- insertion order */
			if( VmGcSlotOwned(&(*pVm),pNode->nValIdx,pNode) ){
				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);
				pChild = VmGcValueTarget(pVal,&bChildMap);
				if( pChild && VmGcCollectable(&(*pVm),pChild,bChildMap) ){
					xVisit(pCtx,pChild,bChildMap,pVal);
				}
			}
			pNode = pNext;
			n--;
		}
	}else{
		ph7_class_instance *pThis = (ph7_class_instance *)pCont;
		SyHashEntry *pEntry = SyHashFirstEntry(&pThis->hAttr);
		while( pEntry ){
			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;
			SyHashEntry *pNext = SyHashEntryNext(pEntry);
			/* pInst == 0 is a class STATIC or a class constant: one slot shared by
			 * every instance, so it appears in every instance's table and is not
			 * this object's edge at all. Counting it once per instance would
			 * subtract a reference per instance for a value held once. */
			if( pVmAttr && pVmAttr->pInst == pThis && VmGcSlotOwned(&(*pVm),pVmAttr->nIdx,0) ){
				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx);
				pChild = VmGcValueTarget(pVal,&bChildMap);
				if( pChild && VmGcCollectable(&(*pVm),pChild,bChildMap) ){
					xVisit(pCtx,pChild,bChildMap,pVal);
				}
			}
			pEntry = pNext;
		}
	}
}
/* ------------------------------------------------------------------- buffering */
/*
 * A container whose refcount just dropped WITHOUT reaching zero: the only event
 * that can strand a cycle, and so the only one worth remembering.
 */
PH7_PRIVATE void PH7_GcPossibleRoot(ph7_vm *pVm,void *pCont,int bMap)
{
	VmGcRef sRef;
	if( pVm->bGcEnabled == 0 || pVm->bGcRunning || pVm->bInReset ){
		return;
	}
	if( !VmGcCollectable(&(*pVm),pCont,bMap) ){
		return;
	}
	if( bMap ){
		ph7_hashmap *pMap = (ph7_hashmap *)pCont;
		if( pMap->nGcRoot != 0 ){
			return; /* already buffered */
		}
		pMap->iGcColor = PH7_GC_PURPLE;
		pMap->nGcRoot = SySetUsed(&pVm->aGcRoot) + 1;
	}else{
		ph7_class_instance *pThis = (ph7_class_instance *)pCont;
		if( pThis->nGcRoot != 0 ){
			return;
		}
		pThis->iGcColor = PH7_GC_PURPLE;
		pThis->nGcRoot = SySetUsed(&pVm->aGcRoot) + 1;
	}
	sRef.pPtr = pCont;
	sRef.bMap = (sxu8)bMap;
	if( SySetPut(&pVm->aGcRoot,(const void *)&sRef) != SXRET_OK ){
		/* No room to remember it: forget it rather than record it wrong */
		if( bMap ){
			((ph7_hashmap *)pCont)->nGcRoot = 0;
		}else{
			((ph7_class_instance *)pCont)->nGcRoot = 0;
		}
		return;
	}
	if( SySetUsed(&pVm->aGcRoot) >= pVm->nGcThreshold ){
		/* Ask the VM to collect at its next fetch point -- NOT here, which is the
		 * middle of somebody's refcount drop and so the middle of an opcode. */
		pVm->bGcWanted = 1;
	}
}
/*
 * A buffered container is dying. Its row has to stop naming it before the memory
 * goes back to the pool, or the next collection walks freed memory.
 */
PH7_PRIVATE void PH7_GcForget(ph7_vm *pVm,void *pCont,int bMap)
{
	sxu32 nRoot = bMap ? ((ph7_hashmap *)pCont)->nGcRoot
	                   : ((ph7_class_instance *)pCont)->nGcRoot;
	if( nRoot != 0 && nRoot <= SySetUsed(&pVm->aGcRoot) ){
		VmGcRef *aRoot = (VmGcRef *)SySetBasePtr(&pVm->aGcRoot);
		if( aRoot[nRoot-1].pPtr == pCont ){
			aRoot[nRoot-1].pPtr = 0;
		}
	}
	if( bMap ){
		((ph7_hashmap *)pCont)->nGcRoot = 0;
	}else{
		((ph7_class_instance *)pCont)->nGcRoot = 0;
	}
}
/* --------------------------------------------------------------- the traversal */

/* The worklists are VM-owned, so a collection allocates nothing per run. */
static void VmGcPush(SySet *pWork,void *pCont,int bMap)
{
	VmGcRef sRef;
	sRef.pPtr = pCont;
	sRef.bMap = (sxu8)bMap;
	SySetPut(pWork,(const void *)&sRef);
}
static void VmGcMarkVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)
{
	SXUNUSED(pVal);
	VmGcAddRef(pChild,bChildMap,-1);
	if( VmGcColor(pChild,bChildMap) != PH7_GC_GREY ){
		VmGcSetColor(pChild,bChildMap,PH7_GC_GREY);
		VmGcPush(pCtx->pWork,pChild,bChildMap);
	}
}
static void VmGcDrain(VmGcCtx *pCtx,ProcGcVisit xVisit)
{
	for(;;){
		VmGcRef *pTop = (VmGcRef *)SySetPop(pCtx->pWork);
		VmGcRef sCur;
		if( pTop == 0 ){
			break;
		}
		sCur = *pTop;
		VmGcWalkChildren(pCtx,sCur.pPtr,sCur.bMap,xVisit);
	}
}
static void VmGcMarkGrey(VmGcCtx *pCtx,void *pRoot,int bMap)
{
	if( VmGcColor(pRoot,bMap) == PH7_GC_GREY ){
		return;
	}
	VmGcSetColor(pRoot,bMap,PH7_GC_GREY);
	pCtx->pWork = &pCtx->pVm->aGcWork;
	SySetReset(pCtx->pWork);
	VmGcPush(pCtx->pWork,pRoot,bMap);
	VmGcDrain(pCtx,VmGcMarkVisit);
}
static void VmGcBlackVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)
{
	SXUNUSED(pVal);
	VmGcAddRef(pChild,bChildMap,1);
	if( VmGcColor(pChild,bChildMap) != PH7_GC_BLACK ){
		VmGcSetColor(pChild,bChildMap,PH7_GC_BLACK);
		VmGcPush(pCtx->pWork,pChild,bChildMap);
	}
}
/* Runs on its OWN worklist: the scan below is mid-drain of the primary one. */
static void VmGcScanBlack(VmGcCtx *pCtx,void *pRoot,int bMap)
{
	VmGcCtx sSub;
	sSub.pVm = pCtx->pVm;
	sSub.pDead = pCtx->pDead;
	sSub.pWork = &pCtx->pVm->aGcAux;
	SySetReset(sSub.pWork);
	VmGcSetColor(pRoot,bMap,PH7_GC_BLACK);
	VmGcPush(sSub.pWork,pRoot,bMap);
	VmGcDrain(&sSub,VmGcBlackVisit);
}
static void VmGcPushVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)
{
	SXUNUSED(pVal);
	VmGcPush(pCtx->pWork,pChild,bChildMap);
}
static void VmGcScan(VmGcCtx *pCtx,void *pRoot,int bMap)
{
	pCtx->pWork = &pCtx->pVm->aGcWork;
	SySetReset(pCtx->pWork);
	VmGcPush(pCtx->pWork,pRoot,bMap);
	for(;;){
		VmGcRef *pTop = (VmGcRef *)SySetPop(pCtx->pWork);
		VmGcRef sCur;
		if( pTop == 0 ){
			break;
		}
		sCur = *pTop;
		if( VmGcColor(sCur.pPtr,sCur.bMap) != PH7_GC_GREY ){
			continue;
		}
		/* A count left over is an outside hold -- and so is a state the traversal
		 * cannot see through. A container that acquired a walk in flight between
		 * being buffered and being scanned is held by that cursor, which no
		 * refcount names, so it is put back exactly like one that still counts. */
		if( VmGcRefCount(sCur.pPtr,sCur.bMap) > 0
		 || !VmGcCollectable(pCtx->pVm,sCur.pPtr,sCur.bMap) ){
			VmGcScanBlack(pCtx,sCur.pPtr,sCur.bMap);
		}else{
			VmGcSetColor(sCur.pPtr,sCur.bMap,PH7_GC_WHITE);
			VmGcWalkChildren(pCtx,sCur.pPtr,sCur.bMap,VmGcPushVisit);
		}
	}
}
/* Everything still white is garbage: move it to the dead list, once. */
static void VmGcCollectWhite(VmGcCtx *pCtx,void *pRoot,int bMap)
{
	pCtx->pWork = &pCtx->pVm->aGcWork;
	SySetReset(pCtx->pWork);
	VmGcPush(pCtx->pWork,pRoot,bMap);
	for(;;){
		VmGcRef *pTop = (VmGcRef *)SySetPop(pCtx->pWork);
		VmGcRef sCur;
		if( pTop == 0 ){
			break;
		}
		sCur = *pTop;
		if( VmGcColor(sCur.pPtr,sCur.bMap) != PH7_GC_WHITE ){
			continue;
		}
		VmGcSetColor(sCur.pPtr,sCur.bMap,PH7_GC_DEAD);
		SySetPut(pCtx->pDead,(const void *)&sCur);
		VmGcWalkChildren(pCtx,sCur.pPtr,sCur.bMap,VmGcPushVisit);
	}
}
/* --------------------------------------------------------- proof, and the free */

static void VmGcUncountVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)
{
	SXUNUSED(pCtx); SXUNUSED(pVal);
	if( VmGcColor(pChild,bChildMap) == PH7_GC_DEAD ){
		VmGcAddRef(pChild,bChildMap,-1);
	}
}
static void VmGcRecountVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)
{
	SXUNUSED(pCtx); SXUNUSED(pVal);
	if( VmGcColor(pChild,bChildMap) == PH7_GC_DEAD ){
		VmGcAddRef(pChild,bChildMap,1);
	}
}
static void VmGcCutVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)
{
	SXUNUSED(pCtx);
	if( VmGcColor(pChild,bChildMap) == PH7_GC_DEAD ){
		/* The child is pinned for the duration, so this drops the edge and nothing else */
		PH7_MemObjRelease(pVal);
	}
}
/*
 * Put back the edges the MARK phase subtracted and nothing has put back.
 *
 * Mark decremented every child of every grey node. Scan restored the children of
 * the ones that turned out BLACK; the ones that stayed white did not, so this
 * walks the dead set and restores ALL of their children -- the ones that are dead
 * too AND the live ones they point at. Restoring only the dead ones left a live
 * child short by one for each dead parent naming it, and the parent's ordinary
 * release then decremented it a SECOND time: a live value freed on a count that
 * was never real.
 *
 * From here on ordinary refcounting is exact for the dead set: a destructor may
 * add a reference, drop one, or rewire the graph, and the count follows.
 */
static void VmGcRestoreVisit(VmGcCtx *pCtx,void *pChild,int bChildMap,ph7_value *pVal)
{
	SXUNUSED(pCtx); SXUNUSED(pVal);
	VmGcAddRef(pChild,bChildMap,1);
}
static void VmGcRestoreDead(VmGcCtx *pCtx)
{
	VmGcRef *aDead = (VmGcRef *)SySetBasePtr(pCtx->pDead);
	sxu32 n, nDead = SySetUsed(pCtx->pDead);
	for( n = 0 ; n < nDead ; ++n ){
		VmGcWalkChildren(pCtx,aDead[n].pPtr,aDead[n].bMap,VmGcRestoreVisit);
	}
}
/*
 * Re-derive the answer before acting on it: subtract the edges INSIDE the dead
 * set and require every member to fall to exactly the one reference the collector
 * itself is holding. Anything else -- a destructor that took a new reference, an
 * edge this file cannot see -- and the round is abandoned with every count put
 * back. Missing a reclaim is a missed reclaim; freeing something live is a crash,
 * so the check is unconditional.
 *
 * It runs on the graph as it stands NOW, which is what makes it robust to a
 * destructor that rewired something: refcounting stayed exact through the
 * destructor, so subtracting the edges that exist now is the right subtraction.
 */
static int VmGcVerifyDead(VmGcCtx *pCtx)
{
	VmGcRef *aDead = (VmGcRef *)SySetBasePtr(pCtx->pDead);
	sxu32 n, nDead = SySetUsed(pCtx->pDead);
	int bOk = TRUE;
	for( n = 0 ; n < nDead ; ++n ){
		VmGcWalkChildren(pCtx,aDead[n].pPtr,aDead[n].bMap,VmGcUncountVisit);
	}
	for( n = 0 ; n < nDead ; ++n ){
		if( VmGcRefCount(aDead[n].pPtr,aDead[n].bMap) != 1 ){
			bOk = FALSE;
			break;
		}
	}
	for( n = 0 ; n < nDead ; ++n ){
		VmGcWalkChildren(pCtx,aDead[n].pPtr,aDead[n].bMap,VmGcRecountVisit);
	}
	return bOk;
}
/* ---------------------------------------------------------------- the collection */

PH7_PRIVATE sxu32 PH7_GcCollect(ph7_vm *pVm)
{
	VmGcCtx sCtx;
	VmGcRef *aRoot, *aDead;
	sxu32 n, nRoots, nDead, nCollected = 0;
	pVm->bGcWanted = 0;
	if( pVm->bGcRunning || pVm->bInReset ){
		return 0;
	}
	if( SySetUsed(&pVm->aGcRoot) < 1 ){
		return 0;
	}
	pVm->bGcRunning = 1;
	pVm->nGcRuns++;
	sCtx.pVm = pVm;
	sCtx.pWork = &pVm->aGcWork;
	sCtx.pDead = &pVm->aGcDead;
	SySetReset(&pVm->aGcDead);

	/* (1) mark: subtract every reference internal to the buffered subgraph */
	nRoots = SySetUsed(&pVm->aGcRoot);
	for( n = 0 ; n < nRoots ; ++n ){
		aRoot = (VmGcRef *)SySetBasePtr(&pVm->aGcRoot);
		if( aRoot[n].pPtr == 0 || !VmGcCollectable(pVm,aRoot[n].pPtr,aRoot[n].bMap) ){
			continue;
		}
		VmGcMarkGrey(&sCtx,aRoot[n].pPtr,aRoot[n].bMap);
	}
	/* (2) scan: put back what is still reachable from outside */
	for( n = 0 ; n < nRoots ; ++n ){
		aRoot = (VmGcRef *)SySetBasePtr(&pVm->aGcRoot);
		if( aRoot[n].pPtr == 0 || !VmGcCollectable(pVm,aRoot[n].pPtr,aRoot[n].bMap) ){
			continue;
		}
		VmGcScan(&sCtx,aRoot[n].pPtr,aRoot[n].bMap);
	}
	/* (3) gather what stayed white */
	for( n = 0 ; n < nRoots ; ++n ){
		aRoot = (VmGcRef *)SySetBasePtr(&pVm->aGcRoot);
		if( aRoot[n].pPtr == 0 || !VmGcCollectable(pVm,aRoot[n].pPtr,aRoot[n].bMap) ){
			continue;
		}
		VmGcCollectWhite(&sCtx,aRoot[n].pPtr,aRoot[n].bMap);
	}
	/* The buffer is spent either way; every surviving root is black again. */
	aRoot = (VmGcRef *)SySetBasePtr(&pVm->aGcRoot);
	for( n = 0 ; n < nRoots ; ++n ){
		if( aRoot[n].pPtr == 0 ){
			continue;
		}
		if( VmGcColor(aRoot[n].pPtr,aRoot[n].bMap) != PH7_GC_DEAD ){
			VmGcSetColor(aRoot[n].pPtr,aRoot[n].bMap,PH7_GC_BLACK);
		}
		if( aRoot[n].bMap ){
			((ph7_hashmap *)aRoot[n].pPtr)->nGcRoot = 0;
		}else{
			((ph7_class_instance *)aRoot[n].pPtr)->nGcRoot = 0;
		}
	}
	SySetReset(&pVm->aGcRoot);

	nDead = SySetUsed(&pVm->aGcDead);
	if( nDead < 1 ){
		pVm->bGcRunning = 0;
		return 0;
	}
	/* (4) hold everything dead while the destructors run: a destructor reads its
	 * own object's properties, and those are other members of the same dead set.
	 * Then put back what the MARK phase subtracted, so from here on the dead set
	 * carries its TRUE refcount plus that one hold -- which is what lets a
	 * destructor run arbitrary PHP over it and leave the numbers exact. */
	aDead = (VmGcRef *)SySetBasePtr(&pVm->aGcDead);
	for( n = 0 ; n < nDead ; ++n ){
		VmGcAddRef(aDead[n].pPtr,aDead[n].bMap,1);
	}
	VmGcRestoreDead(&sCtx);
	for( n = 0 ; n < nDead ; ++n ){
		aDead = (VmGcRef *)SySetBasePtr(&pVm->aGcDead);
		if( aDead[n].bMap == 0 ){
			ph7_class_instance *pThis = (ph7_class_instance *)aDead[n].pPtr;
			if( (pThis->iFlags & CLASS_INSTANCE_DTOR_CALLED) == 0 ){
				PH7_ClassInstanceCallDestructor(pThis);
			}
		}
	}
	/* (5) prove it, then break the edges inside the dead set */
	if( VmGcVerifyDead(&sCtx) ){
		aDead = (VmGcRef *)SySetBasePtr(&pVm->aGcDead);
		for( n = 0 ; n < nDead ; ++n ){
			VmGcWalkChildren(&sCtx,aDead[n].pPtr,aDead[n].bMap,VmGcCutVisit);
		}
		nCollected = nDead;
	}
	/* (6) give back the hold. With the edges cut every member falls to zero and
	 * dies through its ordinary release path -- native teardown, weak cells, the
	 * slot free list, all of it. When the proof failed nothing was cut, so this
	 * only puts the counts back where they were.
	 *
	 * bGcRunning stays UP across it. Freeing a member runs its holders' teardown,
	 * which runs PHP, which reaches a fetch point -- and a collection entered from
	 * there would reset the very list this loop is walking. */
	aDead = (VmGcRef *)SySetBasePtr(&pVm->aGcDead);
	for( n = 0 ; n < nDead ; ++n ){
		if( VmGcColor(aDead[n].pPtr,aDead[n].bMap) == PH7_GC_DEAD ){
			VmGcSetColor(aDead[n].pPtr,aDead[n].bMap,PH7_GC_BLACK);
		}
	}
	for( n = 0 ; n < nDead ; ++n ){
		aDead = (VmGcRef *)SySetBasePtr(&pVm->aGcDead);
		if( aDead[n].bMap ){
			PH7_HashmapUnref((ph7_hashmap *)aDead[n].pPtr);
		}else{
			PH7_ClassInstanceUnref((ph7_class_instance *)aDead[n].pPtr);
		}
	}
	SySetReset(&pVm->aGcDead);
	pVm->nGcCollected += nCollected;
	/* What this round cost against what it bought. Rewarding a productive run with
	 * the low threshold keeps a cycle-heavy program collecting often; doubling after
	 * a barren one is what stops a cycle-FREE program paying for the search. */
	if( nCollected * 4 < nRoots ){
		if( pVm->nGcThreshold < VM_GC_THRESHOLD_MAX ){
			pVm->nGcThreshold <<= 1;
		}
	}else{
		pVm->nGcThreshold = VM_GC_THRESHOLD_MIN;
	}
	pVm->bGcRunning = 0;
	return nCollected;
}
/* ------------------------------------------------------------------- lifecycle */

PH7_PRIVATE void PH7_GcInit(ph7_vm *pVm)
{
	pVm->nGcThreshold = VM_GC_THRESHOLD_MIN;
	SySetInit(&pVm->aGcRoot,&pVm->sAllocator,sizeof(VmGcRef));
	SySetInit(&pVm->aGcWork,&pVm->sAllocator,sizeof(VmGcRef));
	SySetInit(&pVm->aGcAux,&pVm->sAllocator,sizeof(VmGcRef));
	SySetInit(&pVm->aGcDead,&pVm->sAllocator,sizeof(VmGcRef));
}
/*
 * Forget every buffered root without touching the containers: ph7_vm_reset is
 * about to release the whole object pool, so a row that outlived it would name
 * freed memory on the next run.
 */
PH7_PRIVATE void PH7_GcResetBuffer(ph7_vm *pVm)
{
	VmGcRef *aRoot = (VmGcRef *)SySetBasePtr(&pVm->aGcRoot);
	sxu32 n;
	for( n = 0 ; n < SySetUsed(&pVm->aGcRoot) ; ++n ){
		if( aRoot[n].pPtr == 0 ){
			continue;
		}
		if( aRoot[n].bMap ){
			((ph7_hashmap *)aRoot[n].pPtr)->nGcRoot = 0;
		}else{
			((ph7_class_instance *)aRoot[n].pPtr)->nGcRoot = 0;
		}
	}
	SySetReset(&pVm->aGcRoot);
	SySetReset(&pVm->aGcWork);
	SySetReset(&pVm->aGcAux);
	SySetReset(&pVm->aGcDead);
	pVm->bGcWanted = 0;
	pVm->nGcThreshold = VM_GC_THRESHOLD_MIN;
}
PH7_PRIVATE void PH7_GcRelease(ph7_vm *pVm)
{
	SySetRelease(&pVm->aGcRoot);
	SySetRelease(&pVm->aGcWork);
	SySetRelease(&pVm->aGcAux);
	SySetRelease(&pVm->aGcDead);
}
