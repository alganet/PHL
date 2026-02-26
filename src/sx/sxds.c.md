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
|  5260880 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|        2 |   15 |  |
|  5260882 |   16 | `	pSet->nSize = 0 ;` |
|  5260882 |   17 | `	pSet->nUsed = 0;` |
|  5260882 |   18 | `	pSet->nCursor = 0;` |
|  5260882 |   19 | `	pSet->eSize = ElemSize;` |
|  5260882 |   20 | `	pSet->pAllocator = pAllocator;` |
|  5260882 |   21 | `	pSet->pBase =  0;` |
|  5260882 |   22 | `	pSet->pUserData = 0;` |
|  5260882 |   23 | `	return SXRET_OK;` |
|        2 |   24 |  |
|  8675192 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|        2 |   26 |  |
|        - |   27 | `	unsigned char *zbase;` |
|  8675194 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|        - |   29 | `		void *pNew;` |
|  1046844 |   30 | `		if( pSet->pAllocator == 0 ){` |
|      ! 0 |   31 | `			return  SXERR_LOCKED;` |
|        - |   32 | `		}` |
|  1046844 |   33 | `		if( pSet->nSize <= 0 ){` |
|   992020 |   34 | `			pSet->nSize = 4;` |
|   496009 |   35 | `		}` |
|  1046844 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|  1046844 |   37 | `		if( pNew == 0 ){` |
|      ! 0 |   38 | `			return SXERR_MEM;` |
|        - |   39 | `		}` |
|  1046844 |   40 | `		pSet->pBase = pNew;` |
|  1046844 |   41 | `		pSet->nSize <<= 1;` |
|   523421 |   42 | `	}` |
|  8675194 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 57782426 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  8675194 |   45 | `	pSet->nUsed++;` |
|  8675194 |   46 | `	return SXRET_OK;` |
|  4337620 |   47 |  |
|   393842 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|        2 |   49 |  |
|   393844 |   50 | `	if( pSet->nSize > 0 ){` |
|      ! 0 |   51 | `		return SXERR_LOCKED;` |
|        - |   52 | `	}` |
|   393844 |   53 | `	if( nItem < 8 ){` |
|      ! 0 |   54 | `		nItem = 8;` |
|      ! 0 |   55 | `	}` |
|   393844 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   393844 |   57 | `	if( pSet->pBase == 0 ){` |
|      ! 0 |   58 | `		return SXERR_MEM;` |
|        - |   59 | `	}` |
|   393844 |   60 | `	pSet->nSize = nItem;` |
|   393844 |   61 | `	return SXRET_OK;` |
|   196923 |   62 |  |
|   786842 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|        2 |   64 |  |
|   786844 |   65 | `	pSet->nUsed   = 0;` |
|   786844 |   66 | `	pSet->nCursor = 0;` |
|   786844 |   67 | `	return SXRET_OK;` |
|        2 |   68 |  |
|    32460 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|        2 |   70 |  |
|    32462 |   71 | `	pSet->nCursor = 0;` |
|    32462 |   72 | `	return SXRET_OK;` |
|        2 |   73 |  |
|    35494 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|        2 |   75 |  |
|        - |   76 | `	register unsigned char *zSrc;` |
|    35496 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|        - |   78 | `		/* Reset cursor */` |
|    12942 |   79 | `		pSet->nCursor = 0;` |
|    12942 |   80 | `		return SXERR_EOF;` |
|        - |   81 | `	}` |
|    22556 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|    22556 |   83 | `	if( ppEntry ){` |
|    22556 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|    11277 |   85 | `	}` |
|    22556 |   86 | `	pSet->nCursor++;` |
|    22556 |   87 | `	return SXRET_OK;` |
|    17749 |   88 |  |
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
|    49566 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|        2 |  101 |  |
|    49568 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       20 |  103 | `		pSet->nUsed = nNewSize;` |
|        9 |  104 | `	}` |
|    49568 |  105 | `	return SXRET_OK;` |
|        2 |  106 |  |
|  2310468 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|        2 |  108 |  |
|  2310470 |  109 | `	sxi32 rc = SXRET_OK;` |
|  2310470 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|  1240236 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   620117 |  112 | `	}` |
|  2310470 |  113 | `	pSet->pBase = 0;` |
|  2310470 |  114 | `	pSet->nUsed = 0;` |
|  2310470 |  115 | `	pSet->nCursor = 0;` |
|  2310470 |  116 | `	return rc;` |
|        2 |  117 |  |
|  1143082 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|        2 |  119 |  |
|        - |  120 | `	const char *zBase;` |
|  1143084 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       92 |  122 | `		return 0;` |
|        - |  123 | `	}` |
|  1142994 |  124 | `	zBase = (const char *)pSet->pBase;` |
|  1142994 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   571543 |  126 |  |
|   763088 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|        2 |  128 |  |
|        - |  129 | `	const char *zBase;` |
|        - |  130 | `	void *pData;` |
|   763090 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    81132 |  132 | `		return 0;` |
|        - |  133 | `	}` |
|   681960 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   681960 |  135 | `	pSet->nUsed--;` |
|   681960 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   681960 |  137 | `	return pData;` |
|   381546 |  138 |  |
|  5721603 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|        2 |  140 |  |
|        - |  141 | `	const char *zBase;` |
|  5721605 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|        - |  143 | `		/* Out of range */` |
|      ! 0 |  144 | `		return 0;` |
|        - |  145 | `	}` |
|  5721605 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  5721605 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|  2860998 |  148 |  |
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
|    70580 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|        2 |  163 |  |
|        - |  164 | `	SyHashEntry_Pr **apNew;` |
|        - |  165 | `#if defined(UNTRUST)` |
|        - |  166 | `	if( pHash == 0 ){` |
|        - |  167 | `		return SXERR_EMPTY;` |
|        - |  168 | `	}` |
|        - |  169 | `#endif` |
|        - |  170 | `	/* Allocate a new table */` |
|    70582 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    70582 |  172 | `	if( apNew == 0 ){` |
|      ! 0 |  173 | `		return SXERR_MEM;` |
|        - |  174 | `	}` |
|    70582 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    70582 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    70582 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    70582 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    70582 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    70582 |  180 | `	pHash->nEntry = 0;` |
|    70582 |  181 | `	pHash->apBucket = apNew;` |
|    70582 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    70582 |  183 | `	return SXRET_OK;` |
|    35292 |  184 |  |
|     9058 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|        2 |  186 |  |
|        - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|        - |  188 | `#if defined(UNTRUST)` |
|        - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|        - |  190 | `		return SXERR_EMPTY;` |
|        - |  191 | `	}` |
|        - |  192 | `#endif` |
|     9060 |  193 | `	pEntry = pHash->pList;` |
|     5112 |  194 | `	for(;;){` |
|    10226 |  195 | `		if( pHash->nEntry == 0 ){` |
|     9060 |  196 | `			break;` |
|        - |  197 | `		}` |
|     1168 |  198 | `		pNext = pEntry->pNext;` |
|     1168 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     1168 |  200 | `		pEntry = pNext;` |
|     1168 |  201 | `		pHash->nEntry--;` |
|        2 |  202 | `	}` |
|     9060 |  203 | `	if( pHash->apBucket ){` |
|     9060 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     4529 |  205 | `	}` |
|     9060 |  206 | `	pHash->apBucket = 0;` |
|     9060 |  207 | `	pHash->nBucketSize = 0;` |
|     9060 |  208 | `	pHash->pAllocator = 0;` |
|     9060 |  209 | `	return SXRET_OK;` |
|        2 |  210 |  |
|  7103358 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|        2 |  212 |  |
|        - |  213 | `	SyHashEntry_Pr *pEntry;` |
|        - |  214 | `	sxu32 nHash;` |
|        - |  215 |  |
|  7103360 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  7103360 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  6195307 |  218 | `	for(;;){` |
| 12446327 |  219 | `		if( pEntry == 0 ){` |
|  3841866 |  220 | `			break;` |
|        - |  221 | `		}` |
| 10235080 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|  3261498 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|  3261496 |  224 | `				return pEntry;` |
|        - |  225 | `		}` |
|  5342969 |  226 | `		pEntry = pEntry->pNextCollide;` |
|        2 |  227 | `	}` |
|        - |  228 | `	/* Entry not found */` |
|  3841866 |  229 | `	return 0;` |
|  3551945 |  230 |  |
|  7143356 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|        2 |  232 |  |
|        - |  233 | `	SyHashEntry_Pr *pEntry;` |
|        - |  234 | `#if defined(UNTRUST)` |
|        - |  235 | `	if( INVALID_HASH(pHash) ){` |
|        - |  236 | `		return 0;` |
|        - |  237 | `	}` |
|        - |  238 | `#endif` |
|  7143358 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|        - |  240 | `		/* Don't bother hashing,return immediately */` |
|    40006 |  241 | `		return 0;` |
|        - |  242 | `	}` |
|  7103354 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  7103354 |  244 | `	if( pEntry == 0 ){` |
|  3841866 |  245 | `		return 0;` |
|        - |  246 | `	}` |
|  3261490 |  247 | `	return (SyHashEntry *)pEntry;` |
|  3571944 |  248 |  |
|    58656 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|        2 |  250 |  |
|        - |  251 | `	sxi32 rc;` |
|    58658 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    43842 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|    21922 |  254 | `	}else{` |
|    14818 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|        - |  256 | `	}` |
|    58658 |  257 | `	if( pEntry->pNextCollide ){` |
|     3553 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     1776 |  259 | `	}` |
|    58658 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    58658 |  261 | `	pHash->nEntry--;` |
|    58658 |  262 | `	if( ppUserData ){` |
|        - |  263 | `		/* Write a pointer to the user data */` |
|      ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|      ! 0 |  265 | `	}` |
|        - |  266 | `	/* Release the entry */` |
|    58658 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    58658 |  268 | `	return rc;` |
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
|    58650 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|        2 |  287 |  |
|    58652 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|        - |  289 | `	sxi32 rc;` |
|        - |  290 | `#if defined(UNTRUST)` |
|        - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|        - |  292 | `		return SXERR_CORRUPT;` |
|        - |  293 | `	}` |
|        - |  294 | `#endif` |
|    58652 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    58652 |  296 | `	return rc;` |
|        2 |  297 |  |
|   104692 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|        2 |  299 |  |
|        - |  300 | `#if defined(UNTRUST)` |
|        - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|        - |  302 | `		return SXERR_CORRUPT;` |
|        - |  303 | `	}` |
|        - |  304 | `#endif` |
|   104694 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   104694 |  306 | `	return SXRET_OK;` |
|        2 |  307 |  |
|   731526 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|        2 |  309 |  |
|        - |  310 | `	SyHashEntry_Pr *pEntry;` |
|        - |  311 | `#if defined(UNTRUST)` |
|        - |  312 | `	if( INVALID_HASH(pHash) ){` |
|        - |  313 | `		return 0;` |
|        - |  314 | `	}` |
|        - |  315 | `#endif` |
|   731528 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   104260 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   104260 |  318 | `		return 0;` |
|        - |  319 | `	}` |
|   627270 |  320 | `	pEntry = pHash->pCurrent;` |
|        - |  321 | `	/* Advance the cursor */` |
|   627270 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|        - |  323 | `	/* Return the current entry */` |
|   627270 |  324 | `	return (SyHashEntry *)pEntry;` |
|   365765 |  325 |  |
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
|     1577 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|        - |  338 | `		/* Invoke the callback */` |
|     1567 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|     1567 |  340 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  341 | `			return rc;` |
|        - |  342 | `		}` |
|        - |  343 | `		/* Point to the next entry */` |
|     1567 |  344 | `		pEntry = pEntry->pNext;` |
|      784 |  345 | `	}` |
|       11 |  346 | `	return SXRET_OK;` |
|        6 |  347 |  |
|     9756 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|        2 |  349 |  |
|     9758 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|        - |  351 | `	SyHashEntry_Pr *pEntry;` |
|        - |  352 | `	SyHashEntry_Pr **apNew;` |
|        - |  353 | `	sxu32 n,iBucket;` |
|        - |  354 |  |
|        - |  355 | `	/* Allocate a new larger table */` |
|     9758 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     9758 |  357 | `	if( apNew == 0 ){` |
|        - |  358 | `		/* Not so fatal,simply a performance hit */` |
|      ! 0 |  359 | `		return SXRET_OK;` |
|        - |  360 | `	}` |
|        - |  361 | `	/* Zero the new table */` |
|     9758 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|        - |  363 | `	/* Rehash all entries */` |
|  1333406 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|  1323650 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - |  366 | `		/* Install in the new bucket */` |
|  1323650 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|  1323650 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|  1323650 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   635660 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|   317823 |  371 | `		}` |
|  1323650 |  372 | `		apNew[iBucket] = pEntry;` |
|        - |  373 | `		/* Point to the next entry */` |
|  1323650 |  374 | `		pEntry = pEntry->pNext;` |
|   661826 |  375 | `	}` |
|        - |  376 | `	/* Release the old table and reflect the change */` |
|     9758 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     9758 |  378 | `	pHash->apBucket = apNew;` |
|     9758 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     9758 |  380 | `	return SXRET_OK;` |
|     4880 |  381 |  |
|  1211718 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|        2 |  383 |  |
|  1211720 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|        - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|  1211720 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|  1211720 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   804635 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   402332 |  389 | `	}` |
|  1211720 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|        - |  391 | `	/* Link to the entry list */` |
|  1211720 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|  1211720 |  393 | `	if( pHash->nEntry == 0 ){` |
|    50528 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    25263 |  395 | `	}` |
|  1211720 |  396 | `	pHash->nEntry++;` |
|  1211720 |  397 | `	return SXRET_OK;` |
|        2 |  398 |  |
|  1211718 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|        2 |  400 |  |
|        - |  401 | `	SyHashEntry_Pr *pEntry;` |
|        - |  402 | `	sxi32 rc;` |
|        - |  403 | `#if defined(UNTRUST)` |
|        - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|        - |  405 | `		return SXERR_CORRUPT;` |
|        - |  406 | `	}` |
|        - |  407 | `#endif` |
|  1211720 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     9758 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     9758 |  410 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  411 | `			return rc;` |
|        - |  412 | `		}` |
|     4878 |  413 | `	}` |
|        - |  414 | `	/* Allocate a new hash entry */` |
|  1211720 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|  1211720 |  416 | `	if( pEntry == 0 ){` |
|      ! 0 |  417 | `		return SXERR_MEM;` |
|        - |  418 | `	}` |
|        - |  419 | `	/* Zero the entry */` |
|  1211720 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|  1211720 |  421 | `	pEntry->pHash = pHash;` |
|  1211720 |  422 | `	pEntry->pKey = pKey;` |
|  1211720 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|  1211720 |  424 | `	pEntry->pUserData = pUserData;` |
|  1211720 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|        - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|  1211720 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|  1211720 |  428 | `	return rc;` |
|   605861 |  429 |  |
|    70952 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|        2 |  431 |  |
|        - |  432 | `#if defined(UNTRUST)` |
|        - |  433 | `	if( INVALID_HASH(pHash) ){` |
|        - |  434 | `		return 0;` |
|        - |  435 | `	}` |
|        - |  436 | `#endif` |
|        - |  437 | `	/* Last inserted entry */` |
|    70954 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|        2 |  439 |  |
|        - |  440 |  |
