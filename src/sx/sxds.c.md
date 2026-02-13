# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 272/287 lines (94.77%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "sxtypes.h"` |
|        - |    7 | `#include "sxmacros.h"` |
|        - |    8 | `#include "sxset.h"` |
|        - |    9 | `#include "sxmem.h"` |
|        - |   10 | `#include "sxhashtable.h"` |
|        - |   11 | `#include "sxhash.h"` |
|        - |   12 | `#include "sxstr.h"` |
|        - |   13 |  |
|  4543754 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|        2 |   15 |  |
|  4543756 |   16 | `	pSet->nSize = 0 ;` |
|  4543756 |   17 | `	pSet->nUsed = 0;` |
|  4543756 |   18 | `	pSet->nCursor = 0;` |
|  4543756 |   19 | `	pSet->eSize = ElemSize;` |
|  4543756 |   20 | `	pSet->pAllocator = pAllocator;` |
|  4543756 |   21 | `	pSet->pBase =  0;` |
|  4543756 |   22 | `	pSet->pUserData = 0;` |
|  4543756 |   23 | `	return SXRET_OK;` |
|        2 |   24 |  |
|  7423462 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|        2 |   26 |  |
|        - |   27 | `	unsigned char *zbase;` |
|  7423464 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|        - |   29 | `		void *pNew;` |
|   934052 |   30 | `		if( pSet->pAllocator == 0 ){` |
|      ! 0 |   31 | `			return  SXERR_LOCKED;` |
|        - |   32 | `		}` |
|   934052 |   33 | `		if( pSet->nSize <= 0 ){` |
|   888070 |   34 | `			pSet->nSize = 4;` |
|   444034 |   35 | `		}` |
|   934052 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   934052 |   37 | `		if( pNew == 0 ){` |
|      ! 0 |   38 | `			return SXERR_MEM;` |
|        - |   39 | `		}` |
|   934052 |   40 | `		pSet->pBase = pNew;` |
|   934052 |   41 | `		pSet->nSize <<= 1;` |
|   467025 |   42 | `	}` |
|  7423464 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 48906128 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  7423464 |   45 | `	pSet->nUsed++;` |
|  7423464 |   46 | `	return SXRET_OK;` |
|  3711755 |   47 |  |
|   331548 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|        2 |   49 |  |
|   331550 |   50 | `	if( pSet->nSize > 0 ){` |
|      ! 0 |   51 | `		return SXERR_LOCKED;` |
|        - |   52 | `	}` |
|   331550 |   53 | `	if( nItem < 8 ){` |
|      ! 0 |   54 | `		nItem = 8;` |
|      ! 0 |   55 | `	}` |
|   331550 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   331550 |   57 | `	if( pSet->pBase == 0 ){` |
|      ! 0 |   58 | `		return SXERR_MEM;` |
|        - |   59 | `	}` |
|   331550 |   60 | `	pSet->nSize = nItem;` |
|   331550 |   61 | `	return SXRET_OK;` |
|   165776 |   62 |  |
|   697568 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|        2 |   64 |  |
|   697570 |   65 | `	pSet->nUsed   = 0;` |
|   697570 |   66 | `	pSet->nCursor = 0;` |
|   697570 |   67 | `	return SXRET_OK;` |
|        2 |   68 |  |
|    30486 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|        2 |   70 |  |
|    30488 |   71 | `	pSet->nCursor = 0;` |
|    30488 |   72 | `	return SXRET_OK;` |
|        2 |   73 |  |
|    33346 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|        2 |   75 |  |
|        - |   76 | `	register unsigned char *zSrc;` |
|    33348 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|        - |   78 | `		/* Reset cursor */` |
|    12038 |   79 | `		pSet->nCursor = 0;` |
|    12038 |   80 | `		return SXERR_EOF;` |
|        - |   81 | `	}` |
|    21312 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|    21312 |   83 | `	if( ppEntry ){` |
|    21312 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|    10655 |   85 | `	}` |
|    21312 |   86 | `	pSet->nCursor++;` |
|    21312 |   87 | `	return SXRET_OK;` |
|    16675 |   88 |  |
|        - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        8 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|        1 |   91 |  |
|        - |   92 | `	register unsigned char *zSrc;` |
|        9 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|        3 |   94 | `		return 0;` |
|        - |   95 | `	}` |
|        7 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|        7 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|        5 |   98 |  |
|        - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    40918 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|        2 |  101 |  |
|    40920 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       20 |  103 | `		pSet->nUsed = nNewSize;` |
|        9 |  104 | `	}` |
|    40920 |  105 | `	return SXRET_OK;` |
|        2 |  106 |  |
|  2041162 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|        2 |  108 |  |
|  2041164 |  109 | `	sxi32 rc = SXRET_OK;` |
|  2041164 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|  1100248 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   550123 |  112 | `	}` |
|  2041164 |  113 | `	pSet->pBase = 0;` |
|  2041164 |  114 | `	pSet->nUsed = 0;` |
|  2041164 |  115 | `	pSet->nCursor = 0;` |
|  2041164 |  116 | `	return rc;` |
|        2 |  117 |  |
|   964326 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|        2 |  119 |  |
|        - |  120 | `	const char *zBase;` |
|   964328 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       92 |  122 | `		return 0;` |
|        - |  123 | `	}` |
|   964238 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   964238 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   482165 |  126 |  |
|   694470 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|        2 |  128 |  |
|        - |  129 | `	const char *zBase;` |
|        - |  130 | `	void *pData;` |
|   694472 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    75564 |  132 | `		return 0;` |
|        - |  133 | `	}` |
|   618910 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   618910 |  135 | `	pSet->nUsed--;` |
|   618910 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   618910 |  137 | `	return pData;` |
|   347237 |  138 |  |
|  5165980 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|        2 |  140 |  |
|        - |  141 | `	const char *zBase;` |
|  5165982 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|        - |  143 | `		/* Out of range */` |
|      ! 0 |  144 | `		return 0;` |
|        - |  145 | `	}` |
|  5165982 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  5165982 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|  2583218 |  148 |  |
|        - |  149 | `/* Private hash entry */` |
|        - |  150 | `struct SyHashEntry_Pr` |
|        - |  151 |  |
|        - |  152 | `	const void *pKey; /* Hash key */` |
|        - |  153 | `	sxu32 nKeyLen;    /* Key length */` |
|        - |  154 | `	void *pUserData;  /* User private data */` |
|        - |  155 | `	/* Private fields */` |
|        - |  156 | `	sxu32 nHash;` |
|        - |  157 | `	SyHash *pHash;` |
|        - |  158 | `	SyHashEntry_Pr *pNext,*pPrev; /* Next and previous entry in the list */` |
|        - |  159 | `	SyHashEntry_Pr *pNextCollide,*pPrevCollide; /* Collision list */` |
|        - |  160 | `};` |
|        - |  161 | `#define INVALID_HASH(H) ((H)->apBucket == 0)` |
|    44626 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|        2 |  163 |  |
|        - |  164 | `	SyHashEntry_Pr **apNew;` |
|        - |  165 | `#if defined(UNTRUST)` |
|        - |  166 | `	if( pHash == 0 ){` |
|        - |  167 | `		return SXERR_EMPTY;` |
|        - |  168 | `	}` |
|        - |  169 | `#endif` |
|        - |  170 | `	/* Allocate a new table */` |
|    44628 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    44628 |  172 | `	if( apNew == 0 ){` |
|      ! 0 |  173 | `		return SXERR_MEM;` |
|        - |  174 | `	}` |
|    44628 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    44628 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    44628 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    44628 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    44628 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    44628 |  180 | `	pHash->nEntry = 0;` |
|    44628 |  181 | `	pHash->apBucket = apNew;` |
|    44628 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    44628 |  183 | `	return SXRET_OK;` |
|    22315 |  184 |  |
|     8012 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|        2 |  186 |  |
|        - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|        - |  188 | `#if defined(UNTRUST)` |
|        - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|        - |  190 | `		return SXERR_EMPTY;` |
|        - |  191 | `	}` |
|        - |  192 | `#endif` |
|     8014 |  193 | `	pEntry = pHash->pList;` |
|     4223 |  194 | `	for(;;){` |
|     8448 |  195 | `		if( pHash->nEntry == 0 ){` |
|     8014 |  196 | `			break;` |
|        - |  197 | `		}` |
|      436 |  198 | `		pNext = pEntry->pNext;` |
|      436 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      436 |  200 | `		pEntry = pNext;` |
|      436 |  201 | `		pHash->nEntry--;` |
|        2 |  202 | `	}` |
|     8014 |  203 | `	if( pHash->apBucket ){` |
|     8014 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     4006 |  205 | `	}` |
|     8014 |  206 | `	pHash->apBucket = 0;` |
|     8014 |  207 | `	pHash->nBucketSize = 0;` |
|     8014 |  208 | `	pHash->pAllocator = 0;` |
|     8014 |  209 | `	return SXRET_OK;` |
|        2 |  210 |  |
|  5999034 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|        2 |  212 |  |
|        - |  213 | `	SyHashEntry_Pr *pEntry;` |
|        - |  214 | `	sxu32 nHash;` |
|        - |  215 |  |
|  5999036 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  5999036 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  5289707 |  218 | `	for(;;){` |
| 10614753 |  219 | `		if( pEntry == 0 ){` |
|  3238100 |  220 | `			break;` |
|        - |  221 | `		}` |
|  8756993 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|  2760940 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|  2760938 |  224 | `				return pEntry;` |
|        - |  225 | `		}` |
|  4615719 |  226 | `		pEntry = pEntry->pNextCollide;` |
|        2 |  227 | `	}` |
|        - |  228 | `	/* Entry not found */` |
|  3238100 |  229 | `	return 0;` |
|  2999783 |  230 |  |
|  6023242 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|        2 |  232 |  |
|        - |  233 | `	SyHashEntry_Pr *pEntry;` |
|        - |  234 | `#if defined(UNTRUST)` |
|        - |  235 | `	if( INVALID_HASH(pHash) ){` |
|        - |  236 | `		return 0;` |
|        - |  237 | `	}` |
|        - |  238 | `#endif` |
|  6023244 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|        - |  240 | `		/* Don't bother hashing,return immediately */` |
|    24216 |  241 | `		return 0;` |
|        - |  242 | `	}` |
|  5999030 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  5999030 |  244 | `	if( pEntry == 0 ){` |
|  3238100 |  245 | `		return 0;` |
|        - |  246 | `	}` |
|  2760932 |  247 | `	return (SyHashEntry *)pEntry;` |
|  3011887 |  248 |  |
|    53356 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|        2 |  250 |  |
|        - |  251 | `	sxi32 rc;` |
|    53358 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    39600 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|    19801 |  254 | `	}else{` |
|    13760 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|        - |  256 | `	}` |
|    53358 |  257 | `	if( pEntry->pNextCollide ){` |
|     3423 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     1711 |  259 | `	}` |
|    53358 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    53358 |  261 | `	pHash->nEntry--;` |
|    53358 |  262 | `	if( ppUserData ){` |
|        - |  263 | `		/* Write a pointer to the user data */` |
|      ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|      ! 0 |  265 | `	}` |
|        - |  266 | `	/* Release the entry */` |
|    53358 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    53358 |  268 | `	return rc;` |
|        2 |  269 |  |
|        6 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|        1 |  271 |  |
|        - |  272 | `	SyHashEntry_Pr *pEntry;` |
|        - |  273 | `	sxi32 rc;` |
|        - |  274 | `#if defined(UNTRUST)` |
|        - |  275 | `	if( INVALID_HASH(pHash) ){` |
|        - |  276 | `		return SXERR_CORRUPT;` |
|        - |  277 | `	}` |
|        - |  278 | `#endif` |
|        7 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        7 |  280 | `	if( pEntry == 0 ){` |
|      ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|        - |  282 | `	}` |
|        7 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        7 |  284 | `	return rc;` |
|        4 |  285 |  |
|    53350 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|        2 |  287 |  |
|    53352 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|        - |  289 | `	sxi32 rc;` |
|        - |  290 | `#if defined(UNTRUST)` |
|        - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|        - |  292 | `		return SXERR_CORRUPT;` |
|        - |  293 | `	}` |
|        - |  294 | `#endif` |
|    53352 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    53352 |  296 | `	return rc;` |
|        2 |  297 |  |
|    64664 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|        2 |  299 |  |
|        - |  300 | `#if defined(UNTRUST)` |
|        - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|        - |  302 | `		return SXERR_CORRUPT;` |
|        - |  303 | `	}` |
|        - |  304 | `#endif` |
|    64666 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    64666 |  306 | `	return SXRET_OK;` |
|        2 |  307 |  |
|   366416 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|        2 |  309 |  |
|        - |  310 | `	SyHashEntry_Pr *pEntry;` |
|        - |  311 | `#if defined(UNTRUST)` |
|        - |  312 | `	if( INVALID_HASH(pHash) ){` |
|        - |  313 | `		return 0;` |
|        - |  314 | `	}` |
|        - |  315 | `#endif` |
|   366418 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    64232 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    64232 |  318 | `		return 0;` |
|        - |  319 | `	}` |
|   302188 |  320 | `	pEntry = pHash->pCurrent;` |
|        - |  321 | `	/* Advance the cursor */` |
|   302188 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|        - |  323 | `	/* Return the current entry */` |
|   302188 |  324 | `	return (SyHashEntry *)pEntry;` |
|   183210 |  325 |  |
|       10 |  326 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|        1 |  327 |  |
|        - |  328 | `	SyHashEntry_Pr *pEntry;` |
|        - |  329 | `	sxi32 rc;` |
|        - |  330 | `	sxu32 n;` |
|        - |  331 | `#if defined(UNTRUST)` |
|        - |  332 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|        - |  333 | `		return 0;` |
|        - |  334 | `	}` |
|        - |  335 | `#endif` |
|       11 |  336 | `	pEntry = pHash->pList;` |
|     1569 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|        - |  338 | `		/* Invoke the callback */` |
|     1559 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|     1559 |  340 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  341 | `			return rc;` |
|        - |  342 | `		}` |
|        - |  343 | `		/* Point to the next entry */` |
|     1559 |  344 | `		pEntry = pEntry->pNext;` |
|      780 |  345 | `	}` |
|       11 |  346 | `	return SXRET_OK;` |
|        6 |  347 |  |
|     7708 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|        2 |  349 |  |
|     7710 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|        - |  351 | `	SyHashEntry_Pr *pEntry;` |
|        - |  352 | `	SyHashEntry_Pr **apNew;` |
|        - |  353 | `	sxu32 n,iBucket;` |
|        - |  354 |  |
|        - |  355 | `	/* Allocate a new larger table */` |
|     7710 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     7710 |  357 | `	if( apNew == 0 ){` |
|        - |  358 | `		/* Not so fatal,simply a performance hit */` |
|      ! 0 |  359 | `		return SXRET_OK;` |
|        - |  360 | `	}` |
|        - |  361 | `	/* Zero the new table */` |
|     7710 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|        - |  363 | `	/* Rehash all entries */` |
|  1048638 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|  1040930 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - |  366 | `		/* Install in the new bucket */` |
|  1040930 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|  1040930 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|  1040930 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   499936 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|   249972 |  371 | `		}` |
|  1040930 |  372 | `		apNew[iBucket] = pEntry;` |
|        - |  373 | `		/* Point to the next entry */` |
|  1040930 |  374 | `		pEntry = pEntry->pNext;` |
|   520466 |  375 | `	}` |
|        - |  376 | `	/* Release the old table and reflect the change */` |
|     7710 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     7710 |  378 | `	pHash->apBucket = apNew;` |
|     7710 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     7710 |  380 | `	return SXRET_OK;` |
|     3856 |  381 |  |
|   890402 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|        2 |  383 |  |
|   890404 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|        - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   890404 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   890404 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   631981 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   315974 |  389 | `	}` |
|   890404 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|        - |  391 | `	/* Link to the entry list */` |
|   890404 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   890404 |  393 | `	if( pHash->nEntry == 0 ){` |
|    30240 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    15119 |  395 | `	}` |
|   890404 |  396 | `	pHash->nEntry++;` |
|   890404 |  397 | `	return SXRET_OK;` |
|        2 |  398 |  |
|   890402 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|        2 |  400 |  |
|        - |  401 | `	SyHashEntry_Pr *pEntry;` |
|        - |  402 | `	sxi32 rc;` |
|        - |  403 | `#if defined(UNTRUST)` |
|        - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|        - |  405 | `		return SXERR_CORRUPT;` |
|        - |  406 | `	}` |
|        - |  407 | `#endif` |
|   890404 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     7710 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     7710 |  410 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  411 | `			return rc;` |
|        - |  412 | `		}` |
|     3854 |  413 | `	}` |
|        - |  414 | `	/* Allocate a new hash entry */` |
|   890404 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   890404 |  416 | `	if( pEntry == 0 ){` |
|      ! 0 |  417 | `		return SXERR_MEM;` |
|        - |  418 | `	}` |
|        - |  419 | `	/* Zero the entry */` |
|   890404 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   890404 |  421 | `	pEntry->pHash = pHash;` |
|   890404 |  422 | `	pEntry->pKey = pKey;` |
|   890404 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   890404 |  424 | `	pEntry->pUserData = pUserData;` |
|   890404 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|        - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   890404 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   890404 |  428 | `	return rc;` |
|   445203 |  429 |  |
|    63076 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|        2 |  431 |  |
|        - |  432 | `#if defined(UNTRUST)` |
|        - |  433 | `	if( INVALID_HASH(pHash) ){` |
|        - |  434 | `		return 0;` |
|        - |  435 | `	}` |
|        - |  436 | `#endif` |
|        - |  437 | `	/* Last inserted entry */` |
|    63078 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|        2 |  439 |  |
|        - |  440 |  |
