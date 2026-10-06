# src/sx/sxmem.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 487/563 lines (86.50%)

[Root index](../../index.md) | [Directory index](index.md)

|       Hits | Line | Source |
| ---------: | ---: | :--- |
|          - |    1 | `/**` |
|          - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|          - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|          - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|          - |    5 | ` */` |
|          - |    6 | `#include "sxtypes.h"` |
|          - |    7 | `#include "sxmacros.h"` |
|          - |    8 | `#include "sxset.h"` |
|          - |    9 | `#include "sxmem.h"` |
|          - |   10 | `#include "sxmutex.h"` |
|          - |   11 | `#include "sxstr.h"` |
|          - |   12 | `#if defined(__WINNT__)` |
|          - |   13 | `#include <Windows.h>` |
|          - |   14 | `#else` |
|          - |   15 | `#include <stdlib.h>` |
|          - |   16 | `#endif` |
|          - |   17 | `/*` |
|          - |   18 | ` * ---------------------------------------------------------------------------` |
|          - |   19 | ` * PHL_MEM_CENSUS -- where the heap actually IS, at the high-water mark.` |
|          - |   20 | ` * ---------------------------------------------------------------------------` |
|          - |   21 | ` * Compiled out entirely unless PHL_MEM_CENSUS is defined; build-aux/census.sh` |
|          - |   22 | ` * drives it, and it exists to stop a sixth hand-rolled one. It records every LIVE` |
|          - |   23 | ` * object handed out by the public doors of this file, each tagged with the` |
|          - |   24 | ` * return address that asked for it, and dumps a ranked table the moment the` |
|          - |   25 | ` * recorded live bytes first cross PHL_CENSUS_AT.` |
|          - |   26 | ` *` |
|          - |   27 | ` * Three properties it has to have, each of which cost a session to learn:` |
|          - |   28 | ` *` |
|          - |   29 | ` *  - It must not allocate through the allocator it is measuring. The table is` |
|          - |   30 | ` *    one mmap taken at first use and never grown; if it ever filled, recording` |
|          - |   31 | ` *    would STOP and the dump would say so rather than lie by a smaller number.` |
|          - |   32 | ` *  - Every free path must reach it, or a reused address answers for a dead` |
|          - |   33 | ` *    object. The two free doors are not enough on their own: MemBackendRelease` |
|          - |   34 | ` *    bulk-frees a whole backend without passing through either, so a record` |
|          - |   35 | ` *    carries its backend and a release sweeps the table for it.` |
|          - |   36 | ` *  - The tag has to survive the dump. Site addresses are emitted RELATIVE to` |
|          - |   37 | ` *    the PIE load base (the ADDRESS of __executable_start is that base at run` |
|          - |   38 | ` *    time), so addr2line takes them exactly as printed -- there is no slide` |
|          - |   39 | ` *    left to subtract by hand.` |
|          - |   40 | ` *` |
|          - |   41 | ` * Deletion is backward-shift, not tombstones: a run frees millions of objects` |
|          - |   42 | ` * and a tombstone per free would saturate the table however few are live.` |
|          - |   43 | ` *` |
|          - |   44 | ` * The protocol is two runs, and build-aux/census.sh runs both:` |
|          - |   45 | ` *   1. no PHL_CENSUS_AT      -- prints the peak recorded live bytes at exit` |
|          - |   46 | ` *   2. PHL_CENSUS_AT=<peak>  -- dumps the table the first time the live bytes` |
|          - |   47 | ` *                               reach it, then keeps recording, so this run` |
|          - |   48 | ` *                               ALSO reports its own peak at exit. Comparing` |
|          - |   49 | ` *                               the two is the only way to catch a workload` |
|          - |   50 | ` *                               that did different work the second time.` |
|          - |   51 | ` */` |
|          - |   52 | `#if defined(PHL_MEM_CENSUS)` |
|          - |   53 | `#include <stdio.h>` |
|          - |   54 | `#include <stdlib.h>` |
|          - |   55 | `#include <sys/mman.h>` |
|          - |   56 |  |
|          - |   57 | `extern char __executable_start[];   /* its ADDRESS is the PIE load base */` |
|          - |   58 |  |
|          - |   59 | `#define PHL_CENSUS_KIND_DIRECT 0    /* SyMemBackendAlloc/Realloc -- a SyMemBlock */` |
|          - |   60 | `#define PHL_CENSUS_KIND_POOL   1    /* SyMemBackendPoolAlloc     -- a pool chunk */` |
|          - |   61 |  |
|          - |   62 | `typedef struct phl_census_rec phl_census_rec;` |
|          - |   63 | `struct phl_census_rec {` |
|          - |   64 | `	void *pPtr;              /* what was handed out; 0 = free slot */` |
|          - |   65 | `	void *pSite;             /* __builtin_return_address(0) at the public door */` |
|          - |   66 | `	void *pBackend;          /* which SyMemBackend owns it (for the release sweep) */` |
|          - |   67 | `	sxu32 nByte;             /* bytes the caller ASKED for */` |
|          - |   68 | `	sxu32 nChunk;            /* bytes the allocator actually spent on it */` |
|          - |   69 | `	sxu32 nKind;             /* PHL_CENSUS_KIND_* */` |
|          - |   70 | `	sxu32 nPad;` |
|          - |   71 | `};` |
|          - |   72 | `static struct {` |
|          - |   73 | `	int bReady;              /* 0 = untouched, 1 = live, -1 = off */` |
|          - |   74 | `	int bFull;               /* the table filled; recording stopped */` |
|          - |   75 | `	int bDumped;             /* the one dump PHL_CENSUS_AT buys has been taken */` |
|          - |   76 | `	phl_census_rec *aRec;` |
|          - |   77 | `	sxu32 nSlot;             /* power of two */` |
|          - |   78 | `	sxu32 nMask;` |
|          - |   79 | `	sxu32 nLiveRec;          /* live records == occupied slots */` |
|          - |   80 | `	sxu64 nLiveByte;         /* live chunk bytes */` |
|          - |   81 | `	sxu64 nPeakByte;         /* high-water of nLiveByte */` |
|          - |   82 | `	sxu64 nAt;               /* PHL_CENSUS_AT, 0 = never dump */` |
|          - |   83 | `	const char *zOut;        /* PHL_CENSUS_OUT */` |
|          - |   84 | `} sCensus;` |
|          - |   85 |  |
|          - |   86 | `static sxu32 CensusHashPtr(const void *p)` |
|          - |   87 | `{` |
|          - |   88 | `	/* Fibonacci scramble: the low four to six bits of a chunk address are` |
|          - |   89 | `	 * always the same, so the raw pointer is not a key. */` |
|          - |   90 | `	sxu64 x = (sxu64)(sxuptr)p;` |
|          - |   91 | `	x ^= x >> 33;` |
|          - |   92 | `	x *= (sxu64)0xff51afd7ed558ccdULL;` |
|          - |   93 | `	x ^= x >> 29;` |
|          - |   94 | `	return (sxu32)x;` |
|          - |   95 | `}` |
|          - |   96 | `static void CensusDump(void);` |
|          - |   97 | `static void CensusInit(void)` |
|          - |   98 | `{` |
|          - |   99 | `	const char *zSlots,*zAt;` |
|          - |  100 | `	sxu32 nSlot = 1u << 21;   /* 2M records x 40 B = 80 MB; the phpcs step's peak` |
|          - |  101 | `	                           * is ~700k live objects, a 33% load */` |
|          - |  102 | `	size_t nByte;` |
|          - |  103 | `	void *pMap;` |
|          - |  104 |  |
|          - |  105 | `	sCensus.bReady = -1;      /* pessimistic: any early return leaves it off */` |
|          - |  106 | `	zSlots = getenv("PHL_CENSUS_SLOTS");` |
|          - |  107 | `	if( zSlots ){` |
|          - |  108 | `		sxu32 n = (sxu32)strtoul(zSlots,0,0);` |
|          - |  109 | `		if( n >= 1024 ){` |
|          - |  110 | `			nSlot = 1;` |
|          - |  111 | `			while( nSlot < n && nSlot < (1u<<28) ){ nSlot <<= 1; }` |
|          - |  112 | `		}` |
|          - |  113 | `	}` |
|          - |  114 | `	nByte = (size_t)nSlot * sizeof(phl_census_rec);` |
|          - |  115 | `	pMap = mmap(0,nByte,PROT_READ\|PROT_WRITE,MAP_PRIVATE\|MAP_ANONYMOUS,-1,0);` |
|          - |  116 | `	if( pMap == MAP_FAILED ){` |
|          - |  117 | `		fprintf(stderr,"census: cannot map %lu bytes -- disabled\n",(unsigned long)nByte);` |
|          - |  118 | `		return;` |
|          - |  119 | `	}` |
|          - |  120 | `	sCensus.aRec  = (phl_census_rec *)pMap;` |
|          - |  121 | `	sCensus.nSlot = nSlot;` |
|          - |  122 | `	sCensus.nMask = nSlot - 1;` |
|          - |  123 | `	zAt = getenv("PHL_CENSUS_AT");` |
|          - |  124 | `	sCensus.nAt = zAt ? (sxu64)strtoull(zAt,0,0) : 0;` |
|          - |  125 | `	sCensus.zOut = getenv("PHL_CENSUS_OUT");` |
|          - |  126 | `	if( sCensus.zOut == 0 ){` |
|          - |  127 | `		sCensus.zOut = "phl-census.out";` |
|          - |  128 | `	}` |
|          - |  129 | `	sCensus.bReady = 1;` |
|          - |  130 | `}` |
|          - |  131 | `/* The slot holding pPtr, or -- when bInsert -- the free slot it belongs in. */` |
|          - |  132 | `static sxu32 CensusSlot(void *pPtr,int bInsert)` |
|          - |  133 | `{` |
|          - |  134 | `	sxu32 i = CensusHashPtr(pPtr) & sCensus.nMask;` |
|          - |  135 | `	for(;;){` |
|          - |  136 | `		void *p = sCensus.aRec[i].pPtr;` |
|          - |  137 | `		if( p == pPtr ){` |
|          - |  138 | `			return i;` |
|          - |  139 | `		}` |
|          - |  140 | `		if( p == 0 ){` |
|          - |  141 | `			return bInsert ? i : SXU32_HIGH;` |
|          - |  142 | `		}` |
|          - |  143 | `		i = (i + 1) & sCensus.nMask;` |
|          - |  144 | `	}` |
|          - |  145 | `}` |
|          - |  146 | `/*` |
|          - |  147 | ` * Backward-shift deletion. Emptying the slot outright would cut every probe` |
|          - |  148 | ` * chain that runs through it; a tombstone would never be reclaimed. Instead the` |
|          - |  149 | ` * hole walks forward, pulling back each record that can still be found from` |
|          - |  150 | ` * where it lands.` |
|          - |  151 | ` */` |
|          - |  152 | `static void CensusDeleteAt(sxu32 i)` |
|          - |  153 | `{` |
|          - |  154 | `	sxu32 j = i;` |
|          - |  155 | `	for(;;){` |
|          - |  156 | `		sxu32 k;` |
|          - |  157 | `		int bMove;` |
|          - |  158 | `		j = (j + 1) & sCensus.nMask;` |
|          - |  159 | `		if( sCensus.aRec[j].pPtr == 0 ){` |
|          - |  160 | `			break;` |
|          - |  161 | `		}` |
|          - |  162 | `		k = CensusHashPtr(sCensus.aRec[j].pPtr) & sCensus.nMask;` |
|          - |  163 | `		/* Does k lie cyclically in (i,j]? If it does, record j is still` |
|          - |  164 | `		 * reachable from i's chain and must stay where it is. */` |
|          - |  165 | `		bMove = (i <= j) ? !(i < k && k <= j) : !(i < k \|\| k <= j);` |
|          - |  166 | `		if( bMove ){` |
|          - |  167 | `			sCensus.aRec[i] = sCensus.aRec[j];` |
|          - |  168 | `			i = j;` |
|          - |  169 | `		}` |
|          - |  170 | `	}` |
|          - |  171 | `	sCensus.aRec[i].pPtr = 0;` |
|          - |  172 | `}` |
|          - |  173 | `static void CensusRecord(SyMemBackend *pBackend,void *pPtr,void *pSite,` |
|          - |  174 | `	sxu32 nByte,sxu32 nChunk,sxu32 nKind)` |
|          - |  175 | `{` |
|          - |  176 | `	phl_census_rec *p;` |
|          - |  177 | `	sxu32 i;` |
|          - |  178 | `	if( sCensus.bReady == 0 ){` |
|          - |  179 | `		CensusInit();` |
|          - |  180 | `	}` |
|          - |  181 | `	if( sCensus.bReady != 1 \|\| pPtr == 0 ){` |
|          - |  182 | `		return;` |
|          - |  183 | `	}` |
|          - |  184 | `	if( sCensus.nLiveRec * 4 >= sCensus.nSlot * 3 ){` |
|          - |  185 | `		/* Past a 75% load the probe chains stop being chains. Stop, and say` |
|          - |  186 | `		 * so twice -- here and in the dump -- so no one reads a short answer` |
|          - |  187 | `		 * as a small heap. */` |
|          - |  188 | `		if( !sCensus.bFull ){` |
|          - |  189 | `			sCensus.bFull = 1;` |
|          - |  190 | `			fprintf(stderr,"census: table full at %lu records -- "` |
|          - |  191 | `				"raise PHL_CENSUS_SLOTS\n",(unsigned long)sCensus.nLiveRec);` |
|          - |  192 | `		}` |
|          - |  193 | `		return;` |
|          - |  194 | `	}` |
|          - |  195 | `	i = CensusSlot(pPtr,1);` |
|          - |  196 | `	p = &sCensus.aRec[i];` |
|          - |  197 | `	if( p->pPtr == 0 ){` |
|          - |  198 | `		sCensus.nLiveRec++;` |
|          - |  199 | `	}else{` |
|          - |  200 | `		sCensus.nLiveByte -= p->nChunk;   /* same address re-recorded */` |
|          - |  201 | `	}` |
|          - |  202 | `	p->pPtr = pPtr;` |
|          - |  203 | `	p->pSite = pSite;` |
|          - |  204 | `	p->pBackend = pBackend;` |
|          - |  205 | `	p->nByte = nByte;` |
|          - |  206 | `	p->nChunk = nChunk;` |
|          - |  207 | `	p->nKind = nKind;` |
|          - |  208 | `	sCensus.nLiveByte += nChunk;` |
|          - |  209 | `	if( sCensus.nLiveByte > sCensus.nPeakByte ){` |
|          - |  210 | `		sCensus.nPeakByte = sCensus.nLiveByte;` |
|          - |  211 | `	}` |
|          - |  212 | `	if( sCensus.nAt && !sCensus.bDumped && sCensus.nLiveByte >= sCensus.nAt ){` |
|          - |  213 | `		CensusDump();` |
|          - |  214 | `	}` |
|          - |  215 | `}` |
|          - |  216 | `static void CensusForget(void *pPtr)` |
|          - |  217 | `{` |
|          - |  218 | `	sxu32 i;` |
|          - |  219 | `	if( sCensus.bReady != 1 \|\| pPtr == 0 ){` |
|          - |  220 | `		return;` |
|          - |  221 | `	}` |
|          - |  222 | `	i = CensusSlot(pPtr,0);` |
|          - |  223 | `	if( i == SXU32_HIGH ){` |
|          - |  224 | `		return;` |
|          - |  225 | `	}` |
|          - |  226 | `	sCensus.nLiveByte -= sCensus.aRec[i].nChunk;` |
|          - |  227 | `	sCensus.nLiveRec--;` |
|          - |  228 | `	CensusDeleteAt(i);` |
|          - |  229 | `}` |
|          - |  230 | `/*` |
|          - |  231 | ` * A backend released every one of its blocks at once, without passing through` |
|          - |  232 | ` * either free door -- and the pool chunks carved out of those blocks were never` |
|          - |  233 | ` * blocks of their own to begin with. Sweep its records out, or the next` |
|          - |  234 | ` * allocation to land on one of those addresses answers for a dead object.` |
|          - |  235 | ` * Repeated because backward-shift deletion can move a record to a slot the` |
|          - |  236 | ` * sweep has already walked past.` |
|          - |  237 | ` */` |
|          - |  238 | `static void CensusForgetBackend(SyMemBackend *pBackend)` |
|          - |  239 | `{` |
|          - |  240 | `	int bMoved = 1;` |
|          - |  241 | `	if( sCensus.bReady != 1 ){` |
|          - |  242 | `		return;` |
|          - |  243 | `	}` |
|          - |  244 | `	while( bMoved ){` |
|          - |  245 | `		sxu32 i = 0;` |
|          - |  246 | `		bMoved = 0;` |
|          - |  247 | `		while( i < sCensus.nSlot ){` |
|          - |  248 | `			phl_census_rec *p = &sCensus.aRec[i];` |
|          - |  249 | `			if( p->pPtr != 0 && p->pBackend == (void *)pBackend ){` |
|          - |  250 | `				sCensus.nLiveByte -= p->nChunk;` |
|          - |  251 | `				sCensus.nLiveRec--;` |
|          - |  252 | `				CensusDeleteAt(i);` |
|          - |  253 | `				bMoved = 1;` |
|          - |  254 | `				continue;   /* another record may have shifted INTO i */` |
|          - |  255 | `			}` |
|          - |  256 | `			i++;` |
|          - |  257 | `		}` |
|          - |  258 | `	}` |
|          - |  259 | `}` |
|          - |  260 | `/*` |
|          - |  261 | ` * The dump aggregates by (site, kind, chunk size, requested-size band) so one` |
|          - |  262 | ` * table answers both questions the census is for: WHO asked for the bytes, and` |
|          - |  263 | ` * what SHAPE the requests were. The band column is the only place a fact like` |
|          - |  264 | ` * "most of these strings are sixteen bytes in a forty-byte hole" is visible.` |
|          - |  265 | ` */` |
|          - |  266 | `typedef struct phl_census_agg phl_census_agg;` |
|          - |  267 | `struct phl_census_agg {` |
|          - |  268 | `	void *pSite;` |
|          - |  269 | `	sxu32 nKind;` |
|          - |  270 | `	sxu32 nChunk;` |
|          - |  271 | `	sxu32 nBand;` |
|          - |  272 | `	sxu32 nCount;` |
|          - |  273 | `	sxu64 nByte;` |
|          - |  274 | `	sxu64 nChunkTotal;` |
|          - |  275 | `};` |
|          - |  276 | `/* Requested size -> band: exact eight-byte steps below 256, powers of two above. */` |
|          - |  277 | `static sxu32 CensusBand(sxu32 nByte)` |
|          - |  278 | `{` |
|          - |  279 | `	sxu32 k;` |
|          - |  280 | `	if( nByte < 256 ){` |
|          - |  281 | `		return nByte >> 3;              /* 0 .. 31 */` |
|          - |  282 | `	}` |
|          - |  283 | `	k = 8;` |
|          - |  284 | `	while( k < 31 && nByte >= (1u << (k + 1)) ){` |
|          - |  285 | `		k++;` |
|          - |  286 | `	}` |
|          - |  287 | `	return 24 + k;                      /* 32 .. 55 */` |
|          - |  288 | `}` |
|          - |  289 | `static void CensusBandRange(sxu32 nBand,sxu32 *pLo,sxu32 *pHi)` |
|          - |  290 | `{` |
|          - |  291 | `	if( nBand < 32 ){` |
|          - |  292 | `		*pLo = nBand << 3;` |
|          - |  293 | `		*pHi = (nBand << 3) + 7;` |
|          - |  294 | `	}else{` |
|          - |  295 | `		sxu32 k = nBand - 24;` |
|          - |  296 | `		*pLo = 1u << k;` |
|          - |  297 | `		*pHi = (k >= 31) ? SXU32_HIGH : ((1u << (k + 1)) - 1);` |
|          - |  298 | `	}` |
|          - |  299 | `}` |
|          - |  300 | `static int CensusAggCmp(const void *a,const void *b)` |
|          - |  301 | `{` |
|          - |  302 | `	const phl_census_agg *pA = (const phl_census_agg *)a;` |
|          - |  303 | `	const phl_census_agg *pB = (const phl_census_agg *)b;` |
|          - |  304 | `	if( pA->nChunkTotal < pB->nChunkTotal ){ return  1; }` |
|          - |  305 | `	if( pA->nChunkTotal > pB->nChunkTotal ){ return -1; }` |
|          - |  306 | `	return 0;` |
|          - |  307 | `}` |
|          - |  308 | `static void CensusDump(void)` |
|          - |  309 | `{` |
|          - |  310 | `	const sxu32 nAggSlot = 1u << 17;` |
|          - |  311 | `	phl_census_agg *aAgg;` |
|          - |  312 | `	sxu32 nAgg = 0;` |
|          - |  313 | `	sxu32 nLost = 0;` |
|          - |  314 | `	sxu32 i;` |
|          - |  315 | `	FILE *pOut;` |
|          - |  316 |  |
|          - |  317 | `	/* The dump allocates and writes; it must not be reentered from either.` |
|          - |  318 | `	 * Recording resumes afterwards so the run still reports its TRUE peak at` |
|          - |  319 | `	 * exit -- which is the only way to notice that run two of the protocol` |
|          - |  320 | `	 * measured a different workload than run one (a tool with a warm cache` |
|          - |  321 | `	 * measures itself, not the engine). */` |
|          - |  322 | `	sCensus.bReady = -1;` |
|          - |  323 | `	sCensus.bDumped = 1;` |
|          - |  324 | `	aAgg = (phl_census_agg *)calloc(nAggSlot,sizeof(phl_census_agg));` |
|          - |  325 | `	if( aAgg == 0 ){` |
|          - |  326 | `		fprintf(stderr,"census: no room to aggregate\n");` |
|          - |  327 | `		sCensus.bReady = 1;` |
|          - |  328 | `		return;` |
|          - |  329 | `	}` |
|          - |  330 | `	for( i = 0 ; i < sCensus.nSlot ; ++i ){` |
|          - |  331 | `		phl_census_rec *p = &sCensus.aRec[i];` |
|          - |  332 | `		sxu32 nBand,h,n;` |
|          - |  333 | `		if( p->pPtr == 0 ){` |
|          - |  334 | `			continue;` |
|          - |  335 | `		}` |
|          - |  336 | `		nBand = CensusBand(p->nByte);` |
|          - |  337 | `		h = (CensusHashPtr(p->pSite) ^ (p->nChunk * 2654435761u)` |
|          - |  338 | `			^ (nBand * 40503u) ^ (p->nKind * 97u)) & (nAggSlot - 1);` |
|          - |  339 | `		for( n = 0 ; n < nAggSlot ; ++n ){` |
|          - |  340 | `			phl_census_agg *q = &aAgg[h];` |
|          - |  341 | `			if( q->nCount == 0 ){` |
|          - |  342 | `				q->pSite = p->pSite; q->nKind = p->nKind;` |
|          - |  343 | `				q->nChunk = p->nChunk; q->nBand = nBand;` |
|          - |  344 | `			}` |
|          - |  345 | `			if( q->pSite == p->pSite && q->nKind == p->nKind` |
|          - |  346 | `				&& q->nChunk == p->nChunk && q->nBand == nBand ){` |
|          - |  347 | `				q->nCount++;` |
|          - |  348 | `				q->nByte += p->nByte;` |
|          - |  349 | `				q->nChunkTotal += p->nChunk;` |
|          - |  350 | `				break;` |
|          - |  351 | `			}` |
|          - |  352 | `			h = (h + 1) & (nAggSlot - 1);` |
|          - |  353 | `		}` |
|          - |  354 | `		if( n == nAggSlot ){` |
|          - |  355 | `			nLost++;` |
|          - |  356 | `		}` |
|          - |  357 | `	}` |
|          - |  358 | `	for( i = 0 ; i < nAggSlot ; ++i ){` |
|          - |  359 | `		if( aAgg[i].nCount ){` |
|          - |  360 | `			aAgg[nAgg++] = aAgg[i];` |
|          - |  361 | `		}` |
|          - |  362 | `	}` |
|          - |  363 | `	qsort(aAgg,nAgg,sizeof(phl_census_agg),CensusAggCmp);` |
|          - |  364 | `	pOut = fopen(sCensus.zOut,"w");` |
|          - |  365 | `	if( pOut == 0 ){` |
|          - |  366 | `		pOut = stderr;` |
|          - |  367 | `	}` |
|          - |  368 | `	fprintf(pOut,"# phl heap census\n");` |
|          - |  369 | `	fprintf(pOut,"# base 0x%lx -- SITE addresses below are ALREADY relative to it\n",` |
|          - |  370 | `		(unsigned long)(sxuptr)__executable_start);` |
|          - |  371 | `	fprintf(pOut,"# live-bytes %llu  live-records %lu  peak-bytes %llu  rows %lu%s%s\n",` |
|          - |  372 | `		(unsigned long long)sCensus.nLiveByte,(unsigned long)sCensus.nLiveRec,` |
|          - |  373 | `		(unsigned long long)sCensus.nPeakByte,(unsigned long)nAgg,` |
|          - |  374 | `		sCensus.bFull ? "  TRUNCATED(table-filled)" : "",` |
|          - |  375 | `		nLost ? "  TRUNCATED(rows-filled)" : "");` |
|          - |  376 | `	fprintf(pOut,"# SITE <rel-addr> <pool\|direct> <count> <chunk-bytes> <req-bytes>"` |
|          - |  377 | `		" <chunk-size> <req-lo> <req-hi>\n");` |
|          - |  378 | `	for( i = 0 ; i < nAgg ; ++i ){` |
|          - |  379 | `		sxu32 lo,hi;` |
|          - |  380 | `		CensusBandRange(aAgg[i].nBand,&lo,&hi);` |
|          - |  381 | `		fprintf(pOut,"SITE %lx %s %lu %llu %llu %lu %lu %lu\n",` |
|          - |  382 | `			(unsigned long)((char *)aAgg[i].pSite - __executable_start),` |
|          - |  383 | `			aAgg[i].nKind == PHL_CENSUS_KIND_POOL ? "pool" : "direct",` |
|          - |  384 | `			(unsigned long)aAgg[i].nCount,` |
|          - |  385 | `			(unsigned long long)aAgg[i].nChunkTotal,` |
|          - |  386 | `			(unsigned long long)aAgg[i].nByte,` |
|          - |  387 | `			(unsigned long)aAgg[i].nChunk,` |
|          - |  388 | `			(unsigned long)lo,(unsigned long)hi);` |
|          - |  389 | `	}` |
|          - |  390 | `	if( pOut != stderr ){` |
|          - |  391 | `		fclose(pOut);` |
|          - |  392 | `	}` |
|          - |  393 | `	fprintf(stderr,"census: dumped %lu rows / %llu live bytes to %s\n",` |
|          - |  394 | `		(unsigned long)nAgg,(unsigned long long)sCensus.nLiveByte,sCensus.zOut);` |
|          - |  395 | `	free(aAgg);` |
|          - |  396 | `	sCensus.bReady = 1;` |
|          - |  397 | `}` |
|          - |  398 | `/* Run 1 of the protocol: say what the peak was, so run 2 can aim at it. */` |
|          - |  399 | `static void CensusAtExit(void) __attribute__((destructor));` |
|          - |  400 | `static void CensusAtExit(void)` |
|          - |  401 | `{` |
|          - |  402 | `	if( sCensus.bReady == 0 \|\| sCensus.aRec == 0 ){` |
|          - |  403 | `		return;   /* never armed, or the table could not be mapped */` |
|          - |  404 | `	}` |
|          - |  405 | `	fprintf(stderr,"census: peak recorded live bytes %llu (%.1f MiB)%s\n",` |
|          - |  406 | `		(unsigned long long)sCensus.nPeakByte,` |
|          - |  407 | `		(double)sCensus.nPeakByte / (1024.0*1024.0),` |
|          - |  408 | `		sCensus.bFull ? " -- TRUNCATED, raise PHL_CENSUS_SLOTS" : "");` |
|          - |  409 | `}` |
|          - |  410 | `/* A pool chunk's real cost is its BUCKET, which only its header knows. */` |
|          - |  411 | `static sxu32 CensusPoolChunkSize(void *pChunk,sxu32 nByte)` |
|          - |  412 | `{` |
|          - |  413 | `	SyMemHeader *pHeader = (SyMemHeader *)(((char *)pChunk) - sizeof(SyMemHeader));` |
|          - |  414 | `	sxu32 nBucket = pHeader->nBucket & 0xFFFF;` |
|          - |  415 | `	if( nBucket == SXU16_HIGH ){` |
|          - |  416 | `		/* Big block: a SyMemBlock and a SyMemHeader around the request. */` |
|          - |  417 | `		return nByte + (sxu32)sizeof(SyMemHeader) + (sxu32)sizeof(SyMemBlock);` |
|          - |  418 | `	}` |
|          - |  419 | `	return 1u << (nBucket + SXMEM_POOL_INCR);` |
|          - |  420 | `}` |
|          - |  421 | `#define PHL_CENSUS_DIRECT(B,P,N) \` |
|          - |  422 | `	CensusRecord(B,P,__builtin_return_address(0),N, \` |
|          - |  423 | `		(N) + (sxu32)sizeof(SyMemBlock),PHL_CENSUS_KIND_DIRECT)` |
|          - |  424 | `#define PHL_CENSUS_POOL(B,P,N) \` |
|          - |  425 | `	CensusRecord(B,P,__builtin_return_address(0),N, \` |
|          - |  426 | `		CensusPoolChunkSize(P,N),PHL_CENSUS_KIND_POOL)` |
|          - |  427 | `#define PHL_CENSUS_FORGET(P)         CensusForget(P)` |
|          - |  428 | `#define PHL_CENSUS_FORGET_BACKEND(B) CensusForgetBackend(B)` |
|          - |  429 | `#else` |
|          - |  430 | `#define PHL_CENSUS_DIRECT(B,P,N)     ((void)0)` |
|          - |  431 | `#define PHL_CENSUS_POOL(B,P,N)       ((void)0)` |
|          - |  432 | `#define PHL_CENSUS_FORGET(P)         ((void)0)` |
|          - |  433 | `#define PHL_CENSUS_FORGET_BACKEND(B) ((void)0)` |
|          - |  434 | `#endif /* PHL_MEM_CENSUS */` |
|          - |  435 |  |
|  116201428 |  436 | `static void * SyOSHeapAlloc(sxu32 nByte)` |
|          5 |  437 | `{` |
|          - |  438 | `	void *pNew;` |
|          - |  439 | `#if defined(__WINNT__)` |
|          5 |  440 | `	pNew = HeapAlloc(GetProcessHeap(),0,nByte);` |
|          - |  441 | `#else` |
|  116201428 |  442 | `	pNew = malloc((size_t)nByte);` |
|          - |  443 | `#endif` |
|  116201433 |  444 | `	return pNew;` |
|          5 |  445 | `}` |
|    1873758 |  446 | `static void * SyOSHeapRealloc(void *pOld,sxu32 nByte)` |
|          5 |  447 | `{` |
|          - |  448 | `	void *pNew;` |
|          - |  449 | `#if defined(__WINNT__)` |
|          5 |  450 | `	pNew = HeapReAlloc(GetProcessHeap(),0,pOld,nByte);` |
|          - |  451 | `#else` |
|    1873758 |  452 | `	pNew = realloc(pOld,(size_t)nByte);` |
|          - |  453 | `#endif` |
|    1873763 |  454 | `	return pNew;` |
|          5 |  455 | `}` |
|  116366773 |  456 | `static void SyOSHeapFree(void *pPtr)` |
|          5 |  457 | `{` |
|          - |  458 | `#if defined(__WINNT__)` |
|          5 |  459 | `	HeapFree(GetProcessHeap(),0,pPtr);` |
|          - |  460 | `#else` |
|  116366773 |  461 | `	free(pPtr);` |
|          - |  462 | `#endif` |
|  116366778 |  463 | `}` |
|          - |  464 |  |
|          - |  465 |  |
|          - |  466 | `/*` |
|          - |  467 | ` * Zero a block. Every ph7_value, VM frame, hashmap and reference record is born` |
|          - |  468 | ` * through here -- 52M calls in a nine-second run of the ecosystem gate's phpcs` |
|          - |  469 | ` * step -- and it used to be a hand-unrolled byte loop. memset is the same` |
|          - |  470 | ` * operation a vector register at a time, and unlike the COMPARE (see` |
|          - |  471 | ` * SX_MACRO_FAST_CMP) it cannot over-read: nSize is memory the caller owns.` |
|          - |  472 | ` */` |
|  342962910 |  473 | `PH7_PRIVATE void SyZero(void *pSrc,sxu32 nSize)` |
|          5 |  474 | `{` |
|          - |  475 | `#if defined(UNTRUST)` |
|          - |  476 | `	if( pSrc == 0 \|\| nSize <= 0 ){` |
|          - |  477 | `		return ;` |
|          - |  478 | `	}` |
|          - |  479 | `#endif` |
|  342962915 |  480 | `	if( nSize > 0 ){` |
|  342962725 |  481 | `		memset(pSrc,0,(size_t)nSize);` |
|  171113547 |  482 | `	}` |
|  342962915 |  483 | `}` |
|  209820687 |  484 | `PH7_PRIVATE sxi32 SyMemcmp(const void *pB1,const void *pB2,sxu32 nSize)` |
|          5 |  485 | `{` |
|          - |  486 | `	sxi32 rc;` |
|  209820692 |  487 | `	if( nSize <= 0 ){` |
|      61519 |  488 | `		return 0;` |
|          - |  489 | `	}` |
|  209759178 |  490 | `	if( pB1 == 0 \|\| pB2 == 0 ){` |
|        ! 0 |  491 | `		return pB1 != 0 ? 1 : (pB2 == 0 ? 0 : -1);` |
|          - |  492 | `	}` |
|  270139326 |  493 | `	SX_MACRO_FAST_CMP(pB1,pB2,nSize,rc);` |
|  209759178 |  494 | `	return rc;` |
|  104791173 |  495 | `}` |
|    9314480 |  496 | `PH7_PRIVATE sxu32 SyMemcpy(const void *pSrc,void *pDest,sxu32 nLen)` |
|          5 |  497 | `{` |
|          - |  498 | `#if defined(UNTRUST)` |
|          - |  499 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|          - |  500 | `		return 0;` |
|          - |  501 | `	}` |
|          - |  502 | `#endif` |
|    9314485 |  503 | `	if( pSrc == (const void *)pDest ){` |
|        ! 0 |  504 | `		return nLen;` |
|          - |  505 | `	}` |
|    9314485 |  506 | `	SX_MACRO_FAST_MEMCPY(pSrc,pDest,nLen);` |
|    9314485 |  507 | `	return nLen;` |
|    4653730 |  508 | `}` |
|          - |  509 | `/*` |
|          - |  510 | ` * The OS methods are malloc/realloc/free and nothing else.` |
|          - |  511 | ` *` |
|          - |  512 | ` * They used to carry an 8-byte size prefix ahead of every allocation, for two` |
|          - |  513 | ` * readers. The first was xChunkSize, which no call site in the tree has ever` |
|          - |  514 | ` * used -- SyMemBackendInitFromOthers checked it for non-NULL and nobody called` |
|          - |  515 | ` * it, and sx.h has always documented it as [Optional:]. The second was a` |
|          - |  516 | ` * shrink-in-place shortcut in MemOSRealloc, which saved a realloc() the C` |
|          - |  517 | ` * library answers in a few instructions when the block already fits.` |
|          - |  518 | ` *` |
|          - |  519 | ` * Every direct (non-pool) allocation the engine makes pays for that prefix on` |
|          - |  520 | ` * TOP of the backend's own 24-byte SyMemBlock. On the ecosystem gate's phpcs` |
|          - |  521 | ` * step there are 160,541,911 of them in one run and ~415,000 live at the peak.` |
|          - |  522 | ` * Removing it costs one store fewer per allocation and one cold cache line` |
|          - |  523 | ` * fewer per free, and takes the run's peak RSS from 180.1 MB to 172.2 MB` |
|          - |  524 | ` * (-4.4%, three runs each, the two groups not overlapping). That is more than` |
|          - |  525 | ` * the 8 bytes x 415,000 the arithmetic predicts, because malloc rounds a chunk` |
|          - |  526 | ` * to 16: a 16-byte string body asked 48 bytes and got a 64-byte chunk, and now` |
|          - |  527 | ` * asks 40 and gets 48. Neither the heap census nor memory_get_peak_usage() can` |
|          - |  528 | ` * see any of it -- both count what the BACKEND handed out, and this header was` |
|          - |  529 | ` * underneath.` |
|          - |  530 | ` *` |
|          - |  531 | ` * Alignment: what the reference word's pointer tags need is FOUR bytes` |
|          - |  532 | ` * (VM_REF_TAG_MASK, ph7int.h), and what a ph7_value needs is eight, for its` |
|          - |  533 | ` * double. malloc and HeapAlloc both return at least 8-aligned (16 on x86-64),` |
|          - |  534 | ` * SyMemBlock is a multiple of 8, and the pool's SyMemHeader is one pointer --` |
|          - |  535 | ` * so every chunk the backend hands out is still 8-aligned without the prefix.` |
|          - |  536 | ` * Nothing in the engine allocates a type that wants more; sxlongreal, the only` |
|          - |  537 | ` * long double in the tree, lives on sxfmt.c's stack.` |
|          - |  538 | ` */` |
|  116201428 |  539 | `static void * MemOSAlloc(sxu32 nBytes)` |
|          5 |  540 | `{` |
|  116201433 |  541 | `	return SyOSHeapAlloc(nBytes);` |
|          5 |  542 | `}` |
|    1873758 |  543 | `static void * MemOSRealloc(void *pOld,sxu32 nBytes)` |
|          5 |  544 | `{` |
|    1873763 |  545 | `	return SyOSHeapRealloc(pOld,nBytes);` |
|          5 |  546 | `}` |
|  116366773 |  547 | `static void MemOSFree(void *pBlock)` |
|          5 |  548 | `{` |
|  116366778 |  549 | `	SyOSHeapFree(pBlock);` |
|  116366778 |  550 | `}` |
|          - |  551 | `/* Export OS allocation methods */` |
|          - |  552 | `static const SyMemMethods sOSAllocMethods = {` |
|          - |  553 | `	MemOSAlloc,` |
|          - |  554 | `	MemOSRealloc,` |
|          - |  555 | `	MemOSFree,` |
|          - |  556 | `	0,  /* xChunkSize: optional, and nothing in the engine asks */` |
|          - |  557 | `	0,` |
|          - |  558 | `	0,` |
|          - |  559 | `	0` |
|          - |  560 | `};` |
|          - |  561 | `/*` |
|          - |  562 | ` * Would this allocation take the backend past its total live-byte ceiling` |
|          - |  563 | ` * (php's memory_limit)?` |
|          - |  564 | ` *` |
|          - |  565 | ` * The first request that would cross it fails AND DISARMS the ceiling, recording` |
|          - |  566 | ` * its size in nMemTried. Disarming is not a leak of the guarantee: the script is` |
|          - |  567 | ` * already over and is about to die, and the fatal that says so has to be able to` |
|          - |  568 | ` * allocate -- a limit that stays armed starves its own error path and the engine` |
|          - |  569 | ` * dies without ever saying why. php keeps a reserve block for the same reason.` |
|          - |  570 | ` * nMemTried is what carries the size php names in the message out to the VM.` |
|          - |  571 | ` */` |
|  118075186 |  572 | `static int MemBackendOverLimit(SyMemBackend *pBackend,sxu32 nByte,sxu32 nFreed)` |
|          5 |  573 | `{` |
|          - |  574 | `	sxu32 nLive;` |
|  118075191 |  575 | `	if( pBackend->nMemLimit == 0 ){` |
|  118067267 |  576 | `		return 0;` |
|          - |  577 | `	}` |
|       7925 |  578 | `	nLive = (pBackend->nMemUsed >= nFreed) ? (pBackend->nMemUsed - nFreed) : 0;` |
|       7925 |  579 | `	if( nLive + nByte <= pBackend->nMemLimit ){` |
|       7925 |  580 | `		return 0;` |
|          - |  581 | `	}` |
|        ! 0 |  582 | `	pBackend->nMemTried = nByte;` |
|        ! 0 |  583 | `	pBackend->nMemLimitHit = pBackend->nMemLimit;` |
|        ! 0 |  584 | `	pBackend->nMemLimit = 0; /* disarm so the fatal can be built and printed */` |
|        ! 0 |  585 | `	return 1;` |
|   58766270 |  586 | `}` |
|  116201428 |  587 | `static void * MemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|          5 |  588 | `{` |
|          - |  589 | `	SyMemBlock *pBlock;` |
|  116201433 |  590 | `	sxi32 nRetry = 0;` |
|          - |  591 |  |
|          - |  592 | `	/* Append an extra block so we can tracks allocated chunks and avoid memory` |
|          - |  593 | `	 * leaks.` |
|          - |  594 | `	 */` |
|  116201433 |  595 | `	nByte += sizeof(SyMemBlock);` |
|          - |  596 | `	/* Enforce the optional per-allocation cap (0 = unlimited). A capped failure` |
|          - |  597 | `	 * returns NULL just like a genuine OS failure, driving the normal SXERR_MEM` |
|          - |  598 | `	 * propagation; the retry callback is intentionally skipped (hard limit). */` |
|  116201433 |  599 | `	if( pBackend->nMaxRequest && nByte > pBackend->nMaxRequest ){` |
|        ! 0 |  600 | `		return 0;` |
|          - |  601 | `	}` |
|          - |  602 | `	/* ...and the total ceiling (php's memory_limit). */` |
|  116201433 |  603 | `	if( MemBackendOverLimit(&(*pBackend),nByte,0) ){` |
|        ! 0 |  604 | `		return 0;` |
|          - |  605 | `	}` |
|   57828673 |  606 | `	for(;;){` |
|   57828678 |  607 | `		pBlock = (SyMemBlock *)pBackend->pMethods->xAlloc(nByte);` |
|  116201428 |  608 | `		if( pBlock != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY` |
|          5 |  609 | `			\|\| SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|   57828678 |  610 | `				break;` |
|          - |  611 | `		}` |
|        ! 0 |  612 | `		nRetry++;` |
|        ! 0 |  613 | `	}` |
|  116201433 |  614 | `	if( pBlock  == 0 ){` |
|        ! 0 |  615 | `		return 0;` |
|          - |  616 | `	}` |
|  116201433 |  617 | `	pBlock->pNext = pBlock->pPrev = 0;` |
|          - |  618 | `	/* Link to the list of already tracked blocks */` |
|  116201433 |  619 | `	MACRO_LD_PUSH(pBackend->pBlocks,pBlock);` |
|          - |  620 | `#if defined(UNTRUST)` |
|          - |  621 | `	pBlock->nGuard = SXMEM_BACKEND_MAGIC;` |
|          - |  622 | `#endif` |
|  116201433 |  623 | `	pBlock->nSize = nByte;` |
|  116201433 |  624 | `	pBackend->nMemUsed += nByte;` |
|  116201433 |  625 | `	if( pBackend->nMemUsed > pBackend->nMemPeak ){` |
|   81699650 |  626 | `		pBackend->nMemPeak = pBackend->nMemUsed;` |
|   40573474 |  627 | `	}` |
|  116201433 |  628 | `	pBackend->nBlock++;` |
|  116201433 |  629 | `	return (void *)&pBlock[1];` |
|   57828678 |  630 | `}` |
|  104344855 |  631 | `PH7_PRIVATE void * SyMemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|          5 |  632 | `{` |
|          - |  633 | `	void *pChunk;` |
|          - |  634 | `#if defined(UNTRUST)` |
|          - |  635 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  636 | `		return 0;` |
|          - |  637 | `	}` |
|          - |  638 | `#endif` |
|  104344860 |  639 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  640 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  641 | `	}` |
|  104344860 |  642 | `	pChunk = MemBackendAlloc(&(*pBackend),nByte);` |
|          - |  643 | `	PHL_CENSUS_DIRECT(pBackend,pChunk,nByte);` |
|  104344860 |  644 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  645 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  646 | `	}` |
|  104344860 |  647 | `	return pChunk;` |
|          5 |  648 | `}` |
|   12904473 |  649 | `static void * MemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|          5 |  650 | `{` |
|          - |  651 | `	SyMemBlock *pBlock,*pNew,*pPrev,*pNext;` |
|   12904478 |  652 | `	sxu32 nRetry = 0;` |
|          - |  653 |  |
|   12904478 |  654 | `	if( pOld == 0 ){` |
|   11030720 |  655 | `		return MemBackendAlloc(&(*pBackend),nByte);` |
|          - |  656 | `	}` |
|    1873763 |  657 | `	pBlock = (SyMemBlock *)(((char *)pOld) - sizeof(SyMemBlock));` |
|          - |  658 | `#if defined(UNTRUST)` |
|          - |  659 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|          - |  660 | `		return 0;` |
|          - |  661 | `	}` |
|          - |  662 | `#endif` |
|    1873763 |  663 | `	nByte += sizeof(SyMemBlock);` |
|          - |  664 | `	/* Enforce the optional per-allocation cap (0 = unlimited); see MemBackendAlloc. */` |
|    1873763 |  665 | `	if( pBackend->nMaxRequest && nByte > pBackend->nMaxRequest ){` |
|        ! 0 |  666 | `		return 0;` |
|          - |  667 | `	}` |
|          - |  668 | `	/* ...and the total ceiling, against the size this block is GIVING BACK: a` |
|          - |  669 | `	 * realloc that shrinks, or grows by less than it already owns, must not be` |
|          - |  670 | `	 * refused for memory it is not asking for. */` |
|    1873763 |  671 | `	if( MemBackendOverLimit(&(*pBackend),nByte,pBlock->nSize) ){` |
|        ! 0 |  672 | `		return 0;` |
|          - |  673 | `	}` |
|    1873763 |  674 | `	pPrev = pBlock->pPrev;` |
|    1873763 |  675 | `	pNext = pBlock->pNext;` |
|          - |  676 | `	{` |
|          - |  677 | `		/* Old size, captured before realloc may move/free the block; the` |
|          - |  678 | `		 * live-byte counter is adjusted by the delta only on success below. */` |
|    1873763 |  679 | `		sxu32 nOld = pBlock->nSize;` |
|     937592 |  680 | `	for(;;){` |
|     937597 |  681 | `		pNew = (SyMemBlock *)pBackend->pMethods->xRealloc(pBlock,nByte);` |
|    1873763 |  682 | `		if( pNew != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY \|\|` |
|        ! 0 |  683 | `			SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|     937597 |  684 | `				break;` |
|          - |  685 | `		}` |
|        ! 0 |  686 | `		nRetry++;` |
|        ! 0 |  687 | `	}` |
|    1873763 |  688 | `	if( pNew == 0 ){` |
|        ! 0 |  689 | `		return 0;` |
|          - |  690 | `	}` |
|    1873763 |  691 | `	if( pNew != pBlock ){` |
|    1286327 |  692 | `		if( pPrev == 0 ){` |
|     740968 |  693 | `			pBackend->pBlocks = pNew;` |
|     546425 |  694 | `		}else{` |
|     545364 |  695 | `			pPrev->pNext = pNew;` |
|          - |  696 | `		}` |
|    1286327 |  697 | `		if( pNext ){` |
|    1285910 |  698 | `			pNext->pPrev = pNew;` |
|     870056 |  699 | `		}` |
|          - |  700 | `#if defined(UNTRUST)` |
|          - |  701 | `		pNew->nGuard = SXMEM_BACKEND_MAGIC;` |
|          - |  702 | `#endif` |
|     870274 |  703 | `	}` |
|          - |  704 | `	/* Apply the size delta to the live-byte counter (underflow-guarded). */` |
|    1873763 |  705 | `	pBackend->nMemUsed = (pBackend->nMemUsed >= nOld) ? (pBackend->nMemUsed - nOld) : 0;` |
|    1873763 |  706 | `	pBackend->nMemUsed += nByte;` |
|    1873763 |  707 | `	if( pBackend->nMemUsed > pBackend->nMemPeak ){` |
|     756445 |  708 | `		pBackend->nMemPeak = pBackend->nMemUsed;` |
|     377553 |  709 | `	}` |
|    1873763 |  710 | `	pNew->nSize = nByte;` |
|    1873763 |  711 | `	return (void *)&pNew[1];` |
|          - |  712 | `	}` |
|    6450302 |  713 | `}` |
|   12904473 |  714 | `PH7_PRIVATE void * SyMemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|          5 |  715 | `{` |
|          - |  716 | `	void *pChunk;` |
|          - |  717 | `#if defined(UNTRUST)` |
|          - |  718 | `	if( SXMEM_BACKEND_CORRUPT(pBackend)  ){` |
|          - |  719 | `		return 0;` |
|          - |  720 | `	}` |
|          - |  721 | `#endif` |
|   12904478 |  722 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  723 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  724 | `	}` |
|   12904478 |  725 | `	pChunk = MemBackendRealloc(&(*pBackend),pOld,nByte);` |
|    6450297 |  726 | `	if( pChunk ){` |
|          - |  727 | `		/* The old address is gone whether or not realloc moved the block. */` |
|          - |  728 | `		PHL_CENSUS_FORGET(pOld);` |
|          - |  729 | `		PHL_CENSUS_DIRECT(pBackend,pChunk,nByte);` |
|    6450297 |  730 | `	}` |
|   12904478 |  731 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  732 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  733 | `	}` |
|   12904478 |  734 | `	return pChunk;` |
|          5 |  735 | `}` |
|   28257990 |  736 | `static sxi32 MemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|          5 |  737 | `{` |
|          - |  738 | `	SyMemBlock *pBlock;` |
|   28257995 |  739 | `	pBlock = (SyMemBlock *)(((char *)pChunk) - sizeof(SyMemBlock));` |
|          - |  740 | `#if defined(UNTRUST)` |
|          - |  741 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|          - |  742 | `		return SXERR_CORRUPT;` |
|          - |  743 | `	}` |
|          - |  744 | `#endif` |
|          - |  745 | `	/* Unlink from the list of active blocks */` |
|   28257995 |  746 | `	if( pBackend->nBlock > 0 ){` |
|          - |  747 | `		/* Release the block */` |
|          - |  748 | `#if defined(UNTRUST)` |
|          - |  749 | `		/* Mark as stale block */` |
|          - |  750 | `		pBlock->nGuard = 0x635B;` |
|          - |  751 | `#endif` |
|   28257995 |  752 | `		MACRO_LD_REMOVE(pBackend->pBlocks,pBlock);` |
|   28257995 |  753 | `		pBackend->nBlock--;` |
|   42387249 |  754 | `		pBackend->nMemUsed = (pBackend->nMemUsed >= pBlock->nSize)` |
|   28257990 |  755 | `			? (pBackend->nMemUsed - pBlock->nSize) : 0;` |
|   28257995 |  756 | `		pBackend->pMethods->xFree(pBlock);` |
|   14128736 |  757 | `	}` |
|   28257995 |  758 | `	return SXRET_OK;` |
|          5 |  759 | `}` |
|   28257990 |  760 | `PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|          5 |  761 | `{` |
|          - |  762 | `	sxi32 rc;` |
|          - |  763 | `#if defined(UNTRUST)` |
|          - |  764 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  765 | `		return SXERR_CORRUPT;` |
|          - |  766 | `	}` |
|          - |  767 | `#endif` |
|   28257995 |  768 | `	if( pChunk == 0 ){` |
|        ! 0 |  769 | `		return SXRET_OK;` |
|          - |  770 | `	}` |
|   28257995 |  771 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  772 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  773 | `	}` |
|          - |  774 | `	PHL_CENSUS_FORGET(pChunk);` |
|   28257995 |  775 | `	rc = MemBackendFree(&(*pBackend),pChunk);` |
|   28257995 |  776 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  777 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  778 | `	}` |
|   28257995 |  779 | `	return rc;` |
|   14128741 |  780 | `}` |
|          - |  781 | `#if defined(PH7_ENABLE_THREADS)` |
|       8455 |  782 | `PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods)` |
|          5 |  783 | `{` |
|          - |  784 | `	SyMutex *pMutex;` |
|          - |  785 | `#if defined(UNTRUST)` |
|          - |  786 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pMethods == 0 \|\| pMethods->xNew == 0){` |
|          - |  787 | `		return SXERR_CORRUPT;` |
|          - |  788 | `	}` |
|          - |  789 | `#endif` |
|       8460 |  790 | `	pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|       8460 |  791 | `	if( pMutex == 0 ){` |
|        ! 0 |  792 | `		return SXERR_OS;` |
|          - |  793 | `	}` |
|          - |  794 | `	/* Attach the mutex to the memory backend */` |
|       8460 |  795 | `	pBackend->pMutex = pMutex;` |
|       8460 |  796 | `	pBackend->pMutexMethods = pMethods;` |
|       8460 |  797 | `	return SXRET_OK;` |
|       4227 |  798 | `}` |
|       8455 |  799 | `PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend)` |
|          5 |  800 | `{` |
|          - |  801 | `#if defined(UNTRUST)` |
|          - |  802 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  803 | `		return SXERR_CORRUPT;` |
|          - |  804 | `	}` |
|          - |  805 | `#endif` |
|       8460 |  806 | `	if( pBackend->pMutex == 0 ){` |
|          - |  807 | `		/* There is no mutex subsystem at all */` |
|        ! 0 |  808 | `		return SXRET_OK;` |
|          - |  809 | `	}` |
|       8460 |  810 | `	SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|       8460 |  811 | `	pBackend->pMutexMethods = 0;` |
|       8460 |  812 | `	pBackend->pMutex = 0;` |
|       8460 |  813 | `	return SXRET_OK;` |
|       4227 |  814 | `}` |
|          - |  815 | `#endif` |
|          - |  816 | `/*` |
|          - |  817 | ` * Memory pool allocator` |
|          - |  818 | ` */` |
|          - |  819 | `#define SXMEM_POOL_MAGIC		0xDEAD` |
|          - |  820 | `#define SXMEM_POOL_MAXALLOC		(1<<(SXMEM_POOL_NBUCKETS+SXMEM_POOL_INCR))` |
|          - |  821 | `#define SXMEM_POOL_MINALLOC		(1<<(SXMEM_POOL_INCR))` |
|          - |  822 | `/* When SXMEM_POOL_BYPASS is defined (sanitizer builds) the bucket-recycling` |
|          - |  823 | ` * path is compiled but never taken — MemBackendPoolAlloc forces the big-block` |
|          - |  824 | ` * branch — so ASan sees one real allocation per request. A compile-time` |
|          - |  825 | ` * constant (not #ifdef scattered through the alloc body) keeps a single copy` |
|          - |  826 | ` * of the alloc/tag/free logic; production builds fold the constant to 0 and` |
|          - |  827 | ` * lose nothing. */` |
|          - |  828 | `#if defined(SXMEM_POOL_BYPASS)` |
|          - |  829 | `# define SXMEM_POOL_BYPASS_ACTIVE 1` |
|          - |  830 | `#else` |
|          - |  831 | `# define SXMEM_POOL_BYPASS_ACTIVE 0` |
|          - |  832 | `#endif` |
|     825858 |  833 | `static sxi32 MemPoolBucketAlloc(SyMemBackend *pBackend,sxu32 nBucket)` |
|          5 |  834 | `{` |
|          - |  835 | `	char *zBucket,*zBucketEnd;` |
|          - |  836 | `	SyMemHeader *pHeader;` |
|          - |  837 | `	sxu32 nBucketSize;` |
|          - |  838 |  |
|          - |  839 | `	/* Allocate one big block first */` |
|     825863 |  840 | `	zBucket = (char *)MemBackendAlloc(&(*pBackend),SXMEM_POOL_MAXALLOC);` |
|     825863 |  841 | `	if( zBucket == 0 ){` |
|        ! 0 |  842 | `		return SXERR_MEM;` |
|          - |  843 | `	}` |
|     825863 |  844 | `	zBucketEnd = &zBucket[SXMEM_POOL_MAXALLOC];` |
|          - |  845 | `	/* Divide the big block into mini bucket pool */` |
|     825863 |  846 | `	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);` |
|     825863 |  847 | `	pBackend->apPool[nBucket] = pHeader = (SyMemHeader *)zBucket;` |
|   90234994 |  848 | `	for(;;){` |
|  181585727 |  849 | `		if( &zBucket[nBucketSize] >= zBucketEnd ){` |
|     825863 |  850 | `			break;` |
|          - |  851 | `		}` |
|  180759869 |  852 | `		pHeader->pNext = (SyMemHeader *)&zBucket[nBucketSize];` |
|          - |  853 | `		/* Advance the cursor to the next available chunk */` |
|  180759869 |  854 | `		pHeader = pHeader->pNext;` |
|  180759869 |  855 | `		zBucket += nBucketSize;` |
|          5 |  856 | `	}` |
|     825863 |  857 | `	pHeader->pNext = 0;` |
|          - |  858 |  |
|     825863 |  859 | `	return SXRET_OK;` |
|     410700 |  860 | `}` |
|  322652272 |  861 | `static void * MemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|          5 |  862 | `{` |
|          - |  863 | `	SyMemHeader *pBucket,*pNext;` |
|          - |  864 | `	sxu32 nBucketSize;` |
|          - |  865 | `	sxu32 nBucket;` |
|          - |  866 |  |
|          - |  867 | `	/* SXMEM_POOL_BYPASS (sanitizer builds): force the big-block path for every` |
|          - |  868 | `	 * request so there is no bucket recycling and ASan tracks each object's` |
|          - |  869 | `	 * real lifetime. Chunks are freed through MemBackendPoolFree's big-block` |
|          - |  870 | `	 * branch either way — one copy of the alloc+tag logic. */` |
|  322652277 |  871 | `	if( SXMEM_POOL_BYPASS_ACTIVE \|\| nByte + sizeof(SyMemHeader) >= SXMEM_POOL_MAXALLOC ){` |
|          - |  872 | `		/* Allocate a big chunk directly */` |
|        ! 0 |  873 | `		pBucket = (SyMemHeader *)MemBackendAlloc(&(*pBackend),nByte+sizeof(SyMemHeader));` |
|        ! 0 |  874 | `		if( pBucket == 0 ){` |
|        ! 0 |  875 | `			return 0;` |
|          - |  876 | `		}` |
|          - |  877 | `		/* Record as big block */` |
|        ! 0 |  878 | `		pBucket->nBucket = ((sxu32)SXMEM_POOL_MAGIC << 16) \| SXU16_HIGH;` |
|        ! 0 |  879 | `		return (void *)(pBucket+1);` |
|          - |  880 | `	}` |
|          - |  881 | `	/* Locate the appropriate bucket */` |
|  322652277 |  882 | `	nBucket = 0;` |
|  322652277 |  883 | `	nBucketSize = SXMEM_POOL_MINALLOC;` |
| 1402862094 |  884 | `	while( nByte + sizeof(SyMemHeader) > nBucketSize  ){` |
| 1080209822 |  885 | `		nBucketSize <<= 1;` |
| 1080209822 |  886 | `		nBucket++;` |
|          5 |  887 | `	}` |
|  322652277 |  888 | `	pBucket = pBackend->apPool[nBucket];` |
|  322652277 |  889 | `	if( pBucket == 0 ){` |
|          - |  890 | `		sxi32 rc;` |
|     825863 |  891 | `		rc = MemPoolBucketAlloc(&(*pBackend),nBucket);` |
|     825863 |  892 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  893 | `			return 0;` |
|          - |  894 | `		}` |
|     825863 |  895 | `		pBucket = pBackend->apPool[nBucket];` |
|     410695 |  896 | `	}` |
|          - |  897 | `	/* Remove from the free list */` |
|  322652277 |  898 | `	pNext = pBucket->pNext;` |
|  322652277 |  899 | `	pBackend->apPool[nBucket] = pNext;` |
|          - |  900 | `	/* Record bucket&magic number */` |
|  322652277 |  901 | `	pBucket->nBucket = (((sxu32)SXMEM_POOL_MAGIC << 16) \| nBucket);` |
|  322652277 |  902 | `	return (void *)&pBucket[1];` |
|  160772809 |  903 | `}` |
|  322652272 |  904 | `PH7_PRIVATE void * SyMemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|          5 |  905 | `{` |
|          - |  906 | `	void *pChunk;` |
|          - |  907 | `#if defined(UNTRUST)` |
|          - |  908 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  909 | `		return 0;` |
|          - |  910 | `	}` |
|          - |  911 | `#endif` |
|  322652277 |  912 | `	if( pBackend->pMutexMethods ){` |
|       8460 |  913 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       4222 |  914 | `	}` |
|  322652277 |  915 | `	pChunk = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|  160772804 |  916 | `	if( pChunk ){` |
|          - |  917 | `		PHL_CENSUS_POOL(pBackend,pChunk,nByte);` |
|  160772804 |  918 | `	}` |
|  322652277 |  919 | `	if( pBackend->pMutexMethods ){` |
|       8460 |  920 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       4222 |  921 | `	}` |
|  322652277 |  922 | `	return pChunk;` |
|          5 |  923 | `}` |
|  165110358 |  924 | `static sxi32 MemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|          5 |  925 | `{` |
|          - |  926 | `	SyMemHeader *pHeader;` |
|          - |  927 | `	sxu32 nBucket;` |
|          - |  928 | `	/* Get the corresponding bucket */` |
|  165110363 |  929 | `	pHeader = (SyMemHeader *)(((char *)pChunk) - sizeof(SyMemHeader));` |
|          - |  930 | `	/* Sanity check to avoid misuse */` |
|  165110363 |  931 | `	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){` |
|        ! 0 |  932 | `		return SXERR_CORRUPT;` |
|          - |  933 | `	}` |
|  165110363 |  934 | `	nBucket = pHeader->nBucket & 0xFFFF;` |
|  165110363 |  935 | `	if( nBucket == SXU16_HIGH ){` |
|          - |  936 | `		/* Free the big block */` |
|        ! 0 |  937 | `		MemBackendFree(&(*pBackend),pHeader);` |
|  165110363 |  938 | `	}else if( nBucket >= SXMEM_POOL_NBUCKETS + SXMEM_POOL_INCR ){` |
|          - |  939 | `		/* Corrupted or misused bucket index */` |
|        ! 0 |  940 | `		return SXERR_CORRUPT;` |
|        ! 0 |  941 | `	}else{` |
|          - |  942 | `		/* Return to the free list */` |
|  165110363 |  943 | `		pHeader->pNext = pBackend->apPool[nBucket];` |
|  165110363 |  944 | `		pBackend->apPool[nBucket] = pHeader;` |
|          - |  945 | `	}` |
|  165110363 |  946 | `	return SXRET_OK;` |
|   82528794 |  947 | `}` |
|  165110358 |  948 | `PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|          5 |  949 | `{` |
|          - |  950 | `	sxi32 rc;` |
|          - |  951 | `#if defined(UNTRUST)` |
|          - |  952 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pChunk == 0 ){` |
|          - |  953 | `		return SXERR_CORRUPT;` |
|          - |  954 | `	}` |
|          - |  955 | `#endif` |
|  165110363 |  956 | `	if( pBackend->pMutexMethods ){` |
|       8482 |  957 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       4233 |  958 | `	}` |
|          - |  959 | `	PHL_CENSUS_FORGET(pChunk);` |
|  165110363 |  960 | `	rc = MemBackendPoolFree(&(*pBackend),pChunk);` |
|  165110363 |  961 | `	if( pBackend->pMutexMethods ){` |
|       8482 |  962 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       4233 |  963 | `	}` |
|  165110363 |  964 | `	return rc;` |
|          5 |  965 | `}` |
|          - |  966 | `#if 0` |
|          - |  967 | `static void * MemBackendPoolRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|          - |  968 | `{` |
|          - |  969 | `	sxu32 nBucket,nBucketSize;` |
|          - |  970 | `	SyMemHeader *pHeader;` |
|          - |  971 | `	void * pNew;` |
|          - |  972 |  |
|          - |  973 | `	if( pOld == 0 ){` |
|          - |  974 | `		/* Allocate a new pool */` |
|          - |  975 | `		pNew = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|          - |  976 | `		return pNew;` |
|          - |  977 | `	}` |
|          - |  978 | `	/* Get the corresponding bucket */` |
|          - |  979 | `	pHeader = (SyMemHeader *)(((char *)pOld) - sizeof(SyMemHeader));` |
|          - |  980 | `	/* Sanity check to avoid misuse */` |
|          - |  981 | `	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){` |
|          - |  982 | `		return 0;` |
|          - |  983 | `	}` |
|          - |  984 | `	nBucket = pHeader->nBucket & 0xFFFF;` |
|          - |  985 | `	if( nBucket == SXU16_HIGH ){` |
|          - |  986 | `		/* Big block */` |
|          - |  987 | `		return MemBackendRealloc(&(*pBackend),pHeader,nByte);` |
|          - |  988 | `	}` |
|          - |  989 | `	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);` |
|          - |  990 | `	if( nBucketSize >= nByte + sizeof(SyMemHeader) ){` |
|          - |  991 | `		/* The old bucket can honor the requested size */` |
|          - |  992 | `		return pOld;` |
|          - |  993 | `	}` |
|          - |  994 | `	/* Allocate a new pool */` |
|          - |  995 | `	pNew = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|          - |  996 | `	if( pNew == 0 ){` |
|          - |  997 | `		return 0;` |
|          - |  998 | `	}` |
|          - |  999 | `	/* Copy the old data into the new block */` |
|          - | 1000 | `	SyMemcpy(pOld,pNew,nBucketSize);` |
|          - | 1001 | `	/* Free the stale block */` |
|          - | 1002 | `	MemBackendPoolFree(&(*pBackend),pOld);` |
|          - | 1003 | `	return pNew;` |
|          - | 1004 | `}` |
|          - | 1005 | `PH7_PRIVATE void * SyMemBackendPoolRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|          - | 1006 | `{` |
|          - | 1007 | `	void *pChunk;` |
|          - | 1008 | `#if defined(UNTRUST)` |
|          - | 1009 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - | 1010 | `		return 0;` |
|          - | 1011 | `	}` |
|          - | 1012 | `#endif` |
|          - | 1013 | `	if( pBackend->pMutexMethods ){` |
|          - | 1014 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|          - | 1015 | `	}` |
|          - | 1016 | `	pChunk = MemBackendPoolRealloc(&(*pBackend),pOld,nByte);` |
|          - | 1017 | `	if( pBackend->pMutexMethods ){` |
|          - | 1018 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|          - | 1019 | `	}` |
|          - | 1020 | `	return pChunk;` |
|          - | 1021 | `}` |
|          - | 1022 | `#endif` |
|       8455 | 1023 | `PH7_PRIVATE sxi32 SyMemBackendInit(SyMemBackend *pBackend,ProcMemError xMemErr,void * pUserData)` |
|          5 | 1024 | `{` |
|          - | 1025 | `#if defined(UNTRUST)` |
|          - | 1026 | `	if( pBackend == 0 ){` |
|          - | 1027 | `		return SXERR_EMPTY;` |
|          - | 1028 | `	}` |
|          - | 1029 | `#endif` |
|          - | 1030 | `	/* Zero the allocator first */` |
|       8460 | 1031 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|       8460 | 1032 | `	pBackend->xMemError = xMemErr;` |
|       8460 | 1033 | `	pBackend->pUserData = pUserData;` |
|          - | 1034 | `	/* Switch to the OS memory allocator */` |
|       8460 | 1035 | `	pBackend->pMethods = &sOSAllocMethods;` |
|       8460 | 1036 | `	if( pBackend->pMethods->xInit ){` |
|          - | 1037 | `		/* Initialize the backend  */` |
|        ! 0 | 1038 | `		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){` |
|        ! 0 | 1039 | `			return SXERR_ABORT;` |
|          - | 1040 | `		}` |
|        ! 0 | 1041 | `	}` |
|          - | 1042 | `#if defined(UNTRUST)` |
|          - | 1043 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|          - | 1044 | `#endif` |
|       8460 | 1045 | `	return SXRET_OK;` |
|       4227 | 1046 | `}` |
|        ! 0 | 1047 | `PH7_PRIVATE sxi32 SyMemBackendInitFromOthers(SyMemBackend *pBackend,const SyMemMethods *pMethods,ProcMemError xMemErr,void * pUserData)` |
|        ! 0 | 1048 | `{` |
|          - | 1049 | `#if defined(UNTRUST)` |
|          - | 1050 | `	if( pBackend == 0 \|\| pMethods == 0){` |
|          - | 1051 | `		return SXERR_EMPTY;` |
|          - | 1052 | `	}` |
|          - | 1053 | `#endif` |
|        ! 0 | 1054 | `	if( pMethods->xAlloc == 0 \|\| pMethods->xRealloc == 0 \|\| pMethods->xFree == 0 ){` |
|          - | 1055 | `		/* mandatory methods are missing. xChunkSize is NOT one of them: sx.h has` |
|          - | 1056 | `		 * always documented it as optional and no call site in the library asks` |
|          - | 1057 | `		 * for it, but this check demanded it anyway -- so an embedder supplying` |
|          - | 1058 | `		 * the three methods that are actually used was refused. */` |
|        ! 0 | 1059 | `		return SXERR_INVALID;` |
|          - | 1060 | `	}` |
|          - | 1061 | `	/* Zero the allocator first */` |
|        ! 0 | 1062 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|        ! 0 | 1063 | `	pBackend->xMemError = xMemErr;` |
|        ! 0 | 1064 | `	pBackend->pUserData = pUserData;` |
|          - | 1065 | `	/* Switch to the host application memory allocator */` |
|        ! 0 | 1066 | `	pBackend->pMethods = pMethods;` |
|        ! 0 | 1067 | `	if( pBackend->pMethods->xInit ){` |
|          - | 1068 | `		/* Initialize the backend  */` |
|        ! 0 | 1069 | `		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){` |
|        ! 0 | 1070 | `			return SXERR_ABORT;` |
|          - | 1071 | `		}` |
|        ! 0 | 1072 | `	}` |
|          - | 1073 | `#if defined(UNTRUST)` |
|          - | 1074 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|          - | 1075 | `#endif` |
|        ! 0 | 1076 | `	return SXRET_OK;` |
|        ! 0 | 1077 | `}` |
|      16900 | 1078 | `PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend *pBackend,SyMemBackend *pParent)` |
|          5 | 1079 | `{` |
|          - | 1080 | `	sxu8 bInheritMutex;` |
|          - | 1081 | `#if defined(UNTRUST)` |
|          - | 1082 | `	if( pBackend == 0 \|\| SXMEM_BACKEND_CORRUPT(pParent) ){` |
|          - | 1083 | `		return SXERR_CORRUPT;` |
|          - | 1084 | `	}` |
|          - | 1085 | `#endif` |
|          - | 1086 | `	/* Zero the allocator first */` |
|      16905 | 1087 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|      16905 | 1088 | `	pBackend->pMethods  = pParent->pMethods;` |
|      16905 | 1089 | `	pBackend->xMemError = pParent->xMemError;` |
|      16905 | 1090 | `	pBackend->pUserData = pParent->pUserData;` |
|      16905 | 1091 | `	pBackend->nMaxRequest = pParent->nMaxRequest;` |
|      16905 | 1092 | `	bInheritMutex = pParent->pMutexMethods ? TRUE : FALSE;` |
|      16905 | 1093 | `	if( bInheritMutex ){` |
|       8460 | 1094 | `		pBackend->pMutexMethods = pParent->pMutexMethods;` |
|          - | 1095 | `		/* Create a private mutex */` |
|       8460 | 1096 | `		pBackend->pMutex = pBackend->pMutexMethods->xNew(SXMUTEX_TYPE_FAST);` |
|       8460 | 1097 | `		if( pBackend->pMutex ==  0){` |
|        ! 0 | 1098 | `			return SXERR_OS;` |
|          - | 1099 | `		}` |
|       4222 | 1100 | `	}` |
|          - | 1101 | `#if defined(UNTRUST)` |
|          - | 1102 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|          - | 1103 | `#endif` |
|      16905 | 1104 | `	return SXRET_OK;` |
|       8444 | 1105 | `}` |
|      18184 | 1106 | `static sxi32 MemBackendRelease(SyMemBackend *pBackend)` |
|          5 | 1107 | `{` |
|          - | 1108 | `	SyMemBlock *pBlock,*pNext;` |
|          - | 1109 |  |
|      18189 | 1110 | `	pBlock = pBackend->pBlocks;` |
|   10951713 | 1111 | `	for(;;){` |
|   22039564 | 1112 | `		if( pBackend->nBlock == 0 ){` |
|       2086 | 1113 | `			break;` |
|          - | 1114 | `		}` |
|   22037482 | 1115 | `		pNext  = pBlock->pNext;` |
|   22037482 | 1116 | `		pBackend->pMethods->xFree(pBlock);` |
|   22037482 | 1117 | `		pBlock = pNext;` |
|   22037482 | 1118 | `		pBackend->nBlock--;` |
|          - | 1119 | `		/* LOOP ONE */` |
|   22037482 | 1120 | `		if( pBackend->nBlock == 0 ){` |
|      10606 | 1121 | `			break;` |
|          - | 1122 | `		}` |
|   22026881 | 1123 | `		pNext  = pBlock->pNext;` |
|   22026881 | 1124 | `		pBackend->pMethods->xFree(pBlock);` |
|   22026881 | 1125 | `		pBlock = pNext;` |
|   22026881 | 1126 | `		pBackend->nBlock--;` |
|          - | 1127 | `		/* LOOP TWO */` |
|   22026881 | 1128 | `		if( pBackend->nBlock == 0 ){` |
|       3825 | 1129 | `			break;` |
|          - | 1130 | `		}` |
|   22023060 | 1131 | `		pNext  = pBlock->pNext;` |
|   22023060 | 1132 | `		pBackend->pMethods->xFree(pBlock);` |
|   22023060 | 1133 | `		pBlock = pNext;` |
|   22023060 | 1134 | `		pBackend->nBlock--;` |
|          - | 1135 | `		/* LOOP THREE */` |
|   22023060 | 1136 | `		if( pBackend->nBlock == 0 ){` |
|       1684 | 1137 | `			break;` |
|          - | 1138 | `		}` |
|   22021380 | 1139 | `		pNext  = pBlock->pNext;` |
|   22021380 | 1140 | `		pBackend->pMethods->xFree(pBlock);` |
|   22021380 | 1141 | `		pBlock = pNext;` |
|   22021380 | 1142 | `		pBackend->nBlock--;` |
|          - | 1143 | `		/* LOOP FOUR */` |
|          5 | 1144 | `	}` |
|      18189 | 1145 | `	if( pBackend->pMethods->xRelease ){` |
|        ! 0 | 1146 | `		pBackend->pMethods->xRelease(pBackend->pMethods->pUserData);` |
|        ! 0 | 1147 | `	}` |
|      18189 | 1148 | `	pBackend->pMethods = 0;` |
|      18189 | 1149 | `	pBackend->pBlocks  = 0;` |
|          - | 1150 | `#if defined(UNTRUST)` |
|          - | 1151 | `	pBackend->nMagic = 0x2626;` |
|          - | 1152 | `#endif` |
|      18189 | 1153 | `	return SXRET_OK;` |
|          5 | 1154 | `}` |
|      18184 | 1155 | `PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend *pBackend)` |
|          5 | 1156 | `{` |
|          - | 1157 | `#if defined(UNTRUST)` |
|          - | 1158 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - | 1159 | `		return SXERR_INVALID;` |
|          - | 1160 | `	}` |
|          - | 1161 | `#endif` |
|      18189 | 1162 | `	if( pBackend->pMutexMethods ){` |
|       1250 | 1163 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|        623 | 1164 | `	}` |
|          - | 1165 | `	PHL_CENSUS_FORGET_BACKEND(pBackend);` |
|      18189 | 1166 | `	(void)MemBackendRelease(&(*pBackend));` |
|      18189 | 1167 | `	if( pBackend->pMutexMethods ){` |
|       1250 | 1168 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       1250 | 1169 | `		SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|        623 | 1170 | `	}` |
|      18189 | 1171 | `	return SXRET_OK;` |
|          5 | 1172 | `}` |
|      11911 | 1173 | `PH7_PRIVATE void * SyMemBackendDup(SyMemBackend *pBackend,const void *pSrc,sxu32 nSize)` |
|          5 | 1174 | `{` |
|          - | 1175 | `	void *pNew;` |
|          - | 1176 | `#if defined(UNTRUST)` |
|          - | 1177 | `	if( pSrc == 0 \|\| nSize <= 0 ){` |
|          - | 1178 | `		return 0;` |
|          - | 1179 | `	}` |
|          - | 1180 | `#endif` |
|      11916 | 1181 | `	pNew = SyMemBackendAlloc(&(*pBackend),nSize);` |
|      11916 | 1182 | `	if( pNew ){` |
|      11916 | 1183 | `		SyMemcpy(pSrc,pNew,nSize);` |
|       5903 | 1184 | `	}` |
|      11916 | 1185 | `	return pNew;` |
|          5 | 1186 | `}` |
|   48664556 | 1187 | `PH7_PRIVATE char * SyMemBackendStrDup(SyMemBackend *pBackend,const char *zSrc,sxu32 nSize)` |
|          5 | 1188 | `{` |
|          - | 1189 | `	char *zDest;` |
|   48664561 | 1190 | `	zDest = (char *)SyMemBackendAlloc(&(*pBackend),nSize + 1);` |
|   48664561 | 1191 | `	if( zDest == 0 ){` |
|        ! 0 | 1192 | `		return 0;` |
|          - | 1193 | `	}` |
|   48664561 | 1194 | `	if( nSize < 1 ){` |
|          - | 1195 | `		/* Systrcpy reads a zero length as "the source is NUL-terminated, measure it",` |
|          - | 1196 | `		 * which is the right convention for ITS callers and the wrong one here: a` |
|          - | 1197 | `		 * caller of this function passed a length, so it has already said the answer.` |
|          - | 1198 | `		 * The difference is not academic -- an EMPTY php string hands out a NULL data` |
|          - | 1199 | `		 * pointer (a SyBlob with no bytes has no buffer), so measuring it is a read of` |
|          - | 1200 | ``		 * address zero. `f($o->{''})` reached exactly that, through the deferred`` |
|          - | 1201 | `		 * argument path's copy of the property name, and SEGFAULTED where php warns` |
|          - | 1202 | `		 * about an undefined property and answers null. */` |
|       1994 | 1203 | `		zDest[0] = 0;` |
|       1994 | 1204 | `		return zDest;` |
|          - | 1205 | `	}` |
|   48662572 | 1206 | `	Systrcpy(zDest,nSize+1,zSrc,nSize);` |
|   48662572 | 1207 | `	return zDest;` |
|   24088371 | 1208 | `}` |
|   30464663 | 1209 | `PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize)` |
|          5 | 1210 | `{` |
|          - | 1211 | `#if defined(UNTRUST)` |
|          - | 1212 | `	if( pBlob == 0 \|\| pBuffer == 0 \|\| nSize < 1 ){` |
|          - | 1213 | `		return SXERR_EMPTY;` |
|          - | 1214 | `	}` |
|          - | 1215 | `#endif` |
|   30464668 | 1216 | `	pBlob->pBlob = pBuffer;` |
|   30464668 | 1217 | `	pBlob->mByte = nSize;` |
|   30464668 | 1218 | `	pBlob->nByte = 0;` |
|   30464668 | 1219 | `	pBlob->pAllocator = 0;` |
|   30464668 | 1220 | `	pBlob->nFlags = SXBLOB_LOCKED\|SXBLOB_STATIC;` |
|   30464668 | 1221 | `	return SXRET_OK;` |
|          5 | 1222 | `}` |
|  100699587 | 1223 | `PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator)` |
|          5 | 1224 | `{` |
|          - | 1225 | `#if defined(UNTRUST)` |
|          - | 1226 | `	if( pBlob == 0  ){` |
|          - | 1227 | `		return SXERR_EMPTY;` |
|          - | 1228 | `	}` |
|          - | 1229 | `#endif` |
|  100699592 | 1230 | `	pBlob->pBlob = 0;` |
|  100699592 | 1231 | `	pBlob->mByte = pBlob->nByte	= 0;` |
|  100699592 | 1232 | `	pBlob->pAllocator = &(*pAllocator);` |
|  100699592 | 1233 | `	pBlob->nFlags = 0;` |
|  100699592 | 1234 | `	return SXRET_OK;` |
|          5 | 1235 | `}` |
|   64158089 | 1236 | `PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte)` |
|          5 | 1237 | `{` |
|          - | 1238 | `#if defined(UNTRUST)` |
|          - | 1239 | `	if( pBlob == 0  ){` |
|          - | 1240 | `		return SXERR_EMPTY;` |
|          - | 1241 | `	}` |
|          - | 1242 | `#endif` |
|   64158094 | 1243 | `	pBlob->pBlob = (void *)pData;` |
|   64158094 | 1244 | `	pBlob->nByte = nByte;` |
|   64158094 | 1245 | `	pBlob->mByte = 0;` |
|          - | 1246 | `	/* POOLED describes the pointer being installed, and this one is somebody else's.` |
|          - | 1247 | `	 * (A caller that had an owned buffer here leaked it before this flag existed too --` |
|          - | 1248 | `	 * the contract has always been "release, then alias" -- but the flag must never` |
|          - | 1249 | `	 * outlive the buffer it described.) */` |
|   64158094 | 1250 | `	pBlob->nFlags = (pBlob->nFlags & ~SXBLOB_POOLED) \| SXBLOB_RDONLY;` |
|   64158094 | 1251 | `	return SXRET_OK;` |
|          5 | 1252 | `}` |
|          - | 1253 | `#ifndef SXBLOB_MIN_GROWTH` |
|          - | 1254 | `#define SXBLOB_MIN_GROWTH 16` |
|          - | 1255 | `#endif` |
|          - | 1256 | `/*` |
|          - | 1257 | ` * The largest buffer a blob takes from the POOL rather than from the tracked backend.` |
|          - | 1258 | ` *` |
|          - | 1259 | ` * A blob's backing store used to come from SyMemBackendRealloc without exception, which` |
|          - | 1260 | ` * meant a malloc() for every string body the engine ever built. Counted on the ecosystem` |
|          - | 1261 | ` * gate's phpcs step over 711 files: of 160,541,911 calls the engine makes into malloc,` |
|          - | 1262 | ` * 119,844,811 -- three quarters -- are a blob's FIRST growth arriving here with` |
|          - | 1263 | ` * pBlob->pBlob still 0, and 119,727,780 of those (99.9%) ask for 256 bytes or less.` |
|          - | 1264 | ` * 72,314,251 ask for 64 or less. The pool serves exactly that size in a free-list pop,` |
|          - | 1265 | ` * and was already serving 129 million requests a run of the same shape from hashmap` |
|          - | 1266 | ` * nodes, hash entries and class instances.` |
|          - | 1267 | ` *` |
|          - | 1268 | ` * Why a CAP and not the pool's own 32 KB ceiling: the pool never returns memory to the` |
|          - | 1269 | ` * OS (deliberately -- a feature for a long-running device), so one big transient blob` |
|          - | 1270 | ` * routed through it would hold a 32 KB bucket block for the life of the VM. A large` |
|          - | 1271 | ` * blob keeps the tracked backend, where a free is a free, and keeps realloc's ability` |
|          - | 1272 | ` * to extend in place -- which matters for the output buffer and for string building,` |
|          - | 1273 | ` * the two things that actually grow past this.` |
|          - | 1274 | ` *` |
|          - | 1275 | ` * Why 248 and not 256: 248 + sizeof(SyMemHeader) is exactly the 256-byte bucket, so a` |
|          - | 1276 | ` * pooled buffer can never overshoot into the 512 one. Measured, the value barely` |
|          - | 1277 | ` * matters -- 248, 256 and 1016 all land within noise of each other on peak RSS, because` |
|          - | 1278 | ` * the win is dominated by one band (189,788 live bodies asking 16-23 bytes). What the` |
|          - | 1279 | ` * cap really buys is a BOUND on what the pool can be made to retain.` |
|          - | 1280 | ` */` |
|          - | 1281 | `#ifndef SXBLOB_POOL_MAX` |
|          - | 1282 | `#define SXBLOB_POOL_MAX 248` |
|          - | 1283 | `#endif` |
|          - | 1284 | `/*` |
|          - | 1285 | ` * Give pBlob a buffer of exactly nByte, from whichever allocator that size belongs to,` |
|          - | 1286 | ` * moving it between the two when the size crosses SXBLOB_POOL_MAX.` |
|          - | 1287 | ` *` |
|          - | 1288 | ` * This is the ONLY place that decides which allocator owns a blob's bytes, and` |
|          - | 1289 | ` * SXBLOB_POOLED is set or cleared on every path out of it -- there is no path that` |
|          - | 1290 | ` * leaves the flag describing a buffer that is no longer there. A blob is never demoted` |
|          - | 1291 | ` * back INTO the pool once it has outgrown it: capacity only rises, so the tracked block` |
|          - | 1292 | ` * it already holds is the right home for everything after.` |
|          - | 1293 | ` */` |
|   88038882 | 1294 | `static sxi32 BlobSetCapacity(SyBlob *pBlob,sxu32 nByte)` |
|          5 | 1295 | `{` |
|   88038887 | 1296 | `	SyMemBackend *pAlloc = pBlob->pAllocator;` |
|   88038887 | 1297 | `	int bWantPool = (nByte <= SXBLOB_POOL_MAX);` |
|          - | 1298 | `	void *pNew;` |
|   88038887 | 1299 | `	if( pBlob->pBlob == 0 ){` |
|          - | 1300 | `		/* The first buffer -- three quarters of every direct allocation the engine` |
|          - | 1301 | `		 * used to make, and the reason this function exists. */` |
|   69900025 | 1302 | `		pNew = bWantPool ? SyMemBackendPoolAlloc(pAlloc,nByte)` |
|   35413720 | 1303 | `		                 : SyMemBackendAlloc(pAlloc,nByte);` |
|   70199048 | 1304 | `		if( pNew == 0 ){` |
|        ! 0 | 1305 | `			return SXERR_MEM;` |
|          5 | 1306 | `		}` |
|   52931838 | 1307 | `	}else if( (pBlob->nFlags & SXBLOB_POOLED) == 0 ){` |
|          - | 1308 | `		/* Already a tracked block: realloc, exactly as before this existed. */` |
|     332223 | 1309 | `		pNew = SyMemBackendRealloc(pAlloc,pBlob->pBlob,nByte);` |
|     332223 | 1310 | `		if( pNew == 0 ){` |
|        ! 0 | 1311 | `			return SXERR_MEM;` |
|          - | 1312 | `		}` |
|     167960 | 1313 | `	}else{` |
|          - | 1314 | `		/* A pooled buffer growing. The pool has no realloc, so this is` |
|          - | 1315 | `		 * allocate-copy-free -- which is what realloc does anyway for any growth it` |
|          - | 1316 | `		 * cannot satisfy in place. Only the USED bytes are carried over; the rest of` |
|          - | 1317 | `		 * the old chunk was never written. */` |
|   17389914 | 1318 | `		pNew = bWantPool ? SyMemBackendPoolAlloc(pAlloc,nByte)` |
|    8872962 | 1319 | `		                 : SyMemBackendAlloc(pAlloc,nByte);` |
|   17507626 | 1320 | `		if( pNew == 0 ){` |
|        ! 0 | 1321 | `			return SXERR_MEM;   /* the old buffer is still ours and still valid */` |
|          - | 1322 | `		}` |
|   17507626 | 1323 | `		if( pBlob->nByte > 0 ){` |
|   17301207 | 1324 | `			SX_MACRO_FAST_MEMCPY(pBlob->pBlob,pNew,pBlob->nByte);` |
|    8651076 | 1325 | `		}` |
|   17507626 | 1326 | `		SyMemBackendPoolFree(pAlloc,pBlob->pBlob);` |
|          - | 1327 | `	}` |
|   88038887 | 1328 | `	if( bWantPool ){` |
|   86863680 | 1329 | `		pBlob->nFlags \|= SXBLOB_POOLED;` |
|   43419987 | 1330 | `	}else{` |
|    1175212 | 1331 | `		pBlob->nFlags &= ~SXBLOB_POOLED;` |
|          - | 1332 | `	}` |
|   88038887 | 1333 | `	pBlob->pBlob = pNew;` |
|   88038887 | 1334 | `	pBlob->mByte = nByte;` |
|   88038887 | 1335 | `	return SXRET_OK;` |
|   44014196 | 1336 | `}` |
|  291681242 | 1337 | `static sxi32 BlobPrepareGrow(SyBlob *pBlob,sxu32 *pByte)` |
|          5 | 1338 | `{` |
|          - | 1339 | `	sxu32 nByte;` |
|  291681247 | 1340 | `	nByte = *pByte;` |
|  291681247 | 1341 | `	if( pBlob->nFlags & (SXBLOB_LOCKED\|SXBLOB_STATIC) ){` |
|  162858599 | 1342 | `		if ( SyBlobFreeSpace(pBlob) < nByte ){` |
|        ! 0 | 1343 | `			*pByte = SyBlobFreeSpace(pBlob);` |
|        ! 0 | 1344 | `			if( (*pByte) == 0 ){` |
|        ! 0 | 1345 | `				return SXERR_SHORT;` |
|          - | 1346 | `			}` |
|        ! 0 | 1347 | `		}` |
|  162858599 | 1348 | `		return SXRET_OK;` |
|          - | 1349 | `	}` |
|  128822653 | 1350 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|          - | 1351 | `		/* Make a copy of the read-only item. The second allocation door, and it takes` |
|          - | 1352 | `		 * the same routing as the first: the alias being copied is somebody else's` |
|          - | 1353 | `		 * short string far more often than not. Detached from pBlob first so` |
|          - | 1354 | `		 * BlobSetCapacity sees "no buffer" and cannot try to grow or free an alias it` |
|          - | 1355 | `		 * does not own. */` |
|   16774557 | 1356 | `		sxu32 nCopy = pBlob->nByte;` |
|   16774557 | 1357 | `		const void *pSrc = pBlob->pBlob;` |
|   16774557 | 1358 | `		pBlob->pBlob = 0;` |
|   16774557 | 1359 | `		pBlob->mByte = 0;` |
|   16774557 | 1360 | `		pBlob->nFlags &= ~(SXBLOB_RDONLY\|SXBLOB_POOLED);` |
|   16774557 | 1361 | `		if( nCopy > 0 ){` |
|   16774557 | 1362 | `			if( BlobSetCapacity(&(*pBlob),nCopy) != SXRET_OK ){` |
|          - | 1363 | `				/* Put the alias back: refusing to grow must not lose the bytes. */` |
|        ! 0 | 1364 | `				pBlob->pBlob = (void *)pSrc;` |
|        ! 0 | 1365 | `				pBlob->nFlags \|= SXBLOB_RDONLY;` |
|        ! 0 | 1366 | `				return SXERR_MEM;` |
|          - | 1367 | `			}` |
|   16774557 | 1368 | `			SX_MACRO_FAST_MEMCPY(pSrc,pBlob->pBlob,nCopy);` |
|    8388026 | 1369 | `		}` |
|    8388026 | 1370 | `	}` |
|  128822653 | 1371 | `	if( SyBlobFreeSpace(pBlob) >= nByte ){` |
|   57558323 | 1372 | `		return SXRET_OK;` |
|          - | 1373 | `	}` |
|   71264335 | 1374 | `	if( pBlob->mByte > 0 ){` |
|   17839844 | 1375 | `		nByte = nByte + pBlob->mByte * 2 + SXBLOB_MIN_GROWTH;` |
|   62346693 | 1376 | `	}else if ( nByte < SXBLOB_MIN_GROWTH ){` |
|   42400762 | 1377 | `		nByte = SXBLOB_MIN_GROWTH;` |
|   21193057 | 1378 | `	}` |
|   71264335 | 1379 | `	return BlobSetCapacity(&(*pBlob),nByte);` |
|  145725750 | 1380 | `}` |
|  295314952 | 1381 | `PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize)` |
|          5 | 1382 | `{` |
|          - | 1383 | `	sxu8 *zBlob;` |
|          - | 1384 | `	sxi32 rc;` |
|  295314957 | 1385 | `	if( nSize < 1 ){` |
|    4691392 | 1386 | `		return SXRET_OK;` |
|          - | 1387 | `	}` |
|  290623570 | 1388 | `	rc = BlobPrepareGrow(&(*pBlob),&nSize);` |
|  290623570 | 1389 | `	if( SXRET_OK != rc ){` |
|        ! 0 | 1390 | `		return rc;` |
|          - | 1391 | `	}` |
|  290623570 | 1392 | `	if( pData ){` |
|  290617508 | 1393 | `		zBlob = (sxu8 *)pBlob->pBlob ;` |
|  290617508 | 1394 | `		zBlob = &zBlob[pBlob->nByte];` |
|  290617508 | 1395 | `		pBlob->nByte += nSize;` |
|  290617508 | 1396 | `		SX_MACRO_FAST_MEMCPY(pData,zBlob,nSize);` |
|  145193784 | 1397 | `	}` |
|  290623570 | 1398 | `	return SXRET_OK;` |
|  147542270 | 1399 | `}` |
|   17201846 | 1400 | `PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob)` |
|          5 | 1401 | `{` |
|          - | 1402 | `	sxi32 rc;` |
|          - | 1403 | `	sxu32 n;` |
|   17201851 | 1404 | `	n = pBlob->nByte;` |
|   17201851 | 1405 | `	rc = SyBlobAppend(&(*pBlob),(const void *)"\0",sizeof(char));` |
|   17201851 | 1406 | `	if (rc == SXRET_OK ){` |
|   17201851 | 1407 | `		pBlob->nByte = n;` |
|    8601733 | 1408 | `	}` |
|   17201851 | 1409 | `	return rc;` |
|          5 | 1410 | `}` |
|          - | 1411 | `/*` |
|          - | 1412 | ` * Give a blob its own copy of the bytes it is only borrowing.` |
|          - | 1413 | ` *` |
|          - | 1414 | ` * SyBlobReadOnly leaves a blob pointing INTO somebody else's buffer, which is how a` |
|          - | 1415 | ` * value copy avoids an allocation. That view is only good while the owner does not` |
|          - | 1416 | ` * move or overwrite those bytes, so any holder that must survive a write to the owner` |
|          - | 1417 | ` * calls this first. A blob that already owns its bytes is left alone, so this is` |
|          - | 1418 | ` * cheap to ask.` |
|          - | 1419 | ` */` |
|    1798888 | 1420 | `PH7_PRIVATE sxi32 SyBlobMakePrivate(SyBlob *pBlob)` |
|          5 | 1421 | `{` |
|    1798893 | 1422 | `	sxu32 nByte = 0;` |
|          - | 1423 | `#ifdef UNTRUST` |
|          - | 1424 | `	if( pBlob == 0 ){` |
|          - | 1425 | `		return SXERR_EMPTY;` |
|          - | 1426 | `	}` |
|          - | 1427 | `#endif` |
|    1798893 | 1428 | `	if( (pBlob->nFlags & SXBLOB_RDONLY) == 0 ){` |
|     741216 | 1429 | `		return SXRET_OK;` |
|          - | 1430 | `	}` |
|          - | 1431 | `	/* BlobPrepareGrow's read-only arm IS the copy: asking it for zero extra bytes` |
|          - | 1432 | `	 * detaches the alias and nothing more. */` |
|    1057682 | 1433 | `	return BlobPrepareGrow(&(*pBlob),&nByte);` |
|     899938 | 1434 | `}` |
|   33023169 | 1435 | `PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest)` |
|          5 | 1436 | `{` |
|   33023174 | 1437 | `	sxi32 rc = SXRET_OK;` |
|          - | 1438 | `#ifdef UNTRUST` |
|          - | 1439 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|          - | 1440 | `		return SXERR_EMPTY;` |
|          - | 1441 | `	}` |
|          - | 1442 | `#endif` |
|   33023174 | 1443 | `	if( pSrc->nByte > 0 ){` |
|   33023104 | 1444 | `		rc = SyBlobAppend(&(*pDest),pSrc->pBlob,pSrc->nByte);` |
|   16507529 | 1445 | `	}` |
|   33023174 | 1446 | `	return rc;` |
|          5 | 1447 | `}` |
|         62 | 1448 | `PH7_PRIVATE sxi32 SyBlobCmp(SyBlob *pLeft,SyBlob *pRight)` |
|          3 | 1449 | `{` |
|          - | 1450 | `	sxi32 rc;` |
|          - | 1451 | `#ifdef UNTRUST` |
|          - | 1452 | `	if( pLeft == 0 \|\| pRight == 0 ){` |
|          - | 1453 | `		return pLeft ? 1 : -1;` |
|          - | 1454 | `	}` |
|          - | 1455 | `#endif` |
|         65 | 1456 | `	if( pLeft->nByte != pRight->nByte ){` |
|          - | 1457 | `		/* Length differ */` |
|         13 | 1458 | `		return pLeft->nByte - pRight->nByte;` |
|          - | 1459 | `	}` |
|         54 | 1460 | `	if( pLeft->nByte == 0 ){` |
|        ! 0 | 1461 | `		return 0;` |
|          - | 1462 | `	}` |
|          - | 1463 | `	/* Perform a standard memcmp() operation */` |
|         54 | 1464 | `	rc = SyMemcmp(pLeft->pBlob,pRight->pBlob,pLeft->nByte);` |
|         54 | 1465 | `	return rc;` |
|          8 | 1466 | `}` |
|   47784968 | 1467 | `PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob)` |
|          5 | 1468 | `{` |
|   47784973 | 1469 | `	pBlob->nByte = 0;` |
|   47784973 | 1470 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|      10836 | 1471 | `		pBlob->pBlob = 0;` |
|      10836 | 1472 | `		pBlob->mByte = 0;` |
|      10836 | 1473 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|       5397 | 1474 | `	}` |
|   47784973 | 1475 | `	return SXRET_OK;` |
|          5 | 1476 | `}` |
|  306050816 | 1477 | `PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob)` |
|          5 | 1478 | `{` |
|  306050821 | 1479 | `	if( (pBlob->nFlags & (SXBLOB_STATIC\|SXBLOB_RDONLY)) == 0 && pBlob->mByte > 0 ){` |
|          - | 1480 | `		/* Whichever door BlobSetCapacity took. Getting this wrong is not a leak, it is` |
|          - | 1481 | `		 * a wrong-allocator free -- and under SXMEM_POOL_BYPASS (the sanitizer build)` |
|          - | 1482 | `		 * a pool chunk is a real block handed out eight bytes in, so ASan says so` |
|          - | 1483 | `		 * immediately rather than the heap going quietly wrong. */` |
|   68387179 | 1484 | `		if( pBlob->nFlags & SXBLOB_POOLED ){` |
|   67545722 | 1485 | `			SyMemBackendPoolFree(pBlob->pAllocator,pBlob->pBlob);` |
|   33762464 | 1486 | `		}else{` |
|     841462 | 1487 | `			SyMemBackendFree(pBlob->pAllocator,pBlob->pBlob);` |
|          - | 1488 | `		}` |
|   34187857 | 1489 | `	}` |
|  306050821 | 1490 | `	pBlob->pBlob = 0;` |
|  306050821 | 1491 | `	pBlob->nByte = pBlob->mByte = 0;` |
|  306050821 | 1492 | `	pBlob->nFlags = 0;` |
|  306050821 | 1493 | `	return SXRET_OK;` |
|          5 | 1494 | `}` |
|          - | 1495 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    6243702 | 1496 | `PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft)` |
|          5 | 1497 | `{` |
|    6243707 | 1498 | `	const char *zIn = (const char *)pBlob;` |
|          - | 1499 | `	const char *zEnd;` |
|          - | 1500 | `	sxi32 rc;` |
|    6243707 | 1501 | `	if( pLen > nLen ){` |
|      14453 | 1502 | `		return SXERR_NOTFOUND;` |
|          - | 1503 | `	}` |
|    6229259 | 1504 | `	zEnd = &zIn[nLen-pLen];` |
|   12473247 | 1505 | `	for(;;){` |
|   24967233 | 1506 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   22250680 | 1507 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   19484818 | 1508 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   18868893 | 1509 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|          5 | 1510 | `	}` |
|     471759 | 1511 | `	return SXERR_NOTFOUND;` |
|    3121531 | 1512 | `}` |
|          - | 1513 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|          - | 1514 |  |
