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
|  4972820 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|        2 |   15 |  |
|  4972822 |   16 | `	pSet->nSize = 0 ;` |
|  4972822 |   17 | `	pSet->nUsed = 0;` |
|  4972822 |   18 | `	pSet->nCursor = 0;` |
|  4972822 |   19 | `	pSet->eSize = ElemSize;` |
|  4972822 |   20 | `	pSet->pAllocator = pAllocator;` |
|  4972822 |   21 | `	pSet->pBase =  0;` |
|  4972822 |   22 | `	pSet->pUserData = 0;` |
|  4972822 |   23 | `	return SXRET_OK;` |
|        2 |   24 |  |
|  8175458 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|        2 |   26 |  |
|        - |   27 | `	unsigned char *zbase;` |
|  8175460 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|        - |   29 | `		void *pNew;` |
|  1001046 |   30 | `		if( pSet->pAllocator == 0 ){` |
|      ! 0 |   31 | `			return  SXERR_LOCKED;` |
|        - |   32 | `		}` |
|  1001046 |   33 | `		if( pSet->nSize <= 0 ){` |
|   949848 |   34 | `			pSet->nSize = 4;` |
|   474923 |   35 | `		}` |
|  1001046 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|  1001046 |   37 | `		if( pNew == 0 ){` |
|      ! 0 |   38 | `			return SXERR_MEM;` |
|        - |   39 | `		}` |
|  1001046 |   40 | `		pSet->pBase = pNew;` |
|  1001046 |   41 | `		pSet->nSize <<= 1;` |
|   500522 |   42 | `	}` |
|  8175460 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 54261288 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  8175460 |   45 | `	pSet->nUsed++;` |
|  8175460 |   46 | `	return SXRET_OK;` |
|  4087753 |   47 |  |
|   368232 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|        2 |   49 |  |
|   368234 |   50 | `	if( pSet->nSize > 0 ){` |
|      ! 0 |   51 | `		return SXERR_LOCKED;` |
|        - |   52 | `	}` |
|   368234 |   53 | `	if( nItem < 8 ){` |
|      ! 0 |   54 | `		nItem = 8;` |
|      ! 0 |   55 | `	}` |
|   368234 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   368234 |   57 | `	if( pSet->pBase == 0 ){` |
|      ! 0 |   58 | `		return SXERR_MEM;` |
|        - |   59 | `	}` |
|   368234 |   60 | `	pSet->nSize = nItem;` |
|   368234 |   61 | `	return SXRET_OK;` |
|   184118 |   62 |  |
|   750706 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|        2 |   64 |  |
|   750708 |   65 | `	pSet->nUsed   = 0;` |
|   750708 |   66 | `	pSet->nCursor = 0;` |
|   750708 |   67 | `	return SXRET_OK;` |
|        2 |   68 |  |
|    31684 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|        2 |   70 |  |
|    31686 |   71 | `	pSet->nCursor = 0;` |
|    31686 |   72 | `	return SXRET_OK;` |
|        2 |   73 |  |
|    34646 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|        2 |   75 |  |
|        - |   76 | `	register unsigned char *zSrc;` |
|    34648 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|        - |   78 | `		/* Reset cursor */` |
|    12590 |   79 | `		pSet->nCursor = 0;` |
|    12590 |   80 | `		return SXERR_EOF;` |
|        - |   81 | `	}` |
|    22060 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|    22060 |   83 | `	if( ppEntry ){` |
|    22060 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|    11029 |   85 | `	}` |
|    22060 |   86 | `	pSet->nCursor++;` |
|    22060 |   87 | `	return SXRET_OK;` |
|    17325 |   88 |  |
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
|    46022 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|        2 |  101 |  |
|    46024 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       20 |  103 | `		pSet->nUsed = nNewSize;` |
|        9 |  104 | `	}` |
|    46024 |  105 | `	return SXRET_OK;` |
|        2 |  106 |  |
|  2202530 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|        2 |  108 |  |
|  2202532 |  109 | `	sxi32 rc = SXRET_OK;` |
|  2202532 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|  1183190 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   591594 |  112 | `	}` |
|  2202532 |  113 | `	pSet->pBase = 0;` |
|  2202532 |  114 | `	pSet->nUsed = 0;` |
|  2202532 |  115 | `	pSet->nCursor = 0;` |
|  2202532 |  116 | `	return rc;` |
|        2 |  117 |  |
|  1070266 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|        2 |  119 |  |
|        - |  120 | `	const char *zBase;` |
|  1070268 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       92 |  122 | `		return 0;` |
|        - |  123 | `	}` |
|  1070178 |  124 | `	zBase = (const char *)pSet->pBase;` |
|  1070178 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   535135 |  126 |  |
|   735416 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|        2 |  128 |  |
|        - |  129 | `	const char *zBase;` |
|        - |  130 | `	void *pData;` |
|   735418 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    78794 |  132 | `		return 0;` |
|        - |  133 | `	}` |
|   656626 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   656626 |  135 | `	pSet->nUsed--;` |
|   656626 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   656626 |  137 | `	return pData;` |
|   367710 |  138 |  |
|  5482918 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|        2 |  140 |  |
|        - |  141 | `	const char *zBase;` |
|  5482920 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|        - |  143 | `		/* Out of range */` |
|      ! 0 |  144 | `		return 0;` |
|        - |  145 | `	}` |
|  5482920 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  5482920 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|  2741647 |  148 |  |
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
|    65768 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|        2 |  163 |  |
|        - |  164 | `	SyHashEntry_Pr **apNew;` |
|        - |  165 | `#if defined(UNTRUST)` |
|        - |  166 | `	if( pHash == 0 ){` |
|        - |  167 | `		return SXERR_EMPTY;` |
|        - |  168 | `	}` |
|        - |  169 | `#endif` |
|        - |  170 | `	/* Allocate a new table */` |
|    65770 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    65770 |  172 | `	if( apNew == 0 ){` |
|      ! 0 |  173 | `		return SXERR_MEM;` |
|        - |  174 | `	}` |
|    65770 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    65770 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    65770 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    65770 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    65770 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    65770 |  180 | `	pHash->nEntry = 0;` |
|    65770 |  181 | `	pHash->apBucket = apNew;` |
|    65770 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    65770 |  183 | `	return SXRET_OK;` |
|    32886 |  184 |  |
|     8614 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|        2 |  186 |  |
|        - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|        - |  188 | `#if defined(UNTRUST)` |
|        - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|        - |  190 | `		return SXERR_EMPTY;` |
|        - |  191 | `	}` |
|        - |  192 | `#endif` |
|     8616 |  193 | `	pEntry = pHash->pList;` |
|     4734 |  194 | `	for(;;){` |
|     9470 |  195 | `		if( pHash->nEntry == 0 ){` |
|     8616 |  196 | `			break;` |
|        - |  197 | `		}` |
|      856 |  198 | `		pNext = pEntry->pNext;` |
|      856 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      856 |  200 | `		pEntry = pNext;` |
|      856 |  201 | `		pHash->nEntry--;` |
|        2 |  202 | `	}` |
|     8616 |  203 | `	if( pHash->apBucket ){` |
|     8616 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     4307 |  205 | `	}` |
|     8616 |  206 | `	pHash->apBucket = 0;` |
|     8616 |  207 | `	pHash->nBucketSize = 0;` |
|     8616 |  208 | `	pHash->pAllocator = 0;` |
|     8616 |  209 | `	return SXRET_OK;` |
|        2 |  210 |  |
|  6698300 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|        2 |  212 |  |
|        - |  213 | `	SyHashEntry_Pr *pEntry;` |
|        - |  214 | `	sxu32 nHash;` |
|        - |  215 |  |
|  6698302 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  6698302 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  5952308 |  218 | `	for(;;){` |
| 11835596 |  219 | `		if( pEntry == 0 ){` |
|  3613948 |  220 | `			break;` |
|        - |  221 | `		}` |
|  9763697 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|  3084358 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|  3084356 |  224 | `				return pEntry;` |
|        - |  225 | `		}` |
|  5137296 |  226 | `		pEntry = pEntry->pNextCollide;` |
|        2 |  227 | `	}` |
|        - |  228 | `	/* Entry not found */` |
|  3613948 |  229 | `	return 0;` |
|  3349416 |  230 |  |
|  6735706 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|        2 |  232 |  |
|        - |  233 | `	SyHashEntry_Pr *pEntry;` |
|        - |  234 | `#if defined(UNTRUST)` |
|        - |  235 | `	if( INVALID_HASH(pHash) ){` |
|        - |  236 | `		return 0;` |
|        - |  237 | `	}` |
|        - |  238 | `#endif` |
|  6735708 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|        - |  240 | `		/* Don't bother hashing,return immediately */` |
|    37414 |  241 | `		return 0;` |
|        - |  242 | `	}` |
|  6698296 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  6698296 |  244 | `	if( pEntry == 0 ){` |
|  3613948 |  245 | `		return 0;` |
|        - |  246 | `	}` |
|  3084350 |  247 | `	return (SyHashEntry *)pEntry;` |
|  3368119 |  248 |  |
|    56422 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|        2 |  250 |  |
|        - |  251 | `	sxi32 rc;` |
|    56424 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    42036 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|    21019 |  254 | `	}else{` |
|    14390 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|        - |  256 | `	}` |
|    56424 |  257 | `	if( pEntry->pNextCollide ){` |
|     3469 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     1734 |  259 | `	}` |
|    56424 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    56424 |  261 | `	pHash->nEntry--;` |
|    56424 |  262 | `	if( ppUserData ){` |
|        - |  263 | `		/* Write a pointer to the user data */` |
|      ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|      ! 0 |  265 | `	}` |
|        - |  266 | `	/* Release the entry */` |
|    56424 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    56424 |  268 | `	return rc;` |
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
|    56416 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|        2 |  287 |  |
|    56418 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|        - |  289 | `	sxi32 rc;` |
|        - |  290 | `#if defined(UNTRUST)` |
|        - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|        - |  292 | `		return SXERR_CORRUPT;` |
|        - |  293 | `	}` |
|        - |  294 | `#endif` |
|    56418 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    56418 |  296 | `	return rc;` |
|        2 |  297 |  |
|    99100 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|        2 |  299 |  |
|        - |  300 | `#if defined(UNTRUST)` |
|        - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|        - |  302 | `		return SXERR_CORRUPT;` |
|        - |  303 | `	}` |
|        - |  304 | `#endif` |
|    99102 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    99102 |  306 | `	return SXRET_OK;` |
|        2 |  307 |  |
|   692502 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|        2 |  309 |  |
|        - |  310 | `	SyHashEntry_Pr *pEntry;` |
|        - |  311 | `#if defined(UNTRUST)` |
|        - |  312 | `	if( INVALID_HASH(pHash) ){` |
|        - |  313 | `		return 0;` |
|        - |  314 | `	}` |
|        - |  315 | `#endif` |
|   692504 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    98668 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    98668 |  318 | `		return 0;` |
|        - |  319 | `	}` |
|   593838 |  320 | `	pEntry = pHash->pCurrent;` |
|        - |  321 | `	/* Advance the cursor */` |
|   593838 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|        - |  323 | `	/* Return the current entry */` |
|   593838 |  324 | `	return (SyHashEntry *)pEntry;` |
|   346253 |  325 |  |
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
|     8924 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|        2 |  349 |  |
|     8926 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|        - |  351 | `	SyHashEntry_Pr *pEntry;` |
|        - |  352 | `	SyHashEntry_Pr **apNew;` |
|        - |  353 | `	sxu32 n,iBucket;` |
|        - |  354 |  |
|        - |  355 | `	/* Allocate a new larger table */` |
|     8926 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     8926 |  357 | `	if( apNew == 0 ){` |
|        - |  358 | `		/* Not so fatal,simply a performance hit */` |
|      ! 0 |  359 | `		return SXRET_OK;` |
|        - |  360 | `	}` |
|        - |  361 | `	/* Zero the new table */` |
|     8926 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|        - |  363 | `	/* Rehash all entries */` |
|  1217758 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|  1208834 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - |  366 | `		/* Install in the new bucket */` |
|  1208834 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|  1208834 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|  1208834 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   580534 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|   290268 |  371 | `		}` |
|  1208834 |  372 | `		apNew[iBucket] = pEntry;` |
|        - |  373 | `		/* Point to the next entry */` |
|  1208834 |  374 | `		pEntry = pEntry->pNext;` |
|   604418 |  375 | `	}` |
|        - |  376 | `	/* Release the old table and reflect the change */` |
|     8926 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     8926 |  378 | `	pHash->apBucket = apNew;` |
|     8926 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     8926 |  380 | `	return SXRET_OK;` |
|     4464 |  381 |  |
|  1114422 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|        2 |  383 |  |
|  1114424 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|        - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|  1114424 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|  1114424 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   737703 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   368862 |  389 | `	}` |
|  1114424 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|        - |  391 | `	/* Link to the entry list */` |
|  1114424 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|  1114424 |  393 | `	if( pHash->nEntry == 0 ){` |
|    47060 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    23529 |  395 | `	}` |
|  1114424 |  396 | `	pHash->nEntry++;` |
|  1114424 |  397 | `	return SXRET_OK;` |
|        2 |  398 |  |
|  1114422 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|        2 |  400 |  |
|        - |  401 | `	SyHashEntry_Pr *pEntry;` |
|        - |  402 | `	sxi32 rc;` |
|        - |  403 | `#if defined(UNTRUST)` |
|        - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|        - |  405 | `		return SXERR_CORRUPT;` |
|        - |  406 | `	}` |
|        - |  407 | `#endif` |
|  1114424 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     8926 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     8926 |  410 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  411 | `			return rc;` |
|        - |  412 | `		}` |
|     4462 |  413 | `	}` |
|        - |  414 | `	/* Allocate a new hash entry */` |
|  1114424 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|  1114424 |  416 | `	if( pEntry == 0 ){` |
|      ! 0 |  417 | `		return SXERR_MEM;` |
|        - |  418 | `	}` |
|        - |  419 | `	/* Zero the entry */` |
|  1114424 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|  1114424 |  421 | `	pEntry->pHash = pHash;` |
|  1114424 |  422 | `	pEntry->pKey = pKey;` |
|  1114424 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|  1114424 |  424 | `	pEntry->pUserData = pUserData;` |
|  1114424 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|        - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|  1114424 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|  1114424 |  428 | `	return rc;` |
|   557213 |  429 |  |
|    67670 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|        2 |  431 |  |
|        - |  432 | `#if defined(UNTRUST)` |
|        - |  433 | `	if( INVALID_HASH(pHash) ){` |
|        - |  434 | `		return 0;` |
|        - |  435 | `	}` |
|        - |  436 | `#endif` |
|        - |  437 | `	/* Last inserted entry */` |
|    67672 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|        2 |  439 |  |
|        - |  440 |  |
