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
|  5033384 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|        2 |   15 |  |
|  5033386 |   16 | `	pSet->nSize = 0 ;` |
|  5033386 |   17 | `	pSet->nUsed = 0;` |
|  5033386 |   18 | `	pSet->nCursor = 0;` |
|  5033386 |   19 | `	pSet->eSize = ElemSize;` |
|  5033386 |   20 | `	pSet->pAllocator = pAllocator;` |
|  5033386 |   21 | `	pSet->pBase =  0;` |
|  5033386 |   22 | `	pSet->pUserData = 0;` |
|  5033386 |   23 | `	return SXRET_OK;` |
|        2 |   24 |  |
|  8275516 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|        2 |   26 |  |
|        - |   27 | `	unsigned char *zbase;` |
|  8275518 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|        - |   29 | `		void *pNew;` |
|  1011110 |   30 | `		if( pSet->pAllocator == 0 ){` |
|      ! 0 |   31 | `			return  SXERR_LOCKED;` |
|        - |   32 | `		}` |
|  1011110 |   33 | `		if( pSet->nSize <= 0 ){` |
|   959178 |   34 | `			pSet->nSize = 4;` |
|   479588 |   35 | `		}` |
|  1011110 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|  1011110 |   37 | `		if( pNew == 0 ){` |
|      ! 0 |   38 | `			return SXERR_MEM;` |
|        - |   39 | `		}` |
|  1011110 |   40 | `		pSet->pBase = pNew;` |
|  1011110 |   41 | `		pSet->nSize <<= 1;` |
|   505554 |   42 | `	}` |
|  8275518 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 54953786 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  8275518 |   45 | `	pSet->nUsed++;` |
|  8275518 |   46 | `	return SXRET_OK;` |
|  4137782 |   47 |  |
|   373092 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|        2 |   49 |  |
|   373094 |   50 | `	if( pSet->nSize > 0 ){` |
|      ! 0 |   51 | `		return SXERR_LOCKED;` |
|        - |   52 | `	}` |
|   373094 |   53 | `	if( nItem < 8 ){` |
|      ! 0 |   54 | `		nItem = 8;` |
|      ! 0 |   55 | `	}` |
|   373094 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   373094 |   57 | `	if( pSet->pBase == 0 ){` |
|      ! 0 |   58 | `		return SXERR_MEM;` |
|        - |   59 | `	}` |
|   373094 |   60 | `	pSet->nSize = nItem;` |
|   373094 |   61 | `	return SXRET_OK;` |
|   186548 |   62 |  |
|   759104 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|        2 |   64 |  |
|   759106 |   65 | `	pSet->nUsed   = 0;` |
|   759106 |   66 | `	pSet->nCursor = 0;` |
|   759106 |   67 | `	return SXRET_OK;` |
|        2 |   68 |  |
|    31912 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|        2 |   70 |  |
|    31914 |   71 | `	pSet->nCursor = 0;` |
|    31914 |   72 | `	return SXRET_OK;` |
|        2 |   73 |  |
|    34878 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|        2 |   75 |  |
|        - |   76 | `	register unsigned char *zSrc;` |
|    34880 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|        - |   78 | `		/* Reset cursor */` |
|    12702 |   79 | `		pSet->nCursor = 0;` |
|    12702 |   80 | `		return SXERR_EOF;` |
|        - |   81 | `	}` |
|    22180 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|    22180 |   83 | `	if( ppEntry ){` |
|    22180 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|    11089 |   85 | `	}` |
|    22180 |   86 | `	pSet->nCursor++;` |
|    22180 |   87 | `	return SXRET_OK;` |
|    17441 |   88 |  |
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
|    46702 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|        2 |  101 |  |
|    46704 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       20 |  103 | `		pSet->nUsed = nNewSize;` |
|        9 |  104 | `	}` |
|    46704 |  105 | `	return SXRET_OK;` |
|        2 |  106 |  |
|  2225582 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|        2 |  108 |  |
|  2225584 |  109 | `	sxi32 rc = SXRET_OK;` |
|  2225584 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|  1195268 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   597633 |  112 | `	}` |
|  2225584 |  113 | `	pSet->pBase = 0;` |
|  2225584 |  114 | `	pSet->nUsed = 0;` |
|  2225584 |  115 | `	pSet->nCursor = 0;` |
|  2225584 |  116 | `	return rc;` |
|        2 |  117 |  |
|  1084434 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|        2 |  119 |  |
|        - |  120 | `	const char *zBase;` |
|  1084436 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       92 |  122 | `		return 0;` |
|        - |  123 | `	}` |
|  1084346 |  124 | `	zBase = (const char *)pSet->pBase;` |
|  1084346 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   542219 |  126 |  |
|   741810 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|        2 |  128 |  |
|        - |  129 | `	const char *zBase;` |
|        - |  130 | `	void *pData;` |
|   741812 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    79450 |  132 | `		return 0;` |
|        - |  133 | `	}` |
|   662364 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   662364 |  135 | `	pSet->nUsed--;` |
|   662364 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   662364 |  137 | `	return pData;` |
|   370907 |  138 |  |
|  5555642 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|        2 |  140 |  |
|        - |  141 | `	const char *zBase;` |
|  5555644 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|        - |  143 | `		/* Out of range */` |
|      ! 0 |  144 | `		return 0;` |
|        - |  145 | `	}` |
|  5555644 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  5555644 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|  2778011 |  148 |  |
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
|    66744 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|        2 |  163 |  |
|        - |  164 | `	SyHashEntry_Pr **apNew;` |
|        - |  165 | `#if defined(UNTRUST)` |
|        - |  166 | `	if( pHash == 0 ){` |
|        - |  167 | `		return SXERR_EMPTY;` |
|        - |  168 | `	}` |
|        - |  169 | `#endif` |
|        - |  170 | `	/* Allocate a new table */` |
|    66746 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    66746 |  172 | `	if( apNew == 0 ){` |
|      ! 0 |  173 | `		return SXERR_MEM;` |
|        - |  174 | `	}` |
|    66746 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    66746 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    66746 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    66746 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    66746 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    66746 |  180 | `	pHash->nEntry = 0;` |
|    66746 |  181 | `	pHash->apBucket = apNew;` |
|    66746 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    66746 |  183 | `	return SXRET_OK;` |
|    33374 |  184 |  |
|     8750 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|        2 |  186 |  |
|        - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|        - |  188 | `#if defined(UNTRUST)` |
|        - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|        - |  190 | `		return SXERR_EMPTY;` |
|        - |  191 | `	}` |
|        - |  192 | `#endif` |
|     8752 |  193 | `	pEntry = pHash->pList;` |
|     4862 |  194 | `	for(;;){` |
|     9726 |  195 | `		if( pHash->nEntry == 0 ){` |
|     8752 |  196 | `			break;` |
|        - |  197 | `		}` |
|      976 |  198 | `		pNext = pEntry->pNext;` |
|      976 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      976 |  200 | `		pEntry = pNext;` |
|      976 |  201 | `		pHash->nEntry--;` |
|        2 |  202 | `	}` |
|     8752 |  203 | `	if( pHash->apBucket ){` |
|     8752 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     4375 |  205 | `	}` |
|     8752 |  206 | `	pHash->apBucket = 0;` |
|     8752 |  207 | `	pHash->nBucketSize = 0;` |
|     8752 |  208 | `	pHash->pAllocator = 0;` |
|     8752 |  209 | `	return SXRET_OK;` |
|        2 |  210 |  |
|  6815160 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|        2 |  212 |  |
|        - |  213 | `	SyHashEntry_Pr *pEntry;` |
|        - |  214 | `	sxu32 nHash;` |
|        - |  215 |  |
|  6815162 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  6815162 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  5996021 |  218 | `	for(;;){` |
| 12070810 |  219 | `		if( pEntry == 0 ){` |
|  3677436 |  220 | `			break;` |
|        - |  221 | `		}` |
|  9962109 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|  3137730 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|  3137728 |  224 | `				return pEntry;` |
|        - |  225 | `		}` |
|  5255650 |  226 | `		pEntry = pEntry->pNextCollide;` |
|        2 |  227 | `	}` |
|        - |  228 | `	/* Entry not found */` |
|  3677436 |  229 | `	return 0;` |
|  3407846 |  230 |  |
|  6853110 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|        2 |  232 |  |
|        - |  233 | `	SyHashEntry_Pr *pEntry;` |
|        - |  234 | `#if defined(UNTRUST)` |
|        - |  235 | `	if( INVALID_HASH(pHash) ){` |
|        - |  236 | `		return 0;` |
|        - |  237 | `	}` |
|        - |  238 | `#endif` |
|  6853112 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|        - |  240 | `		/* Don't bother hashing,return immediately */` |
|    37958 |  241 | `		return 0;` |
|        - |  242 | `	}` |
|  6815156 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  6815156 |  244 | `	if( pEntry == 0 ){` |
|  3677436 |  245 | `		return 0;` |
|        - |  246 | `	}` |
|  3137722 |  247 | `	return (SyHashEntry *)pEntry;` |
|  3426821 |  248 |  |
|    57054 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|        2 |  250 |  |
|        - |  251 | `	sxi32 rc;` |
|    57056 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    42534 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|    21268 |  254 | `	}else{` |
|    14524 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|        - |  256 | `	}` |
|    57056 |  257 | `	if( pEntry->pNextCollide ){` |
|     3373 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     1686 |  259 | `	}` |
|    57056 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    57056 |  261 | `	pHash->nEntry--;` |
|    57056 |  262 | `	if( ppUserData ){` |
|        - |  263 | `		/* Write a pointer to the user data */` |
|      ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|      ! 0 |  265 | `	}` |
|        - |  266 | `	/* Release the entry */` |
|    57056 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    57056 |  268 | `	return rc;` |
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
|    57048 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|        2 |  287 |  |
|    57050 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|        - |  289 | `	sxi32 rc;` |
|        - |  290 | `#if defined(UNTRUST)` |
|        - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|        - |  292 | `		return SXERR_CORRUPT;` |
|        - |  293 | `	}` |
|        - |  294 | `#endif` |
|    57050 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    57050 |  296 | `	return rc;` |
|        2 |  297 |  |
|   100156 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|        2 |  299 |  |
|        - |  300 | `#if defined(UNTRUST)` |
|        - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|        - |  302 | `		return SXERR_CORRUPT;` |
|        - |  303 | `	}` |
|        - |  304 | `#endif` |
|   100158 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   100158 |  306 | `	return SXRET_OK;` |
|        2 |  307 |  |
|   699990 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|        2 |  309 |  |
|        - |  310 | `	SyHashEntry_Pr *pEntry;` |
|        - |  311 | `#if defined(UNTRUST)` |
|        - |  312 | `	if( INVALID_HASH(pHash) ){` |
|        - |  313 | `		return 0;` |
|        - |  314 | `	}` |
|        - |  315 | `#endif` |
|   699992 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    99724 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    99724 |  318 | `		return 0;` |
|        - |  319 | `	}` |
|   600270 |  320 | `	pEntry = pHash->pCurrent;` |
|        - |  321 | `	/* Advance the cursor */` |
|   600270 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|        - |  323 | `	/* Return the current entry */` |
|   600270 |  324 | `	return (SyHashEntry *)pEntry;` |
|   349997 |  325 |  |
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
|     9084 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|        2 |  349 |  |
|     9086 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|        - |  351 | `	SyHashEntry_Pr *pEntry;` |
|        - |  352 | `	SyHashEntry_Pr **apNew;` |
|        - |  353 | `	sxu32 n,iBucket;` |
|        - |  354 |  |
|        - |  355 | `	/* Allocate a new larger table */` |
|     9086 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     9086 |  357 | `	if( apNew == 0 ){` |
|        - |  358 | `		/* Not so fatal,simply a performance hit */` |
|      ! 0 |  359 | `		return SXRET_OK;` |
|        - |  360 | `	}` |
|        - |  361 | `	/* Zero the new table */` |
|     9086 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|        - |  363 | `	/* Rehash all entries */` |
|  1239998 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|  1230914 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - |  366 | `		/* Install in the new bucket */` |
|  1230914 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|  1230914 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|  1230914 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   591135 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|   295570 |  371 | `		}` |
|  1230914 |  372 | `		apNew[iBucket] = pEntry;` |
|        - |  373 | `		/* Point to the next entry */` |
|  1230914 |  374 | `		pEntry = pEntry->pNext;` |
|   615458 |  375 | `	}` |
|        - |  376 | `	/* Release the old table and reflect the change */` |
|     9086 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     9086 |  378 | `	pHash->apBucket = apNew;` |
|     9086 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     9086 |  380 | `	return SXRET_OK;` |
|     4544 |  381 |  |
|  1133410 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|        2 |  383 |  |
|  1133412 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|        - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|  1133412 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|  1133412 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   750464 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   375238 |  389 | `	}` |
|  1133412 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|        - |  391 | `	/* Link to the entry list */` |
|  1133412 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|  1133412 |  393 | `	if( pHash->nEntry == 0 ){` |
|    47784 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    23891 |  395 | `	}` |
|  1133412 |  396 | `	pHash->nEntry++;` |
|  1133412 |  397 | `	return SXRET_OK;` |
|        2 |  398 |  |
|  1133410 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|        2 |  400 |  |
|        - |  401 | `	SyHashEntry_Pr *pEntry;` |
|        - |  402 | `	sxi32 rc;` |
|        - |  403 | `#if defined(UNTRUST)` |
|        - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|        - |  405 | `		return SXERR_CORRUPT;` |
|        - |  406 | `	}` |
|        - |  407 | `#endif` |
|  1133412 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     9086 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     9086 |  410 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  411 | `			return rc;` |
|        - |  412 | `		}` |
|     4542 |  413 | `	}` |
|        - |  414 | `	/* Allocate a new hash entry */` |
|  1133412 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|  1133412 |  416 | `	if( pEntry == 0 ){` |
|      ! 0 |  417 | `		return SXERR_MEM;` |
|        - |  418 | `	}` |
|        - |  419 | `	/* Zero the entry */` |
|  1133412 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|  1133412 |  421 | `	pEntry->pHash = pHash;` |
|  1133412 |  422 | `	pEntry->pKey = pKey;` |
|  1133412 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|  1133412 |  424 | `	pEntry->pUserData = pUserData;` |
|  1133412 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|        - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|  1133412 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|  1133412 |  428 | `	return rc;` |
|   566707 |  429 |  |
|    68510 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|        2 |  431 |  |
|        - |  432 | `#if defined(UNTRUST)` |
|        - |  433 | `	if( INVALID_HASH(pHash) ){` |
|        - |  434 | `		return 0;` |
|        - |  435 | `	}` |
|        - |  436 | `#endif` |
|        - |  437 | `	/* Last inserted entry */` |
|    68512 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|        2 |  439 |  |
|        - |  440 |  |
