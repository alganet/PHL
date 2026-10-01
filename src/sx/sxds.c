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
/*
 * Initialize a hash table whose expected population is KNOWN.
 *
 * SXHASH_BUCKET_SIZE is 16, which with SXHASH_FILL_FACTOR 3 is a table sized for
 * forty-eight entries. That is the right default for the VM's own tables -- the
 * function table, the class table, the constants -- and it is the wrong one for
 * the table this engine builds most: an OBJECT's property table. The census of
 * the ecosystem gate's phpcs step counts 30,349 of these against 27,835
 * instances holding 128,024 property slots between them, which is **4.6
 * properties per object** in a table built for forty-eight: 4.40 MB of bucket
 * arrays, nearly all of them zeroes.
 *
 * A caller that already knows the count says so. Everything else is identical,
 * including the growth rule, so a table that outgrows its estimate doubles
 * exactly as it always did -- and one that was sized right never rehashes at
 * all, which the 16-bucket default could not promise a class with fifty
 * properties either.
 *
 * nBucket is rounded UP to a power of two (the bucket index is `nHash &
 * (nBucketSize - 1)`, so it must be one) and floored at 2.
 */
PH7_PRIVATE sxi32 SyHashInitSized(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp,sxu32 nBucket)
{
	SyHashEntry_Pr **apNew;
	sxu32 nSize;
#if defined(UNTRUST)
	if( pHash == 0 ){
		return SXERR_EMPTY;
	}
#endif
	nSize = 2;
	while( nSize < nBucket ){
		nSize <<= 1;
	}
	/* Allocate a new table */
	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * nSize);
	if( apNew == 0 ){
		return SXERR_MEM;
	}
	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * nSize);
	pHash->pAllocator = &(*pAllocator);
	pHash->xHash = xHash ? xHash : SyBinHash;
	pHash->xCmp = xCmp ? xCmp : SyMemcmp;
	pHash->pCurrent = pHash->pList = pHash->pLast = 0;
	pHash->nEntry = 0;
	pHash->apBucket = apNew;
	pHash->nBucketSize = nSize;
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)
{
	return SyHashInitSized(&(*pHash),&(*pAllocator),xHash,xCmp,SXHASH_BUCKET_SIZE);
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
#if defined(PHL_HASH_CENSUS)
/*
 * Which frame a site address is taken from -- see build-aux/hashcensus.sh -c.
 *
 * By default a row names the line that called SyHashGet, which is what a change is
 * aimed at. But a row can be a whole SUBSYSTEM's worth of work funnelled through one
 * line: PH7_VmExtractClass has 221 callers and PH7_VmExtractVarCached's misses, the
 * foreach step, OP_STORE and three frame-setup doors all reach the frame table
 * through the same SyHashGet. Building with PHL_HCENSUS_CALLER lifts every row one
 * frame, so it names the CALLER instead, and the two runs together decompose a large
 * row into the doors that actually make it. That is how the 146th session found that
 * 60% of the engine's lookups were six doors and not one (PERF.md §2).
 *
 * It needs -fno-omit-frame-pointer (gcc will not walk up without one) and
 * -Wno-frame-address (gcc warns about a nonzero argument on principle); the script
 * passes both. Never in a shipping build -- like everything else in this file it is
 * compiled out unless PHL_HASH_CENSUS is defined.
 */
#if defined(PHL_HCENSUS_CALLER)
#define PHL_HCENSUS_SITE() __builtin_return_address(1)
#else
#define PHL_HCENSUS_SITE() __builtin_return_address(0)
#endif
/*
 * PHL_HASH_CENSUS -- which CALL SITE spends the engine's name hashing.
 * ---------------------------------------------------------------------------
 * Compiled out entirely unless PHL_HASH_CENSUS is defined; see PERF.md §7, and
 * build-aux/hashcensus.sh, which builds it and resolves what it prints.
 *
 * The heap census (src/sx/sxmem.c) answers "where are the bytes and who asked
 * for them"; this answers the same question for SyHashGet, which on a real
 * workload is the single largest subsystem in a profile and which the sampler
 * can only attribute one frame deep, for the samples that happened to land in
 * it. These counts are exact, and they do not care that this box is loaded
 * (PERF.md §7) -- a lookup either happened or it did not.
 *
 * One record per return address, so a site is a place in the SOURCE and not a
 * table: two lookups against the same hash table from two lines are two rows,
 * which is what a change has to be aimed at. Addresses are emitted relative to
 * the PIE load base (the ADDRESS of __executable_start is that base at run
 * time), so addr2line takes them exactly as printed -- the heap census's
 * convention, for the same reason.
 *
 * The table is fixed-size and static: it must not allocate through the
 * allocator whose tables it is measuring. Nothing is ever deleted from it (a
 * site is a code address and there are a few thousand), so a full table would
 * spin -- it refuses to record instead, and says so.
 */
#include <stdio.h>
#include <stdlib.h>

extern char __executable_start[];   /* its ADDRESS is the PIE load base */

typedef struct phl_hcensus_rec phl_hcensus_rec;
struct phl_hcensus_rec {
	void *pSite;     /* PHL_HCENSUS_SITE() at SyHashGet; 0 = free slot */
	sxu64 nCall;     /* lookups made from here */
	sxu64 nKeyByte;  /* key bytes hashed for them (0 for one the table answered
	                  * without hashing -- an empty table, or an empty key) */
	sxu64 nHit;      /* how many found an entry */
	sxu64 nCI;       /* how many were against a case-insensitive table */
};
#define PHL_HCENSUS_SLOTS 8192
static struct {
	int bReady;      /* 0 = untouched, 1 = live */
	int bFull;       /* the table filled; recording stopped */
	phl_hcensus_rec aRec[PHL_HCENSUS_SLOTS];
	sxu64 nCall,nKeyByte,nHit;
} sHCensus;

static void HCensusDump(void)
{
	const char *zOut = getenv("PHL_HCENSUS_OUT");
	FILE *pOut = zOut ? fopen(zOut,"w") : stderr;
	sxu32 i;
	if( pOut == 0 ){
		pOut = stderr;
	}
	fprintf(pOut,"# lookups %llu bytes %llu hits %llu%s\n",
		(unsigned long long)sHCensus.nCall,(unsigned long long)sHCensus.nKeyByte,
		(unsigned long long)sHCensus.nHit,sHCensus.bFull ? "  TRUNCATED" : "");
	for( i = 0 ; i < PHL_HCENSUS_SLOTS ; ++i ){
		phl_hcensus_rec *pRec = &sHCensus.aRec[i];
		if( pRec->pSite == 0 ){
			continue;
		}
		fprintf(pOut,"SITE 0x%lx %llu %llu %llu %llu\n",
			(unsigned long)((char *)pRec->pSite - __executable_start),
			(unsigned long long)pRec->nCall,(unsigned long long)pRec->nKeyByte,
			(unsigned long long)pRec->nHit,(unsigned long long)pRec->nCI);
	}
	if( pOut != stderr ){
		fclose(pOut);
	}
}
static phl_hcensus_rec * HCensusSlot(void *pSite)
{
	/* Fibonacci scramble: the low bits of a code address are not a key. */
	sxu64 x = (sxu64)(sxuptr)pSite;
	sxu32 i,n;
	x ^= x >> 33; x *= (sxu64)0xff51afd7ed558ccdULL; x ^= x >> 29;
	i = (sxu32)x & (PHL_HCENSUS_SLOTS - 1);
	for( n = 0 ; n < PHL_HCENSUS_SLOTS ; ++n ){
		if( sHCensus.aRec[i].pSite == pSite ){
			return &sHCensus.aRec[i];
		}
		if( sHCensus.aRec[i].pSite == 0 ){
			sHCensus.aRec[i].pSite = pSite;
			return &sHCensus.aRec[i];
		}
		i = (i + 1) & (PHL_HCENSUS_SLOTS - 1);
	}
	sHCensus.bFull = 1;   /* said out loud in the dump rather than counted wrong */
	return 0;
}
static void HCensusNote(void *pSite,SyHash *pHash,sxu32 nKeyLen,int bHit)
{
	phl_hcensus_rec *pRec;
	if( !sHCensus.bReady ){
		sHCensus.bReady = 1;
		atexit(HCensusDump);
	}
	pRec = HCensusSlot(pSite);
	if( pRec == 0 ){
		return;
	}
	pRec->nCall++;
	pRec->nKeyByte += nKeyLen;
	pRec->nHit += bHit ? 1 : 0;
	pRec->nCI += (pHash->xHash == SyStrHash) ? 1 : 0;
	sHCensus.nCall++;
	sHCensus.nKeyByte += nKeyLen;
	sHCensus.nHit += bHit ? 1 : 0;
}
#endif /* PHL_HASH_CENSUS */
static SyHashEntry_Pr * HashGetEntryHashed(SyHash *pHash,const void *pKey,sxu32 nKeyLen,sxu32 nHash)
{
	SyHashEntry_Pr *pEntry;

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
static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)
{
	return HashGetEntryHashed(&(*pHash),pKey,nKeyLen,pHash->xHash(pKey,nKeyLen));
}
PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)
{
	SyHashEntry_Pr *pEntry;
#if defined(PHL_HASH_CENSUS)
	void *pCensusSite = PHL_HCENSUS_SITE();
#endif
#if defined(UNTRUST)
	if( INVALID_HASH(pHash) ){
		return 0;
	}
#endif
	if( pHash->nEntry < 1 || nKeyLen < 1 ){
		/* Don't bother hashing,return immediately */
#if defined(PHL_HASH_CENSUS)
		HCensusNote(pCensusSite,pHash,0,0);
#endif
		return 0;
	}
	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);
