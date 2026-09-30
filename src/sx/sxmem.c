/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "sxtypes.h"
#include "sxmacros.h"
#include "sxset.h"
#include "sxmem.h"
#include "sxmutex.h"
#include "sxstr.h"
#if defined(__WINNT__)
#include <Windows.h>
#else
#include <stdlib.h>
#endif
/*
 * ---------------------------------------------------------------------------
 * PHL_MEM_CENSUS -- where the heap actually IS, at the high-water mark.
 * ---------------------------------------------------------------------------
 * Compiled out entirely unless PHL_MEM_CENSUS is defined; see PERF.md §7, which
 * this exists to stop a sixth session from hand-rolling. It records every LIVE
 * object handed out by the public doors of this file, each tagged with the
 * return address that asked for it, and dumps a ranked table the moment the
 * recorded live bytes first cross PHL_CENSUS_AT.
 *
 * Three properties it has to have, each of which cost a session to learn:
 *
 *  - It must not allocate through the allocator it is measuring. The table is
 *    one mmap taken at first use and never grown; if it ever filled, recording
 *    would STOP and the dump would say so rather than lie by a smaller number.
 *  - Every free path must reach it, or a reused address answers for a dead
 *    object. The two free doors are not enough on their own: MemBackendRelease
 *    bulk-frees a whole backend without passing through either, so a record
 *    carries its backend and a release sweeps the table for it.
 *  - The tag has to survive the dump. Site addresses are emitted RELATIVE to
 *    the PIE load base (the ADDRESS of __executable_start is that base at run
 *    time), so addr2line takes them exactly as printed -- there is no slide
 *    left to subtract by hand.
 *
 * Deletion is backward-shift, not tombstones: a run frees millions of objects
 * and a tombstone per free would saturate the table however few are live.
 *
 * The protocol is two runs, and build-aux/census.sh runs both:
 *   1. no PHL_CENSUS_AT      -- prints the peak recorded live bytes at exit
 *   2. PHL_CENSUS_AT=<peak>  -- dumps the table the first time the live bytes
 *                               reach it, then keeps recording, so this run
 *                               ALSO reports its own peak at exit. Comparing
 *                               the two is the only way to catch a workload
 *                               that did different work the second time.
 */
#if defined(PHL_MEM_CENSUS)
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

extern char __executable_start[];   /* its ADDRESS is the PIE load base */

#define PHL_CENSUS_KIND_DIRECT 0    /* SyMemBackendAlloc/Realloc -- a SyMemBlock */
#define PHL_CENSUS_KIND_POOL   1    /* SyMemBackendPoolAlloc     -- a pool chunk */

typedef struct phl_census_rec phl_census_rec;
struct phl_census_rec {
	void *pPtr;              /* what was handed out; 0 = free slot */
	void *pSite;             /* __builtin_return_address(0) at the public door */
	void *pBackend;          /* which SyMemBackend owns it (for the release sweep) */
	sxu32 nByte;             /* bytes the caller ASKED for */
	sxu32 nChunk;            /* bytes the allocator actually spent on it */
	sxu32 nKind;             /* PHL_CENSUS_KIND_* */
	sxu32 nPad;
};
static struct {
	int bReady;              /* 0 = untouched, 1 = live, -1 = off */
	int bFull;               /* the table filled; recording stopped */
	int bDumped;             /* the one dump PHL_CENSUS_AT buys has been taken */
	phl_census_rec *aRec;
	sxu32 nSlot;             /* power of two */
	sxu32 nMask;
	sxu32 nLiveRec;          /* live records == occupied slots */
	sxu64 nLiveByte;         /* live chunk bytes */
	sxu64 nPeakByte;         /* high-water of nLiveByte */
	sxu64 nAt;               /* PHL_CENSUS_AT, 0 = never dump */
	const char *zOut;        /* PHL_CENSUS_OUT */
} sCensus;

