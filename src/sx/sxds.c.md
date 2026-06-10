# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 272/287 lines (94.77%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits | Line | Source |
| --------: | ---: | :--- |
|         - |    1 | `/**` |
|         - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|         - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |    5 | ` */` |
|         - |    6 | `#include "sxtypes.h"` |
|         - |    7 | `#include "sxmacros.h"` |
|         - |    8 | `#include "sxset.h"` |
|         - |    9 | `#include "sxmem.h"` |
|         - |   10 | `#include "sxhashtable.h"` |
|         - |   11 | `#include "sxhash.h"` |
|         - |   12 | `#include "sxstr.h"` |
|         - |   13 |  |
|  17271804 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  17271806 |   16 | `	pSet->nSize = 0 ;` |
|  17271806 |   17 | `	pSet->nUsed = 0;` |
|  17271806 |   18 | `	pSet->nCursor = 0;` |
|  17271806 |   19 | `	pSet->eSize = ElemSize;` |
|  17271806 |   20 | `	pSet->pAllocator = pAllocator;` |
|  17271806 |   21 | `	pSet->pBase =  0;` |
|  17271806 |   22 | `	pSet->pUserData = 0;` |
|  17271806 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  28412486 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  28412488 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4239156 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4239156 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4099238 |   34 | `			pSet->nSize = 4;` |
|   2049618 |   35 | `		}` |
|   4239156 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4239156 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4239156 |   40 | `		pSet->pBase = pNew;` |
|   4239156 |   41 | `		pSet->nSize <<= 1;` |
|   2119577 |   42 | `	}` |
|  28412488 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 212799702 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  28412488 |   45 | `	pSet->nUsed++;` |
|  28412488 |   46 | `	return SXRET_OK;` |
|  14206267 |   47 |  |
|   1150692 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|   1150694 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1150694 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1150694 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1150694 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1150694 |   60 | `	pSet->nSize = nItem;` |
|   1150694 |   61 | `	return SXRET_OK;` |
|    575348 |   62 |  |
|   1622874 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1622876 |   65 | `	pSet->nUsed   = 0;` |
|   1622876 |   66 | `	pSet->nCursor = 0;` |
|   1622876 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     52862 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     52864 |   71 | `	pSet->nCursor = 0;` |
|     52864 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     56944 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     56946 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     21794 |   79 | `		pSet->nCursor = 0;` |
|     21794 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     35154 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     35154 |   83 | `	if( ppEntry ){` |
|     35154 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     17576 |   85 | `	}` |
|     35154 |   86 | `	pSet->nCursor++;` |
|     35154 |   87 | `	return SXRET_OK;` |
|     28474 |   88 |  |
|         - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         8 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|         1 |   91 |  |
|         - |   92 | `	register unsigned char *zSrc;` |
|         9 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         3 |   94 | `		return 0;` |
|         - |   95 | `	}` |
|         7 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|         7 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|         5 |   98 |  |
|         - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    195296 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    195298 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       110 |  103 | `		pSet->nUsed = nNewSize;` |
|        54 |  104 | `	}` |
|    195298 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   9281694 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   9281696 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9281696 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4688360 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2344179 |  112 | `	}` |
|   9281696 |  113 | `	pSet->pBase = 0;` |
|   9281696 |  114 | `	pSet->nUsed = 0;` |
|   9281696 |  115 | `	pSet->nCursor = 0;` |
|   9281696 |  116 | `	return rc;` |
|         2 |  117 |  |
|   5307340 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5307342 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       108 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5307236 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5307236 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2653672 |  126 |  |
|   3416632 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3416634 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2151346 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1265290 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1265290 |  135 | `	pSet->nUsed--;` |
|   1265290 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1265290 |  137 | `	return pData;` |
|   1708318 |  138 |  |
|  12386346 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  12386348 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  12386348 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  12386348 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6193334 |  148 |  |
|         - |  149 | `/* Private hash entry */` |
|         - |  150 | `struct SyHashEntry_Pr` |
|         - |  151 |  |
|         - |  152 | `	const void *pKey; /* Hash key */` |
|         - |  153 | `	sxu32 nKeyLen;    /* Key length */` |
|         - |  154 | `	void *pUserData;  /* User private data */` |
|         - |  155 | `	/* Private fields */` |
|         - |  156 | `	sxu32 nHash;` |
|         - |  157 | `	SyHash *pHash;` |
|         - |  158 | `	SyHashEntry_Pr *pNext,*pPrev; /* Next and previous entry in the list */` |
|         - |  159 | `	SyHashEntry_Pr *pNextCollide,*pPrevCollide; /* Collision list */` |
|         - |  160 | `};` |
|         - |  161 | `#define INVALID_HASH(H) ((H)->apBucket == 0)` |
|    369448 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    369450 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    369450 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    369450 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    369450 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    369450 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    369450 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    369450 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    369450 |  180 | `	pHash->nEntry = 0;` |
|    369450 |  181 | `	pHash->apBucket = apNew;` |
|    369450 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    369450 |  183 | `	return SXRET_OK;` |
|    184726 |  184 |  |
|     91142 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     91144 |  193 | `	pEntry = pHash->pList;` |
|     48495 |  194 | `	for(;;){` |
|     96992 |  195 | `		if( pHash->nEntry == 0 ){` |
|     91144 |  196 | `			break;` |
|         - |  197 | `		}` |
|      5850 |  198 | `		pNext = pEntry->pNext;` |
|      5850 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      5850 |  200 | `		pEntry = pNext;` |
|      5850 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     91144 |  203 | `	if( pHash->apBucket ){` |
|     91144 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     45571 |  205 | `	}` |
|     91144 |  206 | `	pHash->apBucket = 0;` |
|     91144 |  207 | `	pHash->nBucketSize = 0;` |
|     91144 |  208 | `	pHash->pAllocator = 0;` |
|     91144 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  14083544 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  14083546 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  14083546 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  12594441 |  218 | `	for(;;){` |
|  25192353 |  219 | `		if( pEntry == 0 ){` |
|   7636650 |  220 | `			break;` |
|         - |  221 | `		}` |
|  20779023 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   6446900 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   6446898 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  11108809 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   7636650 |  229 | `	return 0;` |
|   7042038 |  230 |  |
|  14703060 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  14703062 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    619672 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  14083392 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  14083392 |  244 | `	if( pEntry == 0 ){` |
|   7636650 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   6446744 |  247 | `	return (SyHashEntry *)pEntry;` |
|   7351796 |  248 |  |
|    109174 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    109176 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     83522 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     41762 |  254 | `	}else{` |
|     25656 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    109176 |  257 | `	if( pEntry->pNextCollide ){` |
|      4765 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2382 |  259 | `	}` |
|    109176 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    109176 |  261 | `	pHash->nEntry--;` |
|    109176 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    109176 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    109176 |  268 | `	return rc;` |
|         2 |  269 |  |
|       154 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       156 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       156 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       156 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       156 |  284 | `	return rc;` |
|        79 |  285 |  |
|    109020 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|    109022 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    109022 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    109022 |  296 | `	return rc;` |
|         2 |  297 |  |
|    605458 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    605460 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    605460 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   3458428 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   3458430 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    605024 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    605024 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2853408 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2853408 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2853408 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1729216 |  325 |  |
|        10 |  326 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         1 |  327 |  |
|         - |  328 | `	SyHashEntry_Pr *pEntry;` |
|         - |  329 | `	sxi32 rc;` |
|         - |  330 | `	sxu32 n;` |
|         - |  331 | `#if defined(UNTRUST)` |
|         - |  332 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  333 | `		return 0;` |
|         - |  334 | `	}` |
|         - |  335 | `#endif` |
|        11 |  336 | `	pEntry = pHash->pList;` |
|      1801 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1791 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1791 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1791 |  344 | `		pEntry = pEntry->pNext;` |
|       896 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     26030 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     26032 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     26032 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     26032 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     26032 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3307984 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3281954 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3281954 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3281954 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3281954 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1568790 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    784376 |  371 | `		}` |
|   3281954 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3281954 |  374 | `		pEntry = pEntry->pNext;` |
|   1640978 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     26032 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     26032 |  378 | `	pHash->apBucket = apNew;` |
|     26032 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     26032 |  380 | `	return SXRET_OK;` |
|     13017 |  381 |  |
|   3501136 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   3501138 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   3501138 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   3501138 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2236193 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1118118 |  389 | `	}` |
|   3501138 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   3501138 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   3501138 |  393 | `	if( pHash->nEntry == 0 ){` |
|    177102 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     88550 |  395 | `	}` |
|   3501138 |  396 | `	pHash->nEntry++;` |
|   3501138 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   3501136 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   3501138 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     26032 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     26032 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     13015 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   3501138 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   3501138 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   3501138 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   3501138 |  421 | `	pEntry->pHash = pHash;` |
|   3501138 |  422 | `	pEntry->pKey = pKey;` |
|   3501138 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   3501138 |  424 | `	pEntry->pUserData = pUserData;` |
|   3501138 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   3501138 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   3501138 |  428 | `	return rc;` |
|   1750570 |  429 |  |
|    138356 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    138358 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
