/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "sxtypes.h"
#include "sxmacros.h"
#include "sxset.h"
#include "sxmem.h"
#include "sxhashtable.h"
#include "sxhash.h"
#include "sxstr.h"

/* Byte budget for a set's FIRST allocation -- see SySetPut. */
#define SXSET_FIRST_BYTES 256
PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)
{
	pSet->nSize = 0 ;
	pSet->nUsed = 0;
	pSet->nCursor = 0;
	pSet->eSize = ElemSize;
	pSet->pAllocator = pAllocator;
	pSet->pBase =  0;
	pSet->pUserData = 0;
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)
{
	unsigned char *zbase;
	if( pSet->nUsed >= pSet->nSize ){
		void *pNew;
		sxu32 nNew;
		if( pSet->pAllocator == 0 ){
			return  SXERR_LOCKED;
		}
		/* The FIRST growth is sized in BYTES, not in slots. It used to be eight
		 * slots whatever they cost -- `nSize = 4` followed by the unconditional
		 * `* 2` of the doubling step -- which is a generous opening bid for a
		 * container whose element is large and whose population is usually one or
		 * two. The engine's biggest such set is a function's declared arguments
		 * (192 bytes each): 1,679 of them were live at the peak of a 40-file lint
		 * run, every one holding eight slots, 2.6 MB to describe a few thousand
		 * parameters. A quarter-kilobyte opening keeps all eight for the small
		 * elements that fill them -- an instruction is 32 bytes, a pointer is 8 --
		 * and hands the large ones only what they are likely to use. Everything
		 * after the first doubles, as before. */
		nNew = pSet->nSize;
		if( nNew > 0 ){
			nNew *= 2;
		}else{
			nNew = pSet->eSize > 0 ? SXSET_FIRST_BYTES / pSet->eSize : 8;
			if( nNew < 1 ){ nNew = 1; }
			else if( nNew > 8 ){ nNew = 8; }
		}
		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * nNew);
		if( pNew == 0 ){
			return SXERR_MEM;
		}
		pSet->pBase = pNew;
		pSet->nSize = nNew;
	}
	zbase = (unsigned char *)pSet->pBase;
	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);
	pSet->nUsed++;
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)
{
	if( pSet->nSize > 0 ){
		return SXERR_LOCKED;
	}
	if( nItem < 8 ){
		nItem = 8;
	}
	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);
	if( pSet->pBase == 0 ){
		return SXERR_MEM;
	}
	pSet->nSize = nItem;
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SySetReset(SySet *pSet)
{
	pSet->nUsed   = 0;
	pSet->nCursor = 0;
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)
{
	pSet->nCursor = 0;
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)
{
	register unsigned char *zSrc;
	if( pSet->nCursor >= pSet->nUsed ){
		/* Reset cursor */
		pSet->nCursor = 0;
		return SXERR_EOF;
	}
	zSrc = (unsigned char *)SySetBasePtr(pSet);
	if( ppEntry ){
		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];
	}
	pSet->nCursor++;
	return SXRET_OK;
}
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)
{
	register unsigned char *zSrc;
	if( pSet->nCursor >= pSet->nUsed ){
		return 0;
	}
	zSrc = (unsigned char *)SySetBasePtr(pSet);
	return (void *)&zSrc[pSet->nCursor * pSet->eSize];
}
#endif /* PH7_DISABLE_BUILTIN_FUNC */
/*
 * Give back the slack a doubling set is holding.
 *
 * A set that grew by doubling ends between half full and full, so a container
 * that will never be appended to again is holding up to as much again as it
 * uses. That is the shape of compiled BYTECODE: a function's instructions are
 * emitted once, at compile time, and then only read -- and they were the single
 * largest thing on the heap of an ecosystem-gate lint run after the value table,
 * 11.1 MB across 2,965 containers.
 *
 * Only for a set nothing will append to: it leaves nUsed == nSize, so the very
 * next SySetPut reallocs. A failed shrink is not an error -- the set keeps the
 * larger buffer it already has.
 */