static sxu32 CensusHashPtr(const void *p)
{
	/* Fibonacci scramble: the low four to six bits of a chunk address are
	 * always the same, so the raw pointer is not a key. */
	sxu64 x = (sxu64)(sxuptr)p;
	x ^= x >> 33;
	x *= (sxu64)0xff51afd7ed558ccdULL;
	x ^= x >> 29;
	return (sxu32)x;
}
static void CensusDump(void);
static void CensusInit(void)
{
	const char *zSlots,*zAt;
	sxu32 nSlot = 1u << 21;   /* 2M records x 40 B = 80 MB; the phpcs step's peak
	                           * is ~700k live objects, a 33% load */
	size_t nByte;
	void *pMap;

	sCensus.bReady = -1;      /* pessimistic: any early return leaves it off */
	zSlots = getenv("PHL_CENSUS_SLOTS");
	if( zSlots ){
		sxu32 n = (sxu32)strtoul(zSlots,0,0);
		if( n >= 1024 ){
			nSlot = 1;
			while( nSlot < n && nSlot < (1u<<28) ){ nSlot <<= 1; }
		}
	}
	nByte = (size_t)nSlot * sizeof(phl_census_rec);
	pMap = mmap(0,nByte,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
	if( pMap == MAP_FAILED ){
		fprintf(stderr,"census: cannot map %lu bytes -- disabled\n",(unsigned long)nByte);
		return;
	}
	sCensus.aRec  = (phl_census_rec *)pMap;
	sCensus.nSlot = nSlot;
	sCensus.nMask = nSlot - 1;
	zAt = getenv("PHL_CENSUS_AT");
	sCensus.nAt = zAt ? (sxu64)strtoull(zAt,0,0) : 0;
	sCensus.zOut = getenv("PHL_CENSUS_OUT");
	if( sCensus.zOut == 0 ){
		sCensus.zOut = "phl-census.out";
	}
	sCensus.bReady = 1;
}
/* The slot holding pPtr, or -- when bInsert -- the free slot it belongs in. */
static sxu32 CensusSlot(void *pPtr,int bInsert)
{
	sxu32 i = CensusHashPtr(pPtr) & sCensus.nMask;
	for(;;){
		void *p = sCensus.aRec[i].pPtr;
		if( p == pPtr ){
			return i;
		}
		if( p == 0 ){
			return bInsert ? i : SXU32_HIGH;
		}
		i = (i + 1) & sCensus.nMask;
	}
}
/*
 * Backward-shift deletion. Emptying the slot outright would cut every probe
 * chain that runs through it; a tombstone would never be reclaimed. Instead the
 * hole walks forward, pulling back each record that can still be found from
 * where it lands.
 */
static void CensusDeleteAt(sxu32 i)
{
	sxu32 j = i;
	for(;;){
		sxu32 k;
		int bMove;
		j = (j + 1) & sCensus.nMask;
		if( sCensus.aRec[j].pPtr == 0 ){
			break;
		}
		k = CensusHashPtr(sCensus.aRec[j].pPtr) & sCensus.nMask;
		/* Does k lie cyclically in (i,j]? If it does, record j is still
		 * reachable from i's chain and must stay where it is. */
		bMove = (i <= j) ? !(i < k && k <= j) : !(i < k || k <= j);
		if( bMove ){
			sCensus.aRec[i] = sCensus.aRec[j];
			i = j;
		}
	}
	sCensus.aRec[i].pPtr = 0;
}
static void CensusRecord(SyMemBackend *pBackend,void *pPtr,void *pSite,
	sxu32 nByte,sxu32 nChunk,sxu32 nKind)
{
	phl_census_rec *p;
	sxu32 i;
	if( sCensus.bReady == 0 ){
		CensusInit();
	}
	if( sCensus.bReady != 1 || pPtr == 0 ){
		return;
	}
	if( sCensus.nLiveRec * 4 >= sCensus.nSlot * 3 ){
		/* Past a 75% load the probe chains stop being chains. Stop, and say
		 * so twice -- here and in the dump -- so no one reads a short answer
		 * as a small heap. */
		if( !sCensus.bFull ){
			sCensus.bFull = 1;
			fprintf(stderr,"census: table full at %lu records -- "
				"raise PHL_CENSUS_SLOTS\n",(unsigned long)sCensus.nLiveRec);
		}
		return;
	}
	i = CensusSlot(pPtr,1);
	p = &sCensus.aRec[i];
	if( p->pPtr == 0 ){
		sCensus.nLiveRec++;
	}else{
		sCensus.nLiveByte -= p->nChunk;   /* same address re-recorded */
	}
	p->pPtr = pPtr;
	p->pSite = pSite;
	p->pBackend = pBackend;
	p->nByte = nByte;
	p->nChunk = nChunk;
	p->nKind = nKind;
	sCensus.nLiveByte += nChunk;
	if( sCensus.nLiveByte > sCensus.nPeakByte ){
		sCensus.nPeakByte = sCensus.nLiveByte;
	}
	if( sCensus.nAt && !sCensus.bDumped && sCensus.nLiveByte >= sCensus.nAt ){
		CensusDump();
	}
}
static void CensusForget(void *pPtr)
{
	sxu32 i;
	if( sCensus.bReady != 1 || pPtr == 0 ){
		return;
	}
	i = CensusSlot(pPtr,0);
	if( i == SXU32_HIGH ){
		return;
	}
	sCensus.nLiveByte -= sCensus.aRec[i].nChunk;
	sCensus.nLiveRec--;
	CensusDeleteAt(i);
}
/*
 * A backend released every one of its blocks at once, without passing through
 * either free door -- and the pool chunks carved out of those blocks were never
 * blocks of their own to begin with. Sweep its records out, or the next
 * allocation to land on one of those addresses answers for a dead object.
 * Repeated because backward-shift deletion can move a record to a slot the
 * sweep has already walked past.
 */
static void CensusForgetBackend(SyMemBackend *pBackend)
{
	int bMoved = 1;
	if( sCensus.bReady != 1 ){
		return;
	}
	while( bMoved ){
		sxu32 i = 0;
		bMoved = 0;
		while( i < sCensus.nSlot ){
			phl_census_rec *p = &sCensus.aRec[i];
			if( p->pPtr != 0 && p->pBackend == (void *)pBackend ){
				sCensus.nLiveByte -= p->nChunk;
				sCensus.nLiveRec--;
				CensusDeleteAt(i);
				bMoved = 1;
				continue;   /* another record may have shifted INTO i */
			}
			i++;
		}
	}
}
/*
 * The dump aggregates by (site, kind, chunk size, requested-size band) so one
 * table answers both questions the census is for: WHO asked for the bytes, and
 * what SHAPE the requests were. The band column is the only place a fact like
 * "most of these strings are sixteen bytes in a forty-byte hole" is visible.
 */
typedef struct phl_census_agg phl_census_agg;
struct phl_census_agg {
	void *pSite;
	sxu32 nKind;
	sxu32 nChunk;
	sxu32 nBand;
	sxu32 nCount;
	sxu64 nByte;
	sxu64 nChunkTotal;
};
/* Requested size -> band: exact eight-byte steps below 256, powers of two above. */
static sxu32 CensusBand(sxu32 nByte)
{
	sxu32 k;
	if( nByte < 256 ){
		return nByte >> 3;              /* 0 .. 31 */
	}
	k = 8;
	while( k < 31 && nByte >= (1u << (k + 1)) ){
		k++;
	}
	return 24 + k;                      /* 32 .. 55 */
}
static void CensusBandRange(sxu32 nBand,sxu32 *pLo,sxu32 *pHi)
{
	if( nBand < 32 ){
		*pLo = nBand << 3;
		*pHi = (nBand << 3) + 7;
	}else{
		sxu32 k = nBand - 24;
		*pLo = 1u << k;
		*pHi = (k >= 31) ? SXU32_HIGH : ((1u << (k + 1)) - 1);
	}
}
static int CensusAggCmp(const void *a,const void *b)
{
	const phl_census_agg *pA = (const phl_census_agg *)a;
	const phl_census_agg *pB = (const phl_census_agg *)b;
	if( pA->nChunkTotal < pB->nChunkTotal ){ return  1; }
	if( pA->nChunkTotal > pB->nChunkTotal ){ return -1; }
	return 0;
}
static void CensusDump(void)
{
	const sxu32 nAggSlot = 1u << 17;
	phl_census_agg *aAgg;
	sxu32 nAgg = 0;
	sxu32 nLost = 0;
	sxu32 i;
	FILE *pOut;

	/* The dump allocates and writes; it must not be reentered from either.
	 * Recording resumes afterwards so the run still reports its TRUE peak at
	 * exit -- which is the only way to notice that run two of the protocol
	 * measured a different workload than run one (a tool with a warm cache
	 * measures itself, not the engine). */
	sCensus.bReady = -1;
	sCensus.bDumped = 1;
	aAgg = (phl_census_agg *)calloc(nAggSlot,sizeof(phl_census_agg));
	if( aAgg == 0 ){
		fprintf(stderr,"census: no room to aggregate\n");
		sCensus.bReady = 1;
		return;
	}
	for( i = 0 ; i < sCensus.nSlot ; ++i ){
		phl_census_rec *p = &sCensus.aRec[i];
		sxu32 nBand,h,n;
		if( p->pPtr == 0 ){
			continue;
		}
		nBand = CensusBand(p->nByte);
		h = (CensusHashPtr(p->pSite) ^ (p->nChunk * 2654435761u)
			^ (nBand * 40503u) ^ (p->nKind * 97u)) & (nAggSlot - 1);
		for( n = 0 ; n < nAggSlot ; ++n ){
			phl_census_agg *q = &aAgg[h];
			if( q->nCount == 0 ){
				q->pSite = p->pSite; q->nKind = p->nKind;
				q->nChunk = p->nChunk; q->nBand = nBand;
			}
			if( q->pSite == p->pSite && q->nKind == p->nKind
				&& q->nChunk == p->nChunk && q->nBand == nBand ){
				q->nCount++;
				q->nByte += p->nByte;
				q->nChunkTotal += p->nChunk;
				break;
			}
			h = (h + 1) & (nAggSlot - 1);
		}
		if( n == nAggSlot ){
			nLost++;
		}
	}
	for( i = 0 ; i < nAggSlot ; ++i ){
		if( aAgg[i].nCount ){
			aAgg[nAgg++] = aAgg[i];
		}
	}
	qsort(aAgg,nAgg,sizeof(phl_census_agg),CensusAggCmp);
	pOut = fopen(sCensus.zOut,"w");
	if( pOut == 0 ){
		pOut = stderr;
	}
	fprintf(pOut,"# phl heap census\n");
	fprintf(pOut,"# base 0x%lx -- SITE addresses below are ALREADY relative to it\n",
		(unsigned long)(sxuptr)__executable_start);
	fprintf(pOut,"# live-bytes %llu  live-records %lu  peak-bytes %llu  rows %lu%s%s\n",
		(unsigned long long)sCensus.nLiveByte,(unsigned long)sCensus.nLiveRec,
		(unsigned long long)sCensus.nPeakByte,(unsigned long)nAgg,
		sCensus.bFull ? "  TRUNCATED(table-filled)" : "",
		nLost ? "  TRUNCATED(rows-filled)" : "");
	fprintf(pOut,"# SITE <rel-addr> <pool|direct> <count> <chunk-bytes> <req-bytes>"
		" <chunk-size> <req-lo> <req-hi>\n");
	for( i = 0 ; i < nAgg ; ++i ){
		sxu32 lo,hi;
		CensusBandRange(aAgg[i].nBand,&lo,&hi);
		fprintf(pOut,"SITE %lx %s %lu %llu %llu %lu %lu %lu\n",
			(unsigned long)((char *)aAgg[i].pSite - __executable_start),
			aAgg[i].nKind == PHL_CENSUS_KIND_POOL ? "pool" : "direct",
			(unsigned long)aAgg[i].nCount,
			(unsigned long long)aAgg[i].nChunkTotal,
			(unsigned long long)aAgg[i].nByte,
			(unsigned long)aAgg[i].nChunk,
			(unsigned long)lo,(unsigned long)hi);
	}
	if( pOut != stderr ){
		fclose(pOut);
	}
	fprintf(stderr,"census: dumped %lu rows / %llu live bytes to %s\n",
		(unsigned long)nAgg,(unsigned long long)sCensus.nLiveByte,sCensus.zOut);
	free(aAgg);
	sCensus.bReady = 1;
}
/* Run 1 of the protocol: say what the peak was, so run 2 can aim at it. */
static void CensusAtExit(void) __attribute__((destructor));
static void CensusAtExit(void)
{
	if( sCensus.bReady == 0 || sCensus.aRec == 0 ){
		return;   /* never armed, or the table could not be mapped */
	}
	fprintf(stderr,"census: peak recorded live bytes %llu (%.1f MiB)%s\n",
		(unsigned long long)sCensus.nPeakByte,
		(double)sCensus.nPeakByte / (1024.0*1024.0),
		sCensus.bFull ? " -- TRUNCATED, raise PHL_CENSUS_SLOTS" : "");
}
/* A pool chunk's real cost is its BUCKET, which only its header knows. */
static sxu32 CensusPoolChunkSize(void *pChunk,sxu32 nByte)
{
	SyMemHeader *pHeader = (SyMemHeader *)(((char *)pChunk) - sizeof(SyMemHeader));
	sxu32 nBucket = pHeader->nBucket & 0xFFFF;
	if( nBucket == SXU16_HIGH ){
		/* Big block: a SyMemBlock and a SyMemHeader around the request. */
		return nByte + (sxu32)sizeof(SyMemHeader) + (sxu32)sizeof(SyMemBlock);
	}
	return 1u << (nBucket + SXMEM_POOL_INCR);
}
#define PHL_CENSUS_DIRECT(B,P,N) \
	CensusRecord(B,P,__builtin_return_address(0),N, \
		(N) + (sxu32)sizeof(SyMemBlock),PHL_CENSUS_KIND_DIRECT)
#define PHL_CENSUS_POOL(B,P,N) \
	CensusRecord(B,P,__builtin_return_address(0),N, \
		CensusPoolChunkSize(P,N),PHL_CENSUS_KIND_POOL)
#define PHL_CENSUS_FORGET(P)         CensusForget(P)
#define PHL_CENSUS_FORGET_BACKEND(B) CensusForgetBackend(B)
#else
#define PHL_CENSUS_DIRECT(B,P,N)     ((void)0)
#define PHL_CENSUS_POOL(B,P,N)       ((void)0)
#define PHL_CENSUS_FORGET(P)         ((void)0)
#define PHL_CENSUS_FORGET_BACKEND(B) ((void)0)
#endif /* PHL_MEM_CENSUS */

static void * SyOSHeapAlloc(sxu32 nByte)
{
	void *pNew;
#if defined(__WINNT__)
	pNew = HeapAlloc(GetProcessHeap(),0,nByte);
#else
	pNew = malloc((size_t)nByte);
#endif
	return pNew;
}
static void * SyOSHeapRealloc(void *pOld,sxu32 nByte)
{
	void *pNew;
#if defined(__WINNT__)
	pNew = HeapReAlloc(GetProcessHeap(),0,pOld,nByte);
#else
	pNew = realloc(pOld,(size_t)nByte);
#endif
	return pNew;
}
static void SyOSHeapFree(void *pPtr)
{
#if defined(__WINNT__)
	HeapFree(GetProcessHeap(),0,pPtr);
#else
	free(pPtr);
#endif
}


/*
 * Zero a block. Every ph7_value, VM frame, hashmap and reference record is born
 * through here -- 52M calls in a nine-second run of the ecosystem gate's phpcs
 * step -- and it used to be a hand-unrolled byte loop. memset is the same
 * operation a vector register at a time, and unlike the COMPARE (see
 * SX_MACRO_FAST_CMP) it cannot over-read: nSize is memory the caller owns.
 */
PH7_PRIVATE void SyZero(void *pSrc,sxu32 nSize)
{
#if defined(UNTRUST)
	if( pSrc == 0 || nSize <= 0 ){
		return ;
	}
#endif
	if( nSize > 0 ){
		memset(pSrc,0,(size_t)nSize);
	}
}
PH7_PRIVATE sxi32 SyMemcmp(const void *pB1,const void *pB2,sxu32 nSize)
{
	sxi32 rc;
	if( nSize <= 0 ){
		return 0;
	}
	if( pB1 == 0 || pB2 == 0 ){
		return pB1 != 0 ? 1 : (pB2 == 0 ? 0 : -1);
	}
	SX_MACRO_FAST_CMP(pB1,pB2,nSize,rc);
	return rc;
}
PH7_PRIVATE sxu32 SyMemcpy(const void *pSrc,void *pDest,sxu32 nLen)
{
#if defined(UNTRUST)
	if( pSrc == 0 || pDest == 0 ){
		return 0;
	}
#endif
	if( pSrc == (const void *)pDest ){
		return nLen;
	}
	SX_MACRO_FAST_MEMCPY(pSrc,pDest,nLen);
	return nLen;
}
/* Size prefix stored ahead of every OS allocation. Padded to pointer size so
 * the returned payload (and the SyMemBlock/SyMemHeader the backend lays on
 * top of it) keeps the allocator's natural alignment — a bare sxu32 prefix
 * left every chunk 4-misaligned on 64-bit platforms. */
typedef union MemOSHeader MemOSHeader;
union MemOSHeader {
	sxu32 nBytes;
	void *pAlign;
};
static void * MemOSAlloc(sxu32 nBytes)
{
	MemOSHeader *pChunk;
	pChunk = (MemOSHeader *)SyOSHeapAlloc(nBytes + sizeof(MemOSHeader));
	if( pChunk == 0 ){
		return 0;
	}
	pChunk->nBytes = nBytes;
	return (void *)&pChunk[1];
}
static void * MemOSRealloc(void *pOld,sxu32 nBytes)
{
	MemOSHeader *pOldChunk;
	MemOSHeader *pChunk;
	pOldChunk = (MemOSHeader *)(((char *)pOld)-sizeof(MemOSHeader));
	if( pOldChunk->nBytes >= nBytes ){
		return pOld;
	}
	pChunk = (MemOSHeader *)SyOSHeapRealloc(pOldChunk,nBytes + sizeof(MemOSHeader));
	if( pChunk == 0 ){
		return 0;
	}
	pChunk->nBytes = nBytes;
	return (void *)&pChunk[1];
}
static void MemOSFree(void *pBlock)
{
	void *pChunk;
	pChunk = (void *)(((char *)pBlock)-sizeof(MemOSHeader));
	SyOSHeapFree(pChunk);
}
static sxu32 MemOSChunkSize(void *pBlock)
{
	MemOSHeader *pChunk;
	pChunk = (MemOSHeader *)(((char *)pBlock)-sizeof(MemOSHeader));
	return pChunk->nBytes;
}
/* Export OS allocation methods */
static const SyMemMethods sOSAllocMethods = {
	MemOSAlloc,
	MemOSRealloc,
	MemOSFree,
	MemOSChunkSize,
	0,
	0,
	0
};
/*
 * Would this allocation take the backend past its total live-byte ceiling
 * (php's memory_limit)?
 *
 * The first request that would cross it fails AND DISARMS the ceiling, recording
 * its size in nMemTried. Disarming is not a leak of the guarantee: the script is
 * already over and is about to die, and the fatal that says so has to be able to
 * allocate -- a limit that stays armed starves its own error path and the engine
 * dies without ever saying why. php keeps a reserve block for the same reason.
 * nMemTried is what carries the size php names in the message out to the VM.
 */
static int MemBackendOverLimit(SyMemBackend *pBackend,sxu32 nByte,sxu32 nFreed)
{
	sxu32 nLive;
	if( pBackend->nMemLimit == 0 ){
		return 0;
	}
	nLive = (pBackend->nMemUsed >= nFreed) ? (pBackend->nMemUsed - nFreed) : 0;
	if( nLive + nByte <= pBackend->nMemLimit ){
		return 0;
	}
	pBackend->nMemTried = nByte;
	pBackend->nMemLimitHit = pBackend->nMemLimit;
	pBackend->nMemLimit = 0; /* disarm so the fatal can be built and printed */
	return 1;
}
static void * MemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)
{
	SyMemBlock *pBlock;
	sxi32 nRetry = 0;

	/* Append an extra block so we can tracks allocated chunks and avoid memory
	 * leaks.
	 */
	nByte += sizeof(SyMemBlock);
	/* Enforce the optional per-allocation cap (0 = unlimited). A capped failure
	 * returns NULL just like a genuine OS failure, driving the normal SXERR_MEM
	 * propagation; the retry callback is intentionally skipped (hard limit). */
	if( pBackend->nMaxRequest && nByte > pBackend->nMaxRequest ){
		return 0;
	}
	/* ...and the total ceiling (php's memory_limit). */
	if( MemBackendOverLimit(&(*pBackend),nByte,0) ){
		return 0;
	}
	for(;;){
		pBlock = (SyMemBlock *)pBackend->pMethods->xAlloc(nByte);
		if( pBlock != 0 || pBackend->xMemError == 0 || nRetry > SXMEM_BACKEND_RETRY
			|| SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){
				break;
		}
		nRetry++;
	}
	if( pBlock  == 0 ){
		return 0;
	}
	pBlock->pNext = pBlock->pPrev = 0;
	/* Link to the list of already tracked blocks */
	MACRO_LD_PUSH(pBackend->pBlocks,pBlock);
#if defined(UNTRUST)
	pBlock->nGuard = SXMEM_BACKEND_MAGIC;
#endif
	pBlock->nSize = nByte;
	pBackend->nMemUsed += nByte;
	if( pBackend->nMemUsed > pBackend->nMemPeak ){
		pBackend->nMemPeak = pBackend->nMemUsed;
	}
	pBackend->nBlock++;
	return (void *)&pBlock[1];
}
PH7_PRIVATE void * SyMemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)
{
	void *pChunk;
#if defined(UNTRUST)
	if( SXMEM_BACKEND_CORRUPT(pBackend) ){
		return 0;
	}
#endif
	if( pBackend->pMutexMethods ){
		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);
	}
	pChunk = MemBackendAlloc(&(*pBackend),nByte);
	PHL_CENSUS_DIRECT(pBackend,pChunk,nByte);
	if( pBackend->pMutexMethods ){
		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);
	}
	return pChunk;
}
static void * MemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)
{
	SyMemBlock *pBlock,*pNew,*pPrev,*pNext;
	sxu32 nRetry = 0;

	if( pOld == 0 ){
		return MemBackendAlloc(&(*pBackend),nByte);
	}
	pBlock = (SyMemBlock *)(((char *)pOld) - sizeof(SyMemBlock));
#if defined(UNTRUST)
	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){
		return 0;
	}
