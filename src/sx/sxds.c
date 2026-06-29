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
		if( pSet->pAllocator == 0 ){
			return  SXERR_LOCKED;
		}
		if( pSet->nSize <= 0 ){
			pSet->nSize = 4;
		}
		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);
		if( pNew == 0 ){
			return SXERR_MEM;
		}
		pSet->pBase = pNew;
		pSet->nSize <<= 1;
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
PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)
{
	const char *zBase;
	if( nIdx >= pSet->nUsed ){
		/* Out of range */
		return 0;
	}
	zBase = (const char *)pSet->pBase;
	return (void *)&zBase[nIdx * pSet->eSize];
}
/* Private hash entry */
struct SyHashEntry_Pr
{
	const void *pKey; /* Hash key */
	sxu32 nKeyLen;    /* Key length */
	void *pUserData;  /* User private data */
	/* Private fields */
	sxu32 nHash;
	SyHash *pHash;
	SyHashEntry_Pr *pNext,*pPrev; /* Next and previous entry in the list */
	SyHashEntry_Pr *pNextCollide,*pPrevCollide; /* Collision list */
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
	if( pEntry->pPrevCollide == 0 ){
		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;
	}else{
		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;
	}
	if( pEntry->pNextCollide ){
		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;
	}
	/* Keep the tail pointer valid when the last entry is the one removed. */
	if( pHash->pLast == pEntry ){
		pHash->pLast = pEntry->pPrev;
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
		pEntry->pNextCollide = pEntry->pPrevCollide = 0;
		/* Install in the new bucket */
		iBucket = pEntry->nHash & (nNewSize - 1);
		pEntry->pNextCollide = apNew[iBucket];
		if( apNew[iBucket] != 0 ){
			apNew[iBucket]->pPrevCollide = pEntry;
		}
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
	if( pHash->apBucket[iBucket] != 0 ){
		pHash->apBucket[iBucket]->pPrevCollide = pEntry;
	}
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