#if defined(PHL_HASH_CENSUS)
	HCensusNote(pCensusSite,pHash,nKeyLen,pEntry != 0);
#endif
	if( pEntry == 0 ){
		return 0;
	}
	return (SyHashEntry *)pEntry;
}
/*
 * The KEY hashed once, for a caller that is about to ask several questions with it.
 *
 * A property access asks two -- does the executing scope declare a private of this
 * name, and where is the slot on this object -- and used to hash the same bytes for
 * each. Hashing is what a lookup spends (PERF.md §5), so the caller hashes once here
 * and hands the answer to SyHashGetHashed below.
 *
 * The hash belongs to the TABLE, not to the key: two tables with different xHash
 * answer differently for the same bytes. Only pass a hash taken from this function,
 * and only to a table that shares this one's xHash.
 */
PH7_PRIVATE sxu32 SyHashKey(SyHash *pHash,const void *pKey,sxu32 nKeyLen)
{
	sxu32 nHash;
#if defined(PHL_HASH_CENSUS)
	void *pCensusSite = PHL_HCENSUS_SITE();
#endif
	nHash = pHash->xHash(pKey,nKeyLen);
#if defined(PHL_HASH_CENSUS)
	/* Counted as a question asked at this site, because that is what it is: the
	 * bytes are charged HERE, and the prehashed probes below then show none. A
	 * census that only counted SyHashGet would make the work disappear. */
	HCensusNote(pCensusSite,pHash,nKeyLen,1);
#endif
	return nHash;
}
/*
 * SyHashGet with the key already hashed (SyHashKey). Hashes nothing.
 */