PH7_PRIVATE void SySetShrinkToFit(SySet *pSet)
{
	void *pNew;
	if( pSet->pAllocator == 0 || pSet->nSize <= pSet->nUsed ){
		return;
	}
	if( pSet->nUsed < 1 ){
		/* Nothing to keep. Spelled out rather than routed through SySetRelease,
		 * which leaves nSize standing over a NULL base -- harmless for a set
		 * nobody touches again, a NULL write for one that is put to. */
		SyMemBackendFree(pSet->pAllocator,pSet->pBase);
		pSet->pBase = 0;
		pSet->nSize = 0;
		return;
	}
	pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nUsed);
	if( pNew == 0 ){
		return;   /* keep what we have */
	}
	pSet->pBase = pNew;
	pSet->nSize = pSet->nUsed;
}
PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)
{
	if( nNewSize < pSet->nUsed ){
		pSet->nUsed = nNewSize;
	}
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)
{
	sxi32 rc = SXRET_OK;
	if( pSet->pAllocator && pSet->pBase ){
		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);
	}
	pSet->pBase = 0;
	pSet->nUsed = 0;
	pSet->nCursor = 0;
	return rc;
}
PH7_PRIVATE void * SySetPeek(SySet *pSet)
{
	const char *zBase;
	if( pSet->nUsed <= 0 ){
		return 0;
	}
	zBase = (const char *)pSet->pBase;
	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];
}
PH7_PRIVATE void * SySetPop(SySet *pSet)
{
	const char *zBase;
	void *pData;
	if( pSet->nUsed <= 0 ){
		return 0;
	}
	zBase = (const char *)pSet->pBase;
	pSet->nUsed--;
	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];
	return pData;
}
/* Private hash entry.
 *
 * Fifty-six bytes, which is what the memory pool's 64-byte bucket holds once its
 * eight-byte header is in it. It used to be seventy-two -- eight of them padding
 * around a badly ordered public prefix (fixed in SyHashEntry itself) and eight a
 * BACKWARD collision link -- and seventy-two lands in the next bucket up, so every
 * hash entry in the engine occupied a hundred and twenty-eight bytes to hold
 * seventy-two. On the ecosystem gate's phpcs step 54,000 of them are live at the
 * heap's high-water mark, which was 6.9 MB of a 76 MB peak for 3.9 MB of entries.
 *
 * The collision chain is singly linked as a result: an unlink walks its bucket to
 * find the predecessor. Buckets hold SXHASH_FILL_FACTOR entries on average, so that
 * is three pointer compares on a DELETE -- against sixty-four bytes on every entry
 * that ever exists. The linear-traversal list (pNext/pPrev) stays doubly linked:
 * SyHashForEachReverse and get_defined_vars() walk it backward. */
