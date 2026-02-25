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
|  4925992 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|        2 |   15 |  |
|  4925994 |   16 | `	pSet->nSize = 0 ;` |
|  4925994 |   17 | `	pSet->nUsed = 0;` |
|  4925994 |   18 | `	pSet->nCursor = 0;` |
|  4925994 |   19 | `	pSet->eSize = ElemSize;` |
|  4925994 |   20 | `	pSet->pAllocator = pAllocator;` |
|  4925994 |   21 | `	pSet->pBase =  0;` |
|  4925994 |   22 | `	pSet->pUserData = 0;` |
|  4925994 |   23 | `	return SXRET_OK;` |
|        2 |   24 |  |
|  8095756 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|        2 |   26 |  |
|        - |   27 | `	unsigned char *zbase;` |
|  8095758 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|        - |   29 | `		void *pNew;` |
|   992992 |   30 | `		if( pSet->pAllocator == 0 ){` |
|      ! 0 |   31 | `			return  SXERR_LOCKED;` |
|        - |   32 | `		}` |
|   992992 |   33 | `		if( pSet->nSize <= 0 ){` |
|   942360 |   34 | `			pSet->nSize = 4;` |
|   471179 |   35 | `		}` |
|   992992 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   992992 |   37 | `		if( pNew == 0 ){` |
|      ! 0 |   38 | `			return SXERR_MEM;` |
|        - |   39 | `		}` |
|   992992 |   40 | `		pSet->pBase = pNew;` |
|   992992 |   41 | `		pSet->nSize <<= 1;` |
|   496495 |   42 | `	}` |
|  8095758 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 53707474 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  8095758 |   45 | `	pSet->nUsed++;` |
|  8095758 |   46 | `	return SXRET_OK;` |
|  4047902 |   47 |  |
|   364256 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|        2 |   49 |  |
|   364258 |   50 | `	if( pSet->nSize > 0 ){` |
|      ! 0 |   51 | `		return SXERR_LOCKED;` |
|        - |   52 | `	}` |
|   364258 |   53 | `	if( nItem < 8 ){` |
|      ! 0 |   54 | `		nItem = 8;` |
|      ! 0 |   55 | `	}` |
|   364258 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   364258 |   57 | `	if( pSet->pBase == 0 ){` |
|      ! 0 |   58 | `		return SXERR_MEM;` |
|        - |   59 | `	}` |
|   364258 |   60 | `	pSet->nSize = nItem;` |
|   364258 |   61 | `	return SXRET_OK;` |
|   182130 |   62 |  |
|   744872 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|        2 |   64 |  |
|   744874 |   65 | `	pSet->nUsed   = 0;` |
|   744874 |   66 | `	pSet->nCursor = 0;` |
|   744874 |   67 | `	return SXRET_OK;` |
|        2 |   68 |  |
|    31540 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|        2 |   70 |  |
|    31542 |   71 | `	pSet->nCursor = 0;` |
|    31542 |   72 | `	return SXRET_OK;` |
|        2 |   73 |  |
|    34486 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|        2 |   75 |  |
|        - |   76 | `	register unsigned char *zSrc;` |
|    34488 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|        - |   78 | `		/* Reset cursor */` |
|    12526 |   79 | `		pSet->nCursor = 0;` |
|    12526 |   80 | `		return SXERR_EOF;` |
|        - |   81 | `	}` |
|    21964 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|    21964 |   83 | `	if( ppEntry ){` |
|    21964 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|    10981 |   85 | `	}` |
|    21964 |   86 | `	pSet->nCursor++;` |
|    21964 |   87 | `	return SXRET_OK;` |
|    17245 |   88 |  |
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
|    45478 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|        2 |  101 |  |
|    45480 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       20 |  103 | `		pSet->nUsed = nNewSize;` |
|        9 |  104 | `	}` |
|    45480 |  105 | `	return SXRET_OK;` |
|        2 |  106 |  |
|  2183940 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|        2 |  108 |  |
|  2183942 |  109 | `	sxi32 rc = SXRET_OK;` |
|  2183942 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|  1173376 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   586687 |  112 | `	}` |
|  2183942 |  113 | `	pSet->pBase = 0;` |
|  2183942 |  114 | `	pSet->nUsed = 0;` |
|  2183942 |  115 | `	pSet->nCursor = 0;` |
|  2183942 |  116 | `	return rc;` |
|        2 |  117 |  |
|  1058984 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|        2 |  119 |  |
|        - |  120 | `	const char *zBase;` |
|  1058986 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       92 |  122 | `		return 0;` |
|        - |  123 | `	}` |
|  1058896 |  124 | `	zBase = (const char *)pSet->pBase;` |
|  1058896 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   529494 |  126 |  |
|   730174 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|        2 |  128 |  |
|        - |  129 | `	const char *zBase;` |
|        - |  130 | `	void *pData;` |
|   730176 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    78436 |  132 | `		return 0;` |
|        - |  133 | `	}` |
|   651742 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   651742 |  135 | `	pSet->nUsed--;` |
|   651742 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   651742 |  137 | `	return pData;` |
|   365089 |  138 |  |
|  5441304 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|        2 |  140 |  |
|        - |  141 | `	const char *zBase;` |
|  5441306 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|        - |  143 | `		/* Out of range */` |
|      ! 0 |  144 | `		return 0;` |
|        - |  145 | `	}` |
|  5441306 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  5441306 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|  2720835 |  148 |  |
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
|    65028 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|        2 |  163 |  |
|        - |  164 | `	SyHashEntry_Pr **apNew;` |
|        - |  165 | `#if defined(UNTRUST)` |
|        - |  166 | `	if( pHash == 0 ){` |
|        - |  167 | `		return SXERR_EMPTY;` |
|        - |  168 | `	}` |
|        - |  169 | `#endif` |
|        - |  170 | `	/* Allocate a new table */` |
|    65030 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    65030 |  172 | `	if( apNew == 0 ){` |
|      ! 0 |  173 | `		return SXERR_MEM;` |
|        - |  174 | `	}` |
|    65030 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    65030 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    65030 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    65030 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    65030 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    65030 |  180 | `	pHash->nEntry = 0;` |
|    65030 |  181 | `	pHash->apBucket = apNew;` |
|    65030 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    65030 |  183 | `	return SXRET_OK;` |
|    32516 |  184 |  |
|     8546 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|        2 |  186 |  |
|        - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|        - |  188 | `#if defined(UNTRUST)` |
|        - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|        - |  190 | `		return SXERR_EMPTY;` |
|        - |  191 | `	}` |
|        - |  192 | `#endif` |
|     8548 |  193 | `	pEntry = pHash->pList;` |
|     4676 |  194 | `	for(;;){` |
|     9354 |  195 | `		if( pHash->nEntry == 0 ){` |
|     8548 |  196 | `			break;` |
|        - |  197 | `		}` |
|      808 |  198 | `		pNext = pEntry->pNext;` |
|      808 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      808 |  200 | `		pEntry = pNext;` |
|      808 |  201 | `		pHash->nEntry--;` |
|        2 |  202 | `	}` |
|     8548 |  203 | `	if( pHash->apBucket ){` |
|     8548 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     4273 |  205 | `	}` |
|     8548 |  206 | `	pHash->apBucket = 0;` |
|     8548 |  207 | `	pHash->nBucketSize = 0;` |
|     8548 |  208 | `	pHash->pAllocator = 0;` |
|     8548 |  209 | `	return SXRET_OK;` |
|        2 |  210 |  |
|  6632846 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|        2 |  212 |  |
|        - |  213 | `	SyHashEntry_Pr *pEntry;` |
|        - |  214 | `	sxu32 nHash;` |
|        - |  215 |  |
|  6632848 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  6632848 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  5849324 |  218 | `	for(;;){` |
| 11735345 |  219 | `		if( pEntry == 0 ){` |
|  3577322 |  220 | `			break;` |
|        - |  221 | `		}` |
|  9685658 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|  3055530 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|  3055528 |  224 | `				return pEntry;` |
|        - |  225 | `		}` |
|  5102499 |  226 | `		pEntry = pEntry->pNextCollide;` |
|        2 |  227 | `	}` |
|        - |  228 | `	/* Entry not found */` |
|  3577322 |  229 | `	return 0;` |
|  3316689 |  230 |  |
|  6669856 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|        2 |  232 |  |
|        - |  233 | `	SyHashEntry_Pr *pEntry;` |
|        - |  234 | `#if defined(UNTRUST)` |
|        - |  235 | `	if( INVALID_HASH(pHash) ){` |
|        - |  236 | `		return 0;` |
|        - |  237 | `	}` |
|        - |  238 | `#endif` |
|  6669858 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|        - |  240 | `		/* Don't bother hashing,return immediately */` |
|    37018 |  241 | `		return 0;` |
|        - |  242 | `	}` |
|  6632842 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  6632842 |  244 | `	if( pEntry == 0 ){` |
|  3577322 |  245 | `		return 0;` |
|        - |  246 | `	}` |
|  3055522 |  247 | `	return (SyHashEntry *)pEntry;` |
|  3335194 |  248 |  |
|    56056 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|        2 |  250 |  |
|        - |  251 | `	sxi32 rc;` |
|    56058 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    41746 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|    20874 |  254 | `	}else{` |
|    14314 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|        - |  256 | `	}` |
|    56058 |  257 | `	if( pEntry->pNextCollide ){` |
|     3449 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     1724 |  259 | `	}` |
|    56058 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    56058 |  261 | `	pHash->nEntry--;` |
|    56058 |  262 | `	if( ppUserData ){` |
|        - |  263 | `		/* Write a pointer to the user data */` |
|      ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|      ! 0 |  265 | `	}` |
|        - |  266 | `	/* Release the entry */` |
|    56058 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    56058 |  268 | `	return rc;` |
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
|    56050 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|        2 |  287 |  |
|    56052 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|        - |  289 | `	sxi32 rc;` |
|        - |  290 | `#if defined(UNTRUST)` |
|        - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|        - |  292 | `		return SXERR_CORRUPT;` |
|        - |  293 | `	}` |
|        - |  294 | `#endif` |
|    56052 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    56052 |  296 | `	return rc;` |
|        2 |  297 |  |
|    98220 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|        2 |  299 |  |
|        - |  300 | `#if defined(UNTRUST)` |
|        - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|        - |  302 | `		return SXERR_CORRUPT;` |
|        - |  303 | `	}` |
|        - |  304 | `#endif` |
|    98222 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    98222 |  306 | `	return SXRET_OK;` |
|        2 |  307 |  |
|   686422 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|        2 |  309 |  |
|        - |  310 | `	SyHashEntry_Pr *pEntry;` |
|        - |  311 | `#if defined(UNTRUST)` |
|        - |  312 | `	if( INVALID_HASH(pHash) ){` |
|        - |  313 | `		return 0;` |
|        - |  314 | `	}` |
|        - |  315 | `#endif` |
|   686424 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    97788 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    97788 |  318 | `		return 0;` |
|        - |  319 | `	}` |
|   588638 |  320 | `	pEntry = pHash->pCurrent;` |
|        - |  321 | `	/* Advance the cursor */` |
|   588638 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|        - |  323 | `	/* Return the current entry */` |
|   588638 |  324 | `	return (SyHashEntry *)pEntry;` |
|   343213 |  325 |  |
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
|     1573 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|        - |  338 | `		/* Invoke the callback */` |
|     1563 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|     1563 |  340 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  341 | `			return rc;` |
|        - |  342 | `		}` |
|        - |  343 | `		/* Point to the next entry */` |
|     1563 |  344 | `		pEntry = pEntry->pNext;` |
|      782 |  345 | `	}` |
|       11 |  346 | `	return SXRET_OK;` |
|        6 |  347 |  |
|     8796 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|        2 |  349 |  |
|     8798 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|        - |  351 | `	SyHashEntry_Pr *pEntry;` |
|        - |  352 | `	SyHashEntry_Pr **apNew;` |
|        - |  353 | `	sxu32 n,iBucket;` |
|        - |  354 |  |
|        - |  355 | `	/* Allocate a new larger table */` |
|     8798 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     8798 |  357 | `	if( apNew == 0 ){` |
|        - |  358 | `		/* Not so fatal,simply a performance hit */` |
|      ! 0 |  359 | `		return SXRET_OK;` |
|        - |  360 | `	}` |
|        - |  361 | `	/* Zero the new table */` |
|     8798 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|        - |  363 | `	/* Rehash all entries */` |
|  1199966 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|  1191170 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - |  366 | `		/* Install in the new bucket */` |
|  1191170 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|  1191170 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|  1191170 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   572054 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|   286030 |  371 | `		}` |
|  1191170 |  372 | `		apNew[iBucket] = pEntry;` |
|        - |  373 | `		/* Point to the next entry */` |
|  1191170 |  374 | `		pEntry = pEntry->pNext;` |
|   595586 |  375 | `	}` |
|        - |  376 | `	/* Release the old table and reflect the change */` |
|     8798 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     8798 |  378 | `	pHash->apBucket = apNew;` |
|     8798 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     8798 |  380 | `	return SXRET_OK;` |
|     4400 |  381 |  |
|  1099440 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|        2 |  383 |  |
|  1099442 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|        - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|  1099442 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|  1099442 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   727400 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   363702 |  389 | `	}` |
|  1099442 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|        - |  391 | `	/* Link to the entry list */` |
|  1099442 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|  1099442 |  393 | `	if( pHash->nEntry == 0 ){` |
|    46528 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    23263 |  395 | `	}` |
|  1099442 |  396 | `	pHash->nEntry++;` |
|  1099442 |  397 | `	return SXRET_OK;` |
|        2 |  398 |  |
|  1099440 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|        2 |  400 |  |
|        - |  401 | `	SyHashEntry_Pr *pEntry;` |
|        - |  402 | `	sxi32 rc;` |
|        - |  403 | `#if defined(UNTRUST)` |
|        - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|        - |  405 | `		return SXERR_CORRUPT;` |
|        - |  406 | `	}` |
|        - |  407 | `#endif` |
|  1099442 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     8798 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     8798 |  410 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  411 | `			return rc;` |
|        - |  412 | `		}` |
|     4398 |  413 | `	}` |
|        - |  414 | `	/* Allocate a new hash entry */` |
|  1099442 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|  1099442 |  416 | `	if( pEntry == 0 ){` |
|      ! 0 |  417 | `		return SXERR_MEM;` |
|        - |  418 | `	}` |
|        - |  419 | `	/* Zero the entry */` |
|  1099442 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|  1099442 |  421 | `	pEntry->pHash = pHash;` |
|  1099442 |  422 | `	pEntry->pKey = pKey;` |
|  1099442 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|  1099442 |  424 | `	pEntry->pUserData = pUserData;` |
|  1099442 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|        - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|  1099442 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|  1099442 |  428 | `	return rc;` |
|   549722 |  429 |  |
|    67144 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|        2 |  431 |  |
|        - |  432 | `#if defined(UNTRUST)` |
|        - |  433 | `	if( INVALID_HASH(pHash) ){` |
|        - |  434 | `		return 0;` |
|        - |  435 | `	}` |
|        - |  436 | `#endif` |
|        - |  437 | `	/* Last inserted entry */` |
|    67146 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|        2 |  439 |  |
|        - |  440 |  |
