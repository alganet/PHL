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
|  11025746 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  11025748 |   16 | `	pSet->nSize = 0 ;` |
|  11025748 |   17 | `	pSet->nUsed = 0;` |
|  11025748 |   18 | `	pSet->nCursor = 0;` |
|  11025748 |   19 | `	pSet->eSize = ElemSize;` |
|  11025748 |   20 | `	pSet->pAllocator = pAllocator;` |
|  11025748 |   21 | `	pSet->pBase =  0;` |
|  11025748 |   22 | `	pSet->pUserData = 0;` |
|  11025748 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  17504950 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  17504952 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3454856 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3454856 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3381426 |   34 | `			pSet->nSize = 4;` |
|   1690712 |   35 | `		}` |
|   3454856 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3454856 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3454856 |   40 | `		pSet->pBase = pNew;` |
|   3454856 |   41 | `		pSet->nSize <<= 1;` |
|   1727427 |   42 | `	}` |
|  17504952 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 130939868 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  17504952 |   45 | `	pSet->nUsed++;` |
|  17504952 |   46 | `	return SXRET_OK;` |
|   8752499 |   47 |  |
|    514662 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    514664 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    514664 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    514664 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    514664 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    514664 |   60 | `	pSet->nSize = nItem;` |
|    514664 |   61 | `	return SXRET_OK;` |
|    257333 |   62 |  |
|    980190 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|    980192 |   65 | `	pSet->nUsed   = 0;` |
|    980192 |   66 | `	pSet->nCursor = 0;` |
|    980192 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     38494 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     38496 |   71 | `	pSet->nCursor = 0;` |
|     38496 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     42312 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     42314 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     15558 |   79 | `		pSet->nCursor = 0;` |
|     15558 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     26758 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     26758 |   83 | `	if( ppEntry ){` |
|     26758 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     13378 |   85 | `	}` |
|     26758 |   86 | `	pSet->nCursor++;` |
|     26758 |   87 | `	return SXRET_OK;` |
|     21158 |   88 |  |
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
|     65324 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     65326 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     65326 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   7188362 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   7188364 |  109 | `	sxi32 rc = SXRET_OK;` |
|   7188364 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3701884 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1850941 |  112 | `	}` |
|   7188364 |  113 | `	pSet->pBase = 0;` |
|   7188364 |  114 | `	pSet->nUsed = 0;` |
|   7188364 |  115 | `	pSet->nCursor = 0;` |
|   7188364 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3539032 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3539034 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3538944 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3538944 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1769518 |  126 |  |
|   3078706 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3078708 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2129434 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    949276 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    949276 |  135 | `	pSet->nUsed--;` |
|    949276 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    949276 |  137 | `	return pData;` |
|   1539355 |  138 |  |
|   9438912 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   9438914 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   9438914 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   9438914 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4719696 |  148 |  |
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
|     92308 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     92310 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     92310 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     92310 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     92310 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     92310 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     92310 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     92310 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     92310 |  180 | `	pHash->nEntry = 0;` |
|     92310 |  181 | `	pHash->apBucket = apNew;` |
|     92310 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     92310 |  183 | `	return SXRET_OK;` |
|     46156 |  184 |  |
|     11366 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     11368 |  193 | `	pEntry = pHash->pList;` |
|      6981 |  194 | `	for(;;){` |
|     13964 |  195 | `		if( pHash->nEntry == 0 ){` |
|     11368 |  196 | `			break;` |
|         - |  197 | `		}` |
|      2598 |  198 | `		pNext = pEntry->pNext;` |
|      2598 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      2598 |  200 | `		pEntry = pNext;` |
|      2598 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     11368 |  203 | `	if( pHash->apBucket ){` |
|     11368 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      5683 |  205 | `	}` |
|     11368 |  206 | `	pHash->apBucket = 0;` |
|     11368 |  207 | `	pHash->nBucketSize = 0;` |
|     11368 |  208 | `	pHash->pAllocator = 0;` |
|     11368 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   9265038 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   9265040 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   9265040 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   8029689 |  218 | `	for(;;){` |
|  15960645 |  219 | `		if( pEntry == 0 ){` |
|   5047062 |  220 | `			break;` |
|         - |  221 | `		}` |
|  13022444 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   4217982 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   4217980 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   6695607 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   5047062 |  229 | `	return 0;` |
|   4632785 |  230 |  |
|   9317006 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   9317008 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     51976 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   9265034 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   9265034 |  244 | `	if( pEntry == 0 ){` |
|   5047062 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   4217974 |  247 | `	return (SyHashEntry *)pEntry;` |
|   4658769 |  248 |  |
|     71528 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     71530 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     53746 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     26874 |  254 | `	}else{` |
|     17786 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     71530 |  257 | `	if( pEntry->pNextCollide ){` |
|      4091 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2045 |  259 | `	}` |
|     71530 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     71530 |  261 | `	pHash->nEntry--;` |
|     71530 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     71530 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     71530 |  268 | `	return rc;` |
|         2 |  269 |  |
|         6 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         1 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|         7 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|         7 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|         7 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|         7 |  284 | `	return rc;` |
|         4 |  285 |  |
|     71522 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     71524 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     71524 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     71524 |  296 | `	return rc;` |
|         2 |  297 |  |
|    131912 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    131914 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    131914 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    916474 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    916476 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    131480 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    131480 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    784998 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    784998 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    784998 |  324 | `	return (SyHashEntry *)pEntry;` |
|    458239 |  325 |  |
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
|      1609 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1599 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1599 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1599 |  344 | `		pEntry = pEntry->pNext;` |
|       800 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     13592 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     13594 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     13594 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     13594 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     13594 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1868410 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1854818 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1854818 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1854818 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1854818 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    890654 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    445326 |  371 | `		}` |
|   1854818 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1854818 |  374 | `		pEntry = pEntry->pNext;` |
|    927410 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     13594 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     13594 |  378 | `	pHash->apBucket = apNew;` |
|     13594 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     13594 |  380 | `	return SXRET_OK;` |
|      6798 |  381 |  |
|   1671332 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1671334 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1671334 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1671334 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1125532 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    562801 |  389 | `	}` |
|   1671334 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1671334 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1671334 |  393 | `	if( pHash->nEntry == 0 ){` |
|     66344 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     33171 |  395 | `	}` |
|   1671334 |  396 | `	pHash->nEntry++;` |
|   1671334 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1671332 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1671334 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     13594 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     13594 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      6796 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1671334 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1671334 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1671334 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1671334 |  421 | `	pEntry->pHash = pHash;` |
|   1671334 |  422 | `	pEntry->pKey = pKey;` |
|   1671334 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1671334 |  424 | `	pEntry->pUserData = pUserData;` |
|   1671334 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1671334 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1671334 |  428 | `	return rc;` |
|    835668 |  429 |  |
|     88716 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     88718 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
