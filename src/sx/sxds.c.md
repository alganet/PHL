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
|  4767940 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|        2 |   15 |  |
|  4767942 |   16 | `	pSet->nSize = 0 ;` |
|  4767942 |   17 | `	pSet->nUsed = 0;` |
|  4767942 |   18 | `	pSet->nCursor = 0;` |
|  4767942 |   19 | `	pSet->eSize = ElemSize;` |
|  4767942 |   20 | `	pSet->pAllocator = pAllocator;` |
|  4767942 |   21 | `	pSet->pBase =  0;` |
|  4767942 |   22 | `	pSet->pUserData = 0;` |
|  4767942 |   23 | `	return SXRET_OK;` |
|        2 |   24 |  |
|  7819334 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|        2 |   26 |  |
|        - |   27 | `	unsigned char *zbase;` |
|  7819336 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|        - |   29 | `		void *pNew;` |
|   967818 |   30 | `		if( pSet->pAllocator == 0 ){` |
|      ! 0 |   31 | `			return  SXERR_LOCKED;` |
|        - |   32 | `		}` |
|   967818 |   33 | `		if( pSet->nSize <= 0 ){` |
|   919120 |   34 | `			pSet->nSize = 4;` |
|   459559 |   35 | `		}` |
|   967818 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   967818 |   37 | `		if( pNew == 0 ){` |
|      ! 0 |   38 | `			return SXERR_MEM;` |
|        - |   39 | `		}` |
|   967818 |   40 | `		pSet->pBase = pNew;` |
|   967818 |   41 | `		pSet->nSize <<= 1;` |
|   483908 |   42 | `	}` |
|  7819336 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 51747984 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  7819336 |   45 | `	pSet->nUsed++;` |
|  7819336 |   46 | `	return SXRET_OK;` |
|  3909691 |   47 |  |
|   350524 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|        2 |   49 |  |
|   350526 |   50 | `	if( pSet->nSize > 0 ){` |
|      ! 0 |   51 | `		return SXERR_LOCKED;` |
|        - |   52 | `	}` |
|   350526 |   53 | `	if( nItem < 8 ){` |
|      ! 0 |   54 | `		nItem = 8;` |
|      ! 0 |   55 | `	}` |
|   350526 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   350526 |   57 | `	if( pSet->pBase == 0 ){` |
|      ! 0 |   58 | `		return SXERR_MEM;` |
|        - |   59 | `	}` |
|   350526 |   60 | `	pSet->nSize = nItem;` |
|   350526 |   61 | `	return SXRET_OK;` |
|   175264 |   62 |  |
|   725696 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|        2 |   64 |  |
|   725698 |   65 | `	pSet->nUsed   = 0;` |
|   725698 |   66 | `	pSet->nCursor = 0;` |
|   725698 |   67 | `	return SXRET_OK;` |
|        2 |   68 |  |
|    31118 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|        2 |   70 |  |
|    31120 |   71 | `	pSet->nCursor = 0;` |
|    31120 |   72 | `	return SXRET_OK;` |
|        2 |   73 |  |
|    34026 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|        2 |   75 |  |
|        - |   76 | `	register unsigned char *zSrc;` |
|    34028 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|        - |   78 | `		/* Reset cursor */` |
|    12334 |   79 | `		pSet->nCursor = 0;` |
|    12334 |   80 | `		return SXERR_EOF;` |
|        - |   81 | `	}` |
|    21696 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|    21696 |   83 | `	if( ppEntry ){` |
|    21696 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|    10847 |   85 | `	}` |
|    21696 |   86 | `	pSet->nCursor++;` |
|    21696 |   87 | `	return SXRET_OK;` |
|    17015 |   88 |  |
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
|    43574 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|        2 |  101 |  |
|    43576 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       20 |  103 | `		pSet->nUsed = nNewSize;` |
|        9 |  104 | `	}` |
|    43576 |  105 | `	return SXRET_OK;` |
|        2 |  106 |  |
|  2123496 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|        2 |  108 |  |
|  2123498 |  109 | `	sxi32 rc = SXRET_OK;` |
|  2123498 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|  1142184 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   571091 |  112 | `	}` |
|  2123498 |  113 | `	pSet->pBase = 0;` |
|  2123498 |  114 | `	pSet->nUsed = 0;` |
|  2123498 |  115 | `	pSet->nCursor = 0;` |
|  2123498 |  116 | `	return rc;` |
|        2 |  117 |  |
|  1019932 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|        2 |  119 |  |
|        - |  120 | `	const char *zBase;` |
|  1019934 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       92 |  122 | `		return 0;` |
|        - |  123 | `	}` |
|  1019844 |  124 | `	zBase = (const char *)pSet->pBase;` |
|  1019844 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   509968 |  126 |  |
|   714678 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|        2 |  128 |  |
|        - |  129 | `	const char *zBase;` |
|        - |  130 | `	void *pData;` |
|   714680 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    77214 |  132 | `		return 0;` |
|        - |  133 | `	}` |
|   637468 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   637468 |  135 | `	pSet->nUsed--;` |
|   637468 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   637468 |  137 | `	return pData;` |
|   357341 |  138 |  |
|  5313177 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|        2 |  140 |  |
|        - |  141 | `	const char *zBase;` |
|  5313179 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|        - |  143 | `		/* Out of range */` |
|      ! 0 |  144 | `		return 0;` |
|        - |  145 | `	}` |
|  5313179 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  5313179 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|  2656768 |  148 |  |
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
|    58652 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|        2 |  163 |  |
|        - |  164 | `	SyHashEntry_Pr **apNew;` |
|        - |  165 | `#if defined(UNTRUST)` |
|        - |  166 | `	if( pHash == 0 ){` |
|        - |  167 | `		return SXERR_EMPTY;` |
|        - |  168 | `	}` |
|        - |  169 | `#endif` |
|        - |  170 | `	/* Allocate a new table */` |
|    58654 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    58654 |  172 | `	if( apNew == 0 ){` |
|      ! 0 |  173 | `		return SXERR_MEM;` |
|        - |  174 | `	}` |
|    58654 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    58654 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    58654 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    58654 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    58654 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    58654 |  180 | `	pHash->nEntry = 0;` |
|    58654 |  181 | `	pHash->apBucket = apNew;` |
|    58654 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    58654 |  183 | `	return SXRET_OK;` |
|    29328 |  184 |  |
|     8320 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|        2 |  186 |  |
|        - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|        - |  188 | `#if defined(UNTRUST)` |
|        - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|        - |  190 | `		return SXERR_EMPTY;` |
|        - |  191 | `	}` |
|        - |  192 | `#endif` |
|     8322 |  193 | `	pEntry = pHash->pList;` |
|     4479 |  194 | `	for(;;){` |
|     8960 |  195 | `		if( pHash->nEntry == 0 ){` |
|     8322 |  196 | `			break;` |
|        - |  197 | `		}` |
|      640 |  198 | `		pNext = pEntry->pNext;` |
|      640 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      640 |  200 | `		pEntry = pNext;` |
|      640 |  201 | `		pHash->nEntry--;` |
|        2 |  202 | `	}` |
|     8322 |  203 | `	if( pHash->apBucket ){` |
|     8322 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     4160 |  205 | `	}` |
|     8322 |  206 | `	pHash->apBucket = 0;` |
|     8322 |  207 | `	pHash->nBucketSize = 0;` |
|     8322 |  208 | `	pHash->pAllocator = 0;` |
|     8322 |  209 | `	return SXRET_OK;` |
|        2 |  210 |  |
|  6376306 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|        2 |  212 |  |
|        - |  213 | `	SyHashEntry_Pr *pEntry;` |
|        - |  214 | `	sxu32 nHash;` |
|        - |  215 |  |
|  6376308 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  6376308 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  5683495 |  218 | `	for(;;){` |
| 11364002 |  219 | `		if( pEntry == 0 ){` |
|  3438512 |  220 | `			break;` |
|        - |  221 | `		}` |
|  9394260 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|  2937800 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|  2937798 |  224 | `				return pEntry;` |
|        - |  225 | `		}` |
|  4987696 |  226 | `		pEntry = pEntry->pNextCollide;` |
|        2 |  227 | `	}` |
|        - |  228 | `	/* Entry not found */` |
|  3438512 |  229 | `	return 0;` |
|  3188419 |  230 |  |
|  6409398 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|        2 |  232 |  |
|        - |  233 | `	SyHashEntry_Pr *pEntry;` |
|        - |  234 | `#if defined(UNTRUST)` |
|        - |  235 | `	if( INVALID_HASH(pHash) ){` |
|        - |  236 | `		return 0;` |
|        - |  237 | `	}` |
|        - |  238 | `#endif` |
|  6409400 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|        - |  240 | `		/* Don't bother hashing,return immediately */` |
|    33100 |  241 | `		return 0;` |
|        - |  242 | `	}` |
|  6376302 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  6376302 |  244 | `	if( pEntry == 0 ){` |
|  3438512 |  245 | `		return 0;` |
|        - |  246 | `	}` |
|  2937792 |  247 | `	return (SyHashEntry *)pEntry;` |
|  3204965 |  248 |  |
|    54950 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|        2 |  250 |  |
|        - |  251 | `	sxi32 rc;` |
|    54952 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    40864 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|    20433 |  254 | `	}else{` |
|    14090 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|        - |  256 | `	}` |
|    54952 |  257 | `	if( pEntry->pNextCollide ){` |
|     3429 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     1714 |  259 | `	}` |
|    54952 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    54952 |  261 | `	pHash->nEntry--;` |
|    54952 |  262 | `	if( ppUserData ){` |
|        - |  263 | `		/* Write a pointer to the user data */` |
|      ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|      ! 0 |  265 | `	}` |
|        - |  266 | `	/* Release the entry */` |
|    54952 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    54952 |  268 | `	return rc;` |
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
|    54944 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|        2 |  287 |  |
|    54946 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|        - |  289 | `	sxi32 rc;` |
|        - |  290 | `#if defined(UNTRUST)` |
|        - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|        - |  292 | `		return SXERR_CORRUPT;` |
|        - |  293 | `	}` |
|        - |  294 | `#endif` |
|    54946 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    54946 |  296 | `	return rc;` |
|        2 |  297 |  |
|    88136 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|        2 |  299 |  |
|        - |  300 | `#if defined(UNTRUST)` |
|        - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|        - |  302 | `		return SXERR_CORRUPT;` |
|        - |  303 | `	}` |
|        - |  304 | `#endif` |
|    88138 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    88138 |  306 | `	return SXRET_OK;` |
|        2 |  307 |  |
|   596550 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|        2 |  309 |  |
|        - |  310 | `	SyHashEntry_Pr *pEntry;` |
|        - |  311 | `#if defined(UNTRUST)` |
|        - |  312 | `	if( INVALID_HASH(pHash) ){` |
|        - |  313 | `		return 0;` |
|        - |  314 | `	}` |
|        - |  315 | `#endif` |
|   596552 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    87704 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    87704 |  318 | `		return 0;` |
|        - |  319 | `	}` |
|   508850 |  320 | `	pEntry = pHash->pCurrent;` |
|        - |  321 | `	/* Advance the cursor */` |
|   508850 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|        - |  323 | `	/* Return the current entry */` |
|   508850 |  324 | `	return (SyHashEntry *)pEntry;` |
|   298277 |  325 |  |
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
|     8348 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|        2 |  349 |  |
|     8350 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|        - |  351 | `	SyHashEntry_Pr *pEntry;` |
|        - |  352 | `	SyHashEntry_Pr **apNew;` |
|        - |  353 | `	sxu32 n,iBucket;` |
|        - |  354 |  |
|        - |  355 | `	/* Allocate a new larger table */` |
|     8350 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     8350 |  357 | `	if( apNew == 0 ){` |
|        - |  358 | `		/* Not so fatal,simply a performance hit */` |
|      ! 0 |  359 | `		return SXRET_OK;` |
|        - |  360 | `	}` |
|        - |  361 | `	/* Zero the new table */` |
|     8350 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|        - |  363 | `	/* Rehash all entries */` |
|  1137694 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|  1129346 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - |  366 | `		/* Install in the new bucket */` |
|  1129346 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|  1129346 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|  1129346 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   542377 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|   271192 |  371 | `		}` |
|  1129346 |  372 | `		apNew[iBucket] = pEntry;` |
|        - |  373 | `		/* Point to the next entry */` |
|  1129346 |  374 | `		pEntry = pEntry->pNext;` |
|   564674 |  375 | `	}` |
|        - |  376 | `	/* Release the old table and reflect the change */` |
|     8350 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     8350 |  378 | `	pHash->apBucket = apNew;` |
|     8350 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     8350 |  380 | `	return SXRET_OK;` |
|     4176 |  381 |  |
|  1025660 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|        2 |  383 |  |
|  1025662 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|        - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|  1025662 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|  1025662 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   688809 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   344373 |  389 | `	}` |
|  1025662 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|        - |  391 | `	/* Link to the entry list */` |
|  1025662 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|  1025662 |  393 | `	if( pHash->nEntry == 0 ){` |
|    42148 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    21073 |  395 | `	}` |
|  1025662 |  396 | `	pHash->nEntry++;` |
|  1025662 |  397 | `	return SXRET_OK;` |
|        2 |  398 |  |
|  1025660 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|        2 |  400 |  |
|        - |  401 | `	SyHashEntry_Pr *pEntry;` |
|        - |  402 | `	sxi32 rc;` |
|        - |  403 | `#if defined(UNTRUST)` |
|        - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|        - |  405 | `		return SXERR_CORRUPT;` |
|        - |  406 | `	}` |
|        - |  407 | `#endif` |
|  1025662 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     8350 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     8350 |  410 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  411 | `			return rc;` |
|        - |  412 | `		}` |
|     4174 |  413 | `	}` |
|        - |  414 | `	/* Allocate a new hash entry */` |
|  1025662 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|  1025662 |  416 | `	if( pEntry == 0 ){` |
|      ! 0 |  417 | `		return SXERR_MEM;` |
|        - |  418 | `	}` |
|        - |  419 | `	/* Zero the entry */` |
|  1025662 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|  1025662 |  421 | `	pEntry->pHash = pHash;` |
|  1025662 |  422 | `	pEntry->pKey = pKey;` |
|  1025662 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|  1025662 |  424 | `	pEntry->pUserData = pUserData;` |
|  1025662 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|        - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|  1025662 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|  1025662 |  428 | `	return rc;` |
|   512832 |  429 |  |
|    65474 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|        2 |  431 |  |
|        - |  432 | `#if defined(UNTRUST)` |
|        - |  433 | `	if( INVALID_HASH(pHash) ){` |
|        - |  434 | `		return 0;` |
|        - |  435 | `	}` |
|        - |  436 | `#endif` |
|        - |  437 | `	/* Last inserted entry */` |
|    65476 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|        2 |  439 |  |
|        - |  440 |  |