#endif
	nByte += sizeof(SyMemBlock);
	/* Enforce the optional per-allocation cap (0 = unlimited); see MemBackendAlloc. */
	if( pBackend->nMaxRequest && nByte > pBackend->nMaxRequest ){
		return 0;
	}
	/* ...and the total ceiling, against the size this block is GIVING BACK: a
	 * realloc that shrinks, or grows by less than it already owns, must not be
	 * refused for memory it is not asking for. */
	if( MemBackendOverLimit(&(*pBackend),nByte,pBlock->nSize) ){
		return 0;
	}
	pPrev = pBlock->pPrev;
	pNext = pBlock->pNext;
	{
		/* Old size, captured before realloc may move/free the block; the
		 * live-byte counter is adjusted by the delta only on success below. */
		sxu32 nOld = pBlock->nSize;
	for(;;){
		pNew = (SyMemBlock *)pBackend->pMethods->xRealloc(pBlock,nByte);
		if( pNew != 0 || pBackend->xMemError == 0 || nRetry > SXMEM_BACKEND_RETRY ||
			SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){
				break;
		}
		nRetry++;
	}
	if( pNew == 0 ){
		return 0;
	}
	if( pNew != pBlock ){
		if( pPrev == 0 ){
			pBackend->pBlocks = pNew;
		}else{
			pPrev->pNext = pNew;
		}
		if( pNext ){
			pNext->pPrev = pNew;
		}
#if defined(UNTRUST)
		pNew->nGuard = SXMEM_BACKEND_MAGIC;
#endif
	}
	/* Apply the size delta to the live-byte counter (underflow-guarded). */
	pBackend->nMemUsed = (pBackend->nMemUsed >= nOld) ? (pBackend->nMemUsed - nOld) : 0;
	pBackend->nMemUsed += nByte;
	if( pBackend->nMemUsed > pBackend->nMemPeak ){
		pBackend->nMemPeak = pBackend->nMemUsed;
	}
	pNew->nSize = nByte;
	return (void *)&pNew[1];
	}
}
PH7_PRIVATE void * SyMemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)
{
	void *pChunk;
#if defined(UNTRUST)
	if( SXMEM_BACKEND_CORRUPT(pBackend)  ){
		return 0;
	}
#endif
	if( pBackend->pMutexMethods ){
		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);
	}
	pChunk = MemBackendRealloc(&(*pBackend),pOld,nByte);
	if( pChunk ){
		/* The old address is gone whether or not realloc moved the block. */
		PHL_CENSUS_FORGET(pOld);
		PHL_CENSUS_DIRECT(pBackend,pChunk,nByte);
	}
	if( pBackend->pMutexMethods ){
		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);
	}
	return pChunk;
}
static sxi32 MemBackendFree(SyMemBackend *pBackend,void * pChunk)
{
	SyMemBlock *pBlock;
	pBlock = (SyMemBlock *)(((char *)pChunk) - sizeof(SyMemBlock));
#if defined(UNTRUST)
	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){
		return SXERR_CORRUPT;
	}