struct SyHashEntry_Pr
{
	/* The public SyHashEntry prefix, in its order -- this struct is cast to it. */
	const void *pKey; /* Hash key */
	void *pUserData;  /* User private data */
	sxu32 nKeyLen;    /* Key length */
	/* Private fields */
	sxu32 nHash;
	SyHash *pHash;
	SyHashEntry_Pr *pNext,*pPrev; /* Next and previous entry in the list */
	SyHashEntry_Pr *pNextCollide; /* Collision chain, forward only (see above) */
};
#define INVALID_HASH(H) ((H)->apBucket == 0)
PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)
{
	SyHashEntry_Pr **apNew;
#if defined(UNTRUST)
	if( pHash == 0 ){
		return SXERR_EMPTY;
	}
#endif
	/* Allocate a new table */
	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);
	if( apNew == 0 ){
		return SXERR_MEM;
	}
	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);
	pHash->pAllocator = &(*pAllocator);
	pHash->xHash = xHash ? xHash : SyBinHash;
	pHash->xCmp = xCmp ? xCmp : SyMemcmp;
	pHash->pCurrent = pHash->pList = pHash->pLast = 0;
	pHash->nEntry = 0;
	pHash->apBucket = apNew;
	pHash->nBucketSize = SXHASH_BUCKET_SIZE;
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)
{
	SyHashEntry_Pr *pEntry,*pNext;
#if defined(UNTRUST)
	if( INVALID_HASH(pHash)  ){
		return SXERR_EMPTY;
	}
#endif
	pEntry = pHash->pList;
	for(;;){
		if( pHash->nEntry == 0 ){
			break;
		}
		pNext = pEntry->pNext;
		SyMemBackendPoolFree(pHash->pAllocator,pEntry);
		pEntry = pNext;
		pHash->nEntry--;
	}
	if( pHash->apBucket ){
		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);
	}
	pHash->apBucket = 0;
	pHash->nBucketSize = 0;
	pHash->pAllocator = 0;
	return SXRET_OK;
}
static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)
{
	SyHashEntry_Pr *pEntry;
	sxu32 nHash;

	nHash = pHash->xHash(pKey,nKeyLen);
	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];
	for(;;){
		if( pEntry == 0 ){
			break;
		}
		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&
			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){
				return pEntry;
		}
		pEntry = pEntry->pNextCollide;
	}
	/* Entry not found */
	return 0;
}
PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)
{
	SyHashEntry_Pr *pEntry;
#if defined(UNTRUST)
	if( INVALID_HASH(pHash) ){
		return 0;
	}
#endif
	if( pHash->nEntry < 1 || nKeyLen < 1 ){
		/* Don't bother hashing,return immediately */
		return 0;
	}
	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);
	if( pEntry == 0 ){
		return 0;
	}
	return (SyHashEntry *)pEntry;
}
static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)
{
	sxi32 rc;
	/* Unlink from the collision chain. It is singly linked (see SyHashEntry_Pr), so
	 * the predecessor is found by walking the bucket -- SXHASH_FILL_FACTOR entries
	 * long on average. An entry that is not in its own bucket is a corrupted table
	 * and the walk simply finds nothing rather than writing through a stale link. */
	{
		SyHashEntry_Pr **ppSlot = &pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)];
		while( *ppSlot ){
			if( *ppSlot == pEntry ){
				*ppSlot = pEntry->pNextCollide;
				break;
			}
			ppSlot = &(*ppSlot)->pNextCollide;
		}
	}
	/* Keep the tail pointer valid when the last entry is the one removed. */
	if( pHash->pLast == pEntry ){
		pHash->pLast = pEntry->pPrev;
	}
	/* ...and the embedded loop cursor, which the caller may be standing on: the
	 * cursor is advanced BEFORE the body of a SyHashGetNextEntry() walk runs, so
	 * a body that deletes the entry it is about to reach left pCurrent pointing
	 * into the pool slot freed below. */
	if( pHash->pCurrent == pEntry ){
		pHash->pCurrent = pEntry->pNext;
	}
	MACRO_LD_REMOVE(pHash->pList,pEntry);
	pHash->nEntry--;
	if( ppUserData ){
		/* Write a pointer to the user data */
		*ppUserData = pEntry->pUserData;
	}
	/* Release the entry */
	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);
	return rc;
}
PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)
{
	SyHashEntry_Pr *pEntry;
	sxi32 rc;
#if defined(UNTRUST)
	if( INVALID_HASH(pHash) ){
		return SXERR_CORRUPT;
	}
#endif
	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);
	if( pEntry == 0 ){
		return SXERR_NOTFOUND;
	}
	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);
	return rc;
}
PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)
{
	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;
	sxi32 rc;
#if defined(UNTRUST)
	if( pPtr == 0 || INVALID_HASH(pPtr->pHash) ){
		return SXERR_CORRUPT;
	}
#endif
	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);
	return rc;
}
PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)
{
#if defined(UNTRUST)
	if( INVALID_HASH(pHash)  ){
		return SXERR_CORRUPT;
	}
#endif
	pHash->pCurrent = pHash->pList;
	return SXRET_OK;
}
PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)
{
	SyHashEntry_Pr *pEntry;
#if defined(UNTRUST)
	if( INVALID_HASH(pHash) ){
		return 0;
	}
#endif
	if( pHash->pCurrent == 0 || pHash->nEntry <= 0 ){
		pHash->pCurrent = pHash->pList;
		return 0;
	}
	pEntry = pHash->pCurrent;
	/* Advance the cursor */
	pHash->pCurrent = pEntry->pNext;
	/* Return the current entry */
	return (SyHashEntry *)pEntry;
}
PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)
{
	SyHashEntry_Pr *pEntry;
	sxi32 rc;
	sxu32 n;
#if defined(UNTRUST)
	if( INVALID_HASH(pHash) || xStep == 0){
		return 0;
	}
#endif
	pEntry = pHash->pList;
	for( n = 0 ; n < pHash->nEntry ; n++ ){
		/* Invoke the callback */
		rc = xStep((SyHashEntry *)pEntry,pUserData);
		if( rc != SXRET_OK ){
			return rc;
		}
		/* Point to the next entry */
		pEntry = pEntry->pNext;
	}
	return SXRET_OK;
}
/*
 * Like SyHashForEach but walks the entries from the tail (pLast) back to the
 * head via pPrev. The frame's local-variable table is built with SyHashInsert
 * (head-push), so its forward pList order is reverse-insertion (LIFO); walking
 * it backward yields DECLARATION order, which is what php's get_defined_vars()
 * reports. Kept as its own primitive so the shared head-push insert path — and
 * the SyHashLastEntry()==pList head contract every RefObj install relies on —
 * stays untouched.
 */