PH7_PRIVATE SyHashEntry * SyHashGetHashed(SyHash *pHash,const void *pKey,sxu32 nKeyLen,sxu32 nHash)
{
	SyHashEntry_Pr *pEntry;
#if defined(PHL_HASH_CENSUS)
	void *pCensusSite = PHL_HCENSUS_SITE();
#endif
#if defined(UNTRUST)
	if( INVALID_HASH(pHash) ){
		return 0;
	}
	/* The one way to get this wrong is to hand over a hash taken for another
	 * table. Answer NOT FOUND rather than whatever sits in the wrong bucket --
	 * the same shape as the INVALID_HASH screen above, and like it this only
	 * exists in an UNTRUST build. */
	if( nKeyLen > 0 && nHash != pHash->xHash(pKey,nKeyLen) ){
		return 0;
	}
#endif
	if( pHash->nEntry < 1 || nKeyLen < 1 ){
#if defined(PHL_HASH_CENSUS)
		HCensusNote(pCensusSite,pHash,0,0);
#endif
		return 0;
	}
	pEntry = HashGetEntryHashed(&(*pHash),pKey,nKeyLen,nHash);
#if defined(PHL_HASH_CENSUS)
	HCensusNote(pCensusSite,pHash,0,pEntry != 0);
#endif
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
PH7_PRIVATE SyHashEntry * SyHashTailEntry(SyHash *pHash)
{
#if defined(UNTRUST)
	if( INVALID_HASH(pHash) ){
		return 0;
	}
#endif
	/* The tail of the head-pushed list, i.e. the FIRST entry inserted. Walk from
	 * here with SyHashEntryPrev for declaration order (see SyHashForEachReverse,
	 * which does the same thing with a callback). */
	return (SyHashEntry *)pHash->pLast;
}
PH7_PRIVATE SyHashEntry * SyHashEntryPrev(SyHashEntry *pEntry)
{
	if( pEntry == 0 ){
		return 0;
	}
	return (SyHashEntry *)((SyHashEntry_Pr *)pEntry)->pPrev;
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