#endif
	/* Unlink from the list of active blocks */
	if( pBackend->nBlock > 0 ){
		/* Release the block */
#if defined(UNTRUST)
		/* Mark as stale block */
		pBlock->nGuard = 0x635B;
#endif
		MACRO_LD_REMOVE(pBackend->pBlocks,pBlock);
		pBackend->nBlock--;
		pBackend->nMemUsed = (pBackend->nMemUsed >= pBlock->nSize)
			? (pBackend->nMemUsed - pBlock->nSize) : 0;
		pBackend->pMethods->xFree(pBlock);
	}
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend *pBackend,void * pChunk)
{
	sxi32 rc;
#if defined(UNTRUST)
	if( SXMEM_BACKEND_CORRUPT(pBackend) ){
		return SXERR_CORRUPT;
	}
#endif
	if( pChunk == 0 ){
		return SXRET_OK;
	}
	if( pBackend->pMutexMethods ){
		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);
	}
	PHL_CENSUS_FORGET(pChunk);
	rc = MemBackendFree(&(*pBackend),pChunk);
	if( pBackend->pMutexMethods ){
		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);
	}
	return rc;
}
#if defined(PH7_ENABLE_THREADS)
PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods)
{
	SyMutex *pMutex;
#if defined(UNTRUST)
	if( SXMEM_BACKEND_CORRUPT(pBackend) || pMethods == 0 || pMethods->xNew == 0){
		return SXERR_CORRUPT;
	}
#endif
	pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);
	if( pMutex == 0 ){
		return SXERR_OS;
	}
	/* Attach the mutex to the memory backend */
	pBackend->pMutex = pMutex;
	pBackend->pMutexMethods = pMethods;
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend)
{
#if defined(UNTRUST)
	if( SXMEM_BACKEND_CORRUPT(pBackend) ){
		return SXERR_CORRUPT;
	}
#endif
	if( pBackend->pMutex == 0 ){
		/* There is no mutex subsystem at all */
		return SXRET_OK;
	}
	SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);
	pBackend->pMutexMethods = 0;
	pBackend->pMutex = 0;
	return SXRET_OK;
}
#endif
/*
 * Memory pool allocator
 */