PH7_PRIVATE sxi32 SyHashForEachReverse(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)
{
	SyHashEntry_Pr *pEntry;
	sxi32 rc;
	sxu32 n;
#if defined(UNTRUST)
	if( INVALID_HASH(pHash) || xStep == 0){
		return 0;
	}
#endif
	pEntry = pHash->pLast;
	for( n = 0 ; n < pHash->nEntry ; n++ ){
		/* Invoke the callback */
		rc = xStep((SyHashEntry *)pEntry,pUserData);
		if( rc != SXRET_OK ){
			return rc;
		}
		/* Point to the previous entry */
		pEntry = pEntry->pPrev;
	}
	return SXRET_OK;
}
static sxi32 HashGrowTable(SyHash *pHash)
{
	sxu32 nNewSize = pHash->nBucketSize * 2;
	SyHashEntry_Pr *pEntry;
	SyHashEntry_Pr **apNew;
	sxu32 n,iBucket;

	/* Allocate a new larger table */
	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));
	if( apNew == 0 ){
		/* Not so fatal,simply a performance hit */
		return SXRET_OK;
	}
	/* Zero the new table */
	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));
	/* Rehash all entries */
	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){
		/* Install in the new bucket */
		iBucket = pEntry->nHash & (nNewSize - 1);
		pEntry->pNextCollide = apNew[iBucket];
		apNew[iBucket] = pEntry;
		/* Point to the next entry */
		pEntry = pEntry->pNext;
	}
	/* Release the old table and reflect the change */
	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);
	pHash->apBucket = apNew;
	pHash->nBucketSize = nNewSize;
	return SXRET_OK;
}
static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)
{
	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);
	/* Insert the entry in its corresponding bucket */
	pEntry->pNextCollide = pHash->apBucket[iBucket];
	pHash->apBucket[iBucket] = pEntry;
	/* Link to the entry list. The default is head-insert (LIFO); bTail appends
	 * to the tail (O(1) via pLast) so iteration follows insertion order — for
	 * callers that need a FIFO traversal. */
	if( bTail && pHash->pLast != 0 ){
		pHash->pLast->pNext = pEntry;
		pEntry->pPrev = pHash->pLast;
		pHash->pLast = pEntry;
	}else{
		MACRO_LD_PUSH(pHash->pList,pEntry);
	}
	if( pHash->nEntry == 0 ){
		/* First entry: it is simultaneously the head, the tail and the cursor. */
		pHash->pCurrent = pHash->pList;
		pHash->pLast = pEntry;
	}
	pHash->nEntry++;
	return SXRET_OK;
}
static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)
{
	SyHashEntry_Pr *pEntry;
	sxi32 rc;
#if defined(UNTRUST)
	if( INVALID_HASH(pHash) || pKey == 0 ){
		return SXERR_CORRUPT;
	}
#endif
	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){
		rc = HashGrowTable(&(*pHash));
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	/* Allocate a new hash entry */
	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));
	if( pEntry == 0 ){
		return SXERR_MEM;
	}
	/* Zero the entry */
	SyZero(pEntry,sizeof(SyHashEntry_Pr));
	pEntry->pHash = pHash;
	pEntry->pKey = pKey;
	pEntry->nKeyLen = nKeyLen;
	pEntry->pUserData = pUserData;
	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);
	/* Finally insert the entry in its corresponding bucket */
	rc = HashInsert(&(*pHash),pEntry,bTail);
	return rc;
}
PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)
{
	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);
}
/*
 * Like SyHashInsert but appends the entry to the tail of the iteration list, so
 * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the
 * default reverse-insertion (LIFO). Used for ordered collections such as dynamic
 * object properties, where PHP preserves property-creation order.
 */
PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)
{
	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);
}
/*
 * The iteration list, walkable WITHOUT the hash's single embedded cursor: the
 * head entry and, from any entry, its successor. A consumer that must survive
 * re-entrancy (nested walks of one table) or a delete under its own feet keeps
 * its own SyHashEntry* here instead of sharing pCurrent — see the instance
 * attribute iterator in oo.c.
 */
PH7_PRIVATE SyHashEntry * SyHashFirstEntry(SyHash *pHash)
{
#if defined(UNTRUST)
	if( INVALID_HASH(pHash) ){
		return 0;
	}
#endif
	return (SyHashEntry *)pHash->pList;
}
PH7_PRIVATE SyHashEntry * SyHashEntryNext(SyHashEntry *pEntry)
{
	if( pEntry == 0 ){
		return 0;
	}
	return (SyHashEntry *)((SyHashEntry_Pr *)pEntry)->pNext;
}
PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)
{
#if defined(UNTRUST)
	if( INVALID_HASH(pHash) ){
		return 0;
	}
#endif
	/* Last inserted entry */
	return (SyHashEntry *)pHash->pList;
}