#define SXMEM_POOL_MAGIC		0xDEAD
#define SXMEM_POOL_MAXALLOC		(1<<(SXMEM_POOL_NBUCKETS+SXMEM_POOL_INCR))
#define SXMEM_POOL_MINALLOC		(1<<(SXMEM_POOL_INCR))
/* When SXMEM_POOL_BYPASS is defined (sanitizer builds) the bucket-recycling
 * path is compiled but never taken — MemBackendPoolAlloc forces the big-block
 * branch — so ASan sees one real allocation per request. A compile-time
 * constant (not #ifdef scattered through the alloc body) keeps a single copy
 * of the alloc/tag/free logic; production builds fold the constant to 0 and
 * lose nothing. */
#if defined(SXMEM_POOL_BYPASS)
# define SXMEM_POOL_BYPASS_ACTIVE 1
#else
# define SXMEM_POOL_BYPASS_ACTIVE 0
#endif
static sxi32 MemPoolBucketAlloc(SyMemBackend *pBackend,sxu32 nBucket)
{
	char *zBucket,*zBucketEnd;
	SyMemHeader *pHeader;
	sxu32 nBucketSize;

	/* Allocate one big block first */
	zBucket = (char *)MemBackendAlloc(&(*pBackend),SXMEM_POOL_MAXALLOC);
	if( zBucket == 0 ){
		return SXERR_MEM;
	}
	zBucketEnd = &zBucket[SXMEM_POOL_MAXALLOC];
	/* Divide the big block into mini bucket pool */
	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);
	pBackend->apPool[nBucket] = pHeader = (SyMemHeader *)zBucket;
	for(;;){
		if( &zBucket[nBucketSize] >= zBucketEnd ){
			break;
		}
		pHeader->pNext = (SyMemHeader *)&zBucket[nBucketSize];
		/* Advance the cursor to the next available chunk */
		pHeader = pHeader->pNext;
		zBucket += nBucketSize;
	}
	pHeader->pNext = 0;

	return SXRET_OK;
}
static void * MemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)
{
	SyMemHeader *pBucket,*pNext;
	sxu32 nBucketSize;
	sxu32 nBucket;

	/* SXMEM_POOL_BYPASS (sanitizer builds): force the big-block path for every
	 * request so there is no bucket recycling and ASan tracks each object's
	 * real lifetime. Chunks are freed through MemBackendPoolFree's big-block
	 * branch either way — one copy of the alloc+tag logic. */
	if( SXMEM_POOL_BYPASS_ACTIVE || nByte + sizeof(SyMemHeader) >= SXMEM_POOL_MAXALLOC ){
		/* Allocate a big chunk directly */
		pBucket = (SyMemHeader *)MemBackendAlloc(&(*pBackend),nByte+sizeof(SyMemHeader));
		if( pBucket == 0 ){
			return 0;
		}
		/* Record as big block */
		pBucket->nBucket = ((sxu32)SXMEM_POOL_MAGIC << 16) | SXU16_HIGH;
		return (void *)(pBucket+1);
	}
	/* Locate the appropriate bucket */
	nBucket = 0;
	nBucketSize = SXMEM_POOL_MINALLOC;
	while( nByte + sizeof(SyMemHeader) > nBucketSize  ){
		nBucketSize <<= 1;
		nBucket++;
	}
	pBucket = pBackend->apPool[nBucket];
	if( pBucket == 0 ){
		sxi32 rc;
		rc = MemPoolBucketAlloc(&(*pBackend),nBucket);
		if( rc != SXRET_OK ){
			return 0;
		}
		pBucket = pBackend->apPool[nBucket];
	}
	/* Remove from the free list */
	pNext = pBucket->pNext;
	pBackend->apPool[nBucket] = pNext;
	/* Record bucket&magic number */
	pBucket->nBucket = (((sxu32)SXMEM_POOL_MAGIC << 16) | nBucket);
	return (void *)&pBucket[1];
}
PH7_PRIVATE void * SyMemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)
{
	void *pChunk;
#if defined(UNTRUST)
	if( SXMEM_BACKEND_CORRUPT(pBackend) ){
		return 0;
	}
#endif
	if( pBackend->pMutexMethods ){
		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);
	}
	pChunk = MemBackendPoolAlloc(&(*pBackend),nByte);
	if( pChunk ){
		PHL_CENSUS_POOL(pBackend,pChunk,nByte);
	}
	if( pBackend->pMutexMethods ){
		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);
	}
	return pChunk;
}
static sxi32 MemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)
{
	SyMemHeader *pHeader;
	sxu32 nBucket;
	/* Get the corresponding bucket */
	pHeader = (SyMemHeader *)(((char *)pChunk) - sizeof(SyMemHeader));
	/* Sanity check to avoid misuse */
	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){
		return SXERR_CORRUPT;
	}
	nBucket = pHeader->nBucket & 0xFFFF;
	if( nBucket == SXU16_HIGH ){
		/* Free the big block */
		MemBackendFree(&(*pBackend),pHeader);
	}else if( nBucket >= SXMEM_POOL_NBUCKETS + SXMEM_POOL_INCR ){
		/* Corrupted or misused bucket index */
		return SXERR_CORRUPT;
	}else{
		/* Return to the free list */
		pHeader->pNext = pBackend->apPool[nBucket];
		pBackend->apPool[nBucket] = pHeader;
	}
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)
{
	sxi32 rc;
#if defined(UNTRUST)
	if( SXMEM_BACKEND_CORRUPT(pBackend) || pChunk == 0 ){
		return SXERR_CORRUPT;
	}
#endif
	if( pBackend->pMutexMethods ){
		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);
	}
	PHL_CENSUS_FORGET(pChunk);
	rc = MemBackendPoolFree(&(*pBackend),pChunk);
	if( pBackend->pMutexMethods ){
		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);
	}
	return rc;
}
#if 0
static void * MemBackendPoolRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)
{
	sxu32 nBucket,nBucketSize;
	SyMemHeader *pHeader;
	void * pNew;

	if( pOld == 0 ){
		/* Allocate a new pool */
		pNew = MemBackendPoolAlloc(&(*pBackend),nByte);
		return pNew;
	}
	/* Get the corresponding bucket */
	pHeader = (SyMemHeader *)(((char *)pOld) - sizeof(SyMemHeader));
	/* Sanity check to avoid misuse */
	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){
		return 0;
	}
	nBucket = pHeader->nBucket & 0xFFFF;
	if( nBucket == SXU16_HIGH ){
		/* Big block */
		return MemBackendRealloc(&(*pBackend),pHeader,nByte);
	}
	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);
	if( nBucketSize >= nByte + sizeof(SyMemHeader) ){
		/* The old bucket can honor the requested size */
		return pOld;
	}
	/* Allocate a new pool */
	pNew = MemBackendPoolAlloc(&(*pBackend),nByte);
	if( pNew == 0 ){
		return 0;
	}
	/* Copy the old data into the new block */
	SyMemcpy(pOld,pNew,nBucketSize);
	/* Free the stale block */
	MemBackendPoolFree(&(*pBackend),pOld);
	return pNew;
}
PH7_PRIVATE void * SyMemBackendPoolRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)
{
	void *pChunk;
#if defined(UNTRUST)
	if( SXMEM_BACKEND_CORRUPT(pBackend) ){
		return 0;
	}
#endif
	if( pBackend->pMutexMethods ){
		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);
	}
	pChunk = MemBackendPoolRealloc(&(*pBackend),pOld,nByte);
	if( pBackend->pMutexMethods ){
		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);
	}
	return pChunk;
}
#endif
PH7_PRIVATE sxi32 SyMemBackendInit(SyMemBackend *pBackend,ProcMemError xMemErr,void * pUserData)
{
#if defined(UNTRUST)
	if( pBackend == 0 ){
		return SXERR_EMPTY;
	}
#endif
	/* Zero the allocator first */
	SyZero(&(*pBackend),sizeof(SyMemBackend));
	pBackend->xMemError = xMemErr;
	pBackend->pUserData = pUserData;
	/* Switch to the OS memory allocator */
	pBackend->pMethods = &sOSAllocMethods;
	if( pBackend->pMethods->xInit ){
		/* Initialize the backend  */
		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){
			return SXERR_ABORT;
		}
	}
#if defined(UNTRUST)
	pBackend->nMagic = SXMEM_BACKEND_MAGIC;
#endif
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SyMemBackendInitFromOthers(SyMemBackend *pBackend,const SyMemMethods *pMethods,ProcMemError xMemErr,void * pUserData)
{
#if defined(UNTRUST)
	if( pBackend == 0 || pMethods == 0){
		return SXERR_EMPTY;
	}
#endif
	if( pMethods->xAlloc == 0 || pMethods->xRealloc == 0 || pMethods->xFree == 0 || pMethods->xChunkSize == 0 ){
		/* mandatory methods are missing */
		return SXERR_INVALID;
	}
	/* Zero the allocator first */
	SyZero(&(*pBackend),sizeof(SyMemBackend));
	pBackend->xMemError = xMemErr;
	pBackend->pUserData = pUserData;
	/* Switch to the host application memory allocator */
	pBackend->pMethods = pMethods;
	if( pBackend->pMethods->xInit ){
		/* Initialize the backend  */
		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){
			return SXERR_ABORT;
		}
	}
#if defined(UNTRUST)
	pBackend->nMagic = SXMEM_BACKEND_MAGIC;
#endif
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend *pBackend,SyMemBackend *pParent)
{
	sxu8 bInheritMutex;
#if defined(UNTRUST)
	if( pBackend == 0 || SXMEM_BACKEND_CORRUPT(pParent) ){
		return SXERR_CORRUPT;
	}
#endif
	/* Zero the allocator first */
	SyZero(&(*pBackend),sizeof(SyMemBackend));
	pBackend->pMethods  = pParent->pMethods;
	pBackend->xMemError = pParent->xMemError;
	pBackend->pUserData = pParent->pUserData;
	pBackend->nMaxRequest = pParent->nMaxRequest;
	bInheritMutex = pParent->pMutexMethods ? TRUE : FALSE;
	if( bInheritMutex ){
		pBackend->pMutexMethods = pParent->pMutexMethods;
		/* Create a private mutex */
		pBackend->pMutex = pBackend->pMutexMethods->xNew(SXMUTEX_TYPE_FAST);
		if( pBackend->pMutex ==  0){
			return SXERR_OS;
		}
	}
#if defined(UNTRUST)
	pBackend->nMagic = SXMEM_BACKEND_MAGIC;
#endif
	return SXRET_OK;
}
static sxi32 MemBackendRelease(SyMemBackend *pBackend)
{
	SyMemBlock *pBlock,*pNext;

	pBlock = pBackend->pBlocks;
	for(;;){
		if( pBackend->nBlock == 0 ){
			break;
		}
		pNext  = pBlock->pNext;
		pBackend->pMethods->xFree(pBlock);
		pBlock = pNext;
		pBackend->nBlock--;
		/* LOOP ONE */
		if( pBackend->nBlock == 0 ){
			break;
		}
		pNext  = pBlock->pNext;
		pBackend->pMethods->xFree(pBlock);
		pBlock = pNext;
		pBackend->nBlock--;
		/* LOOP TWO */
		if( pBackend->nBlock == 0 ){
			break;
		}
		pNext  = pBlock->pNext;
		pBackend->pMethods->xFree(pBlock);
		pBlock = pNext;
		pBackend->nBlock--;
		/* LOOP THREE */
		if( pBackend->nBlock == 0 ){
			break;
		}
		pNext  = pBlock->pNext;
		pBackend->pMethods->xFree(pBlock);
		pBlock = pNext;
		pBackend->nBlock--;
		/* LOOP FOUR */
	}
	if( pBackend->pMethods->xRelease ){
		pBackend->pMethods->xRelease(pBackend->pMethods->pUserData);
	}
	pBackend->pMethods = 0;
	pBackend->pBlocks  = 0;
#if defined(UNTRUST)
	pBackend->nMagic = 0x2626;
#endif
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend *pBackend)
{
#if defined(UNTRUST)
	if( SXMEM_BACKEND_CORRUPT(pBackend) ){
		return SXERR_INVALID;
	}
#endif
	if( pBackend->pMutexMethods ){
		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);
	}
	PHL_CENSUS_FORGET_BACKEND(pBackend);
	(void)MemBackendRelease(&(*pBackend));
	if( pBackend->pMutexMethods ){
		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);
		SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);
	}
	return SXRET_OK;
}
PH7_PRIVATE void * SyMemBackendDup(SyMemBackend *pBackend,const void *pSrc,sxu32 nSize)
{
	void *pNew;
#if defined(UNTRUST)
	if( pSrc == 0 || nSize <= 0 ){
		return 0;
	}
#endif
	pNew = SyMemBackendAlloc(&(*pBackend),nSize);
	if( pNew ){
		SyMemcpy(pSrc,pNew,nSize);
	}
	return pNew;
}
PH7_PRIVATE char * SyMemBackendStrDup(SyMemBackend *pBackend,const char *zSrc,sxu32 nSize)
{
	char *zDest;
	zDest = (char *)SyMemBackendAlloc(&(*pBackend),nSize + 1);
	if( zDest == 0 ){
		return 0;
	}
	if( nSize < 1 ){
		/* Systrcpy reads a zero length as "the source is NUL-terminated, measure it",
		 * which is the right convention for ITS callers and the wrong one here: a
		 * caller of this function passed a length, so it has already said the answer.
		 * The difference is not academic -- an EMPTY php string hands out a NULL data
		 * pointer (a SyBlob with no bytes has no buffer), so measuring it is a read of
		 * address zero. `f($o->{''})` reached exactly that, through the deferred
		 * argument path's copy of the property name, and SEGFAULTED where php warns
		 * about an undefined property and answers null. */
		zDest[0] = 0;
		return zDest;
	}
	Systrcpy(zDest,nSize+1,zSrc,nSize);
	return zDest;
}
PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize)
{
#if defined(UNTRUST)
	if( pBlob == 0 || pBuffer == 0 || nSize < 1 ){
		return SXERR_EMPTY;
	}
#endif
	pBlob->pBlob = pBuffer;
	pBlob->mByte = nSize;
	pBlob->nByte = 0;
	pBlob->pAllocator = 0;
	pBlob->nFlags = SXBLOB_LOCKED|SXBLOB_STATIC;
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator)
{
#if defined(UNTRUST)
	if( pBlob == 0  ){
		return SXERR_EMPTY;
	}
#endif
	pBlob->pBlob = 0;
	pBlob->mByte = pBlob->nByte	= 0;
	pBlob->pAllocator = &(*pAllocator);
	pBlob->nFlags = 0;
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte)
{
#if defined(UNTRUST)
	if( pBlob == 0  ){
		return SXERR_EMPTY;
	}
#endif
	pBlob->pBlob = (void *)pData;
	pBlob->nByte = nByte;
	pBlob->mByte = 0;
	pBlob->nFlags |= SXBLOB_RDONLY;
	return SXRET_OK;
}
#ifndef SXBLOB_MIN_GROWTH
#define SXBLOB_MIN_GROWTH 16
#endif
static sxi32 BlobPrepareGrow(SyBlob *pBlob,sxu32 *pByte)
{
	sxu32 nByte;
	void *pNew;
	nByte = *pByte;
	if( pBlob->nFlags & (SXBLOB_LOCKED|SXBLOB_STATIC) ){
		if ( SyBlobFreeSpace(pBlob) < nByte ){
			*pByte = SyBlobFreeSpace(pBlob);
			if( (*pByte) == 0 ){
				return SXERR_SHORT;
			}
		}
		return SXRET_OK;
	}
	if( pBlob->nFlags & SXBLOB_RDONLY ){
		/* Make a copy of the read-only item */
		if( pBlob->nByte > 0 ){
			pNew = SyMemBackendDup(pBlob->pAllocator,pBlob->pBlob,pBlob->nByte);
			if( pNew == 0 ){
				return SXERR_MEM;
			}
			pBlob->pBlob = pNew;
			pBlob->mByte = pBlob->nByte;
		}else{
			pBlob->pBlob = 0;
			pBlob->mByte = 0;
		}
		/* Remove the read-only flag */
		pBlob->nFlags &= ~SXBLOB_RDONLY;
	}
	if( SyBlobFreeSpace(pBlob) >= nByte ){
		return SXRET_OK;
	}
	if( pBlob->mByte > 0 ){
		nByte = nByte + pBlob->mByte * 2 + SXBLOB_MIN_GROWTH;
	}else if ( nByte < SXBLOB_MIN_GROWTH ){
		nByte = SXBLOB_MIN_GROWTH;
	}
	pNew = SyMemBackendRealloc(pBlob->pAllocator,pBlob->pBlob,nByte);
	if( pNew == 0 ){
		return SXERR_MEM;
	}
	pBlob->pBlob = pNew;
	pBlob->mByte = nByte;
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize)
{
	sxu8 *zBlob;
	sxi32 rc;
	if( nSize < 1 ){
		return SXRET_OK;
	}
	rc = BlobPrepareGrow(&(*pBlob),&nSize);
	if( SXRET_OK != rc ){
		return rc;
	}
	if( pData ){
		zBlob = (sxu8 *)pBlob->pBlob ;
		zBlob = &zBlob[pBlob->nByte];
		pBlob->nByte += nSize;
		SX_MACRO_FAST_MEMCPY(pData,zBlob,nSize);
	}
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob)
{
	sxi32 rc;
	sxu32 n;
	n = pBlob->nByte;
	rc = SyBlobAppend(&(*pBlob),(const void *)"\0",sizeof(char));
	if (rc == SXRET_OK ){
		pBlob->nByte = n;
	}
	return rc;
}
PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest)
{
	sxi32 rc = SXRET_OK;
#ifdef UNTRUST
	if( pSrc == 0 || pDest == 0 ){
		return SXERR_EMPTY;
	}
#endif
	if( pSrc->nByte > 0 ){
		rc = SyBlobAppend(&(*pDest),pSrc->pBlob,pSrc->nByte);
	}
	return rc;
}
PH7_PRIVATE sxi32 SyBlobCmp(SyBlob *pLeft,SyBlob *pRight)
{
	sxi32 rc;
#ifdef UNTRUST
	if( pLeft == 0 || pRight == 0 ){
		return pLeft ? 1 : -1;
	}
#endif
	if( pLeft->nByte != pRight->nByte ){
		/* Length differ */
		return pLeft->nByte - pRight->nByte;
	}
	if( pLeft->nByte == 0 ){
		return 0;
	}
	/* Perform a standard memcmp() operation */
	rc = SyMemcmp(pLeft->pBlob,pRight->pBlob,pLeft->nByte);
	return rc;
}
PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob)
{
	pBlob->nByte = 0;
	if( pBlob->nFlags & SXBLOB_RDONLY ){
		pBlob->pBlob = 0;
		pBlob->mByte = 0;
		pBlob->nFlags &= ~SXBLOB_RDONLY;
	}
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob)
{
	if( (pBlob->nFlags & (SXBLOB_STATIC|SXBLOB_RDONLY)) == 0 && pBlob->mByte > 0 ){
		SyMemBackendFree(pBlob->pAllocator,pBlob->pBlob);
	}
	pBlob->pBlob = 0;
	pBlob->nByte = pBlob->mByte = 0;
	pBlob->nFlags = 0;
	return SXRET_OK;
}
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft)
{
	const char *zIn = (const char *)pBlob;
	const char *zEnd;
	sxi32 rc;
	if( pLen > nLen ){
		return SXERR_NOTFOUND;
	}
	zEnd = &zIn[nLen-pLen];
	for(;;){
		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;
		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;
		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;
		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;
	}
	return SXERR_NOTFOUND;
}
#endif /* PH7_DISABLE_BUILTIN_FUNC */
