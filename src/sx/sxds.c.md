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
|  4285222 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|        2 |   15 |  |
|  4285224 |   16 | `	pSet->nSize = 0 ;` |
|  4285224 |   17 | `	pSet->nUsed = 0;` |
|  4285224 |   18 | `	pSet->nCursor = 0;` |
|  4285224 |   19 | `	pSet->eSize = ElemSize;` |
|  4285224 |   20 | `	pSet->pAllocator = pAllocator;` |
|  4285224 |   21 | `	pSet->pBase =  0;` |
|  4285224 |   22 | `	pSet->pUserData = 0;` |
|  4285224 |   23 | `	return SXRET_OK;` |
|        2 |   24 |  |
|  7172663 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|        2 |   26 |  |
|        - |   27 | `	unsigned char *zbase;` |
|  7172665 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|        - |   29 | `		void *pNew;` |
|   883566 |   30 | `		if( pSet->pAllocator == 0 ){` |
|      ! 0 |   31 | `			return  SXERR_LOCKED;` |
|        - |   32 | `		}` |
|   883566 |   33 | `		if( pSet->nSize <= 0 ){` |
|   838518 |   34 | `			pSet->nSize = 4;` |
|   419258 |   35 | `		}` |
|   883566 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   883566 |   37 | `		if( pNew == 0 ){` |
|      ! 0 |   38 | `			return SXERR_MEM;` |
|        - |   39 | `		}` |
|   883566 |   40 | `		pSet->pBase = pNew;` |
|   883566 |   41 | `		pSet->nSize <<= 1;` |
|   441782 |   42 | `	}` |
|  7172665 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 47904895 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  7172665 |   45 | `	pSet->nUsed++;` |
|  7172665 |   46 | `	return SXRET_OK;` |
|  3586335 |   47 |  |
|   330238 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|        2 |   49 |  |
|   330240 |   50 | `	if( pSet->nSize > 0 ){` |
|      ! 0 |   51 | `		return SXERR_LOCKED;` |
|        - |   52 | `	}` |
|   330240 |   53 | `	if( nItem < 8 ){` |
|      ! 0 |   54 | `		nItem = 8;` |
|      ! 0 |   55 | `	}` |
|   330240 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   330240 |   57 | `	if( pSet->pBase == 0 ){` |
|      ! 0 |   58 | `		return SXERR_MEM;` |
|        - |   59 | `	}` |
|   330240 |   60 | `	pSet->nSize = nItem;` |
|   330240 |   61 | `	return SXRET_OK;` |
|   165121 |   62 |  |
|   613997 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|        2 |   64 |  |
|   613999 |   65 | `	pSet->nUsed   = 0;` |
|   613999 |   66 | `	pSet->nCursor = 0;` |
|   613999 |   67 | `	return SXRET_OK;` |
|        2 |   68 |  |
|    27310 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|        2 |   70 |  |
|    27312 |   71 | `	pSet->nCursor = 0;` |
|    27312 |   72 | `	return SXRET_OK;` |
|        2 |   73 |  |
|    30170 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|        2 |   75 |  |
|        - |   76 | `	register unsigned char *zSrc;` |
|    30172 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|        - |   78 | `		/* Reset cursor */` |
|    10450 |   79 | `		pSet->nCursor = 0;` |
|    10450 |   80 | `		return SXERR_EOF;` |
|        - |   81 | `	}` |
|    19724 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|    19724 |   83 | `	if( ppEntry ){` |
|    19724 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     9861 |   85 | `	}` |
|    19724 |   86 | `	pSet->nCursor++;` |
|    19724 |   87 | `	return SXRET_OK;` |
|    15087 |   88 |  |
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
|    40840 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|        2 |  101 |  |
|    40842 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       20 |  103 | `		pSet->nUsed = nNewSize;` |
|        9 |  104 | `	}` |
|    40842 |  105 | `	return SXRET_OK;` |
|        2 |  106 |  |
|  1953632 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|        2 |  108 |  |
|  1953634 |  109 | `	sxi32 rc = SXRET_OK;` |
|  1953634 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|  1050240 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   525119 |  112 | `	}` |
|  1953634 |  113 | `	pSet->pBase = 0;` |
|  1953634 |  114 | `	pSet->nUsed = 0;` |
|  1953634 |  115 | `	pSet->nCursor = 0;` |
|  1953634 |  116 | `	return rc;` |
|        2 |  117 |  |
|   958288 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|        2 |  119 |  |
|        - |  120 | `	const char *zBase;` |
|   958290 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       92 |  122 | `		return 0;` |
|        - |  123 | `	}` |
|   958200 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   958200 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   479146 |  126 |  |
|   656560 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|        2 |  128 |  |
|        - |  129 | `	const char *zBase;` |
|        - |  130 | `	void *pData;` |
|   656562 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    74554 |  132 | `		return 0;` |
|        - |  133 | `	}` |
|   582010 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   582010 |  135 | `	pSet->nUsed--;` |
|   582010 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   582010 |  137 | `	return pData;` |
|   328282 |  138 |  |
|  4030682 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|        2 |  140 |  |
|        - |  141 | `	const char *zBase;` |
|  4030684 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|        - |  143 | `		/* Out of range */` |
|      ! 0 |  144 | `		return 0;` |
|        - |  145 | `	}` |
|  4030684 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  4030684 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|  2015419 |  148 |  |
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
|    42502 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|        2 |  163 |  |
|        - |  164 | `	SyHashEntry_Pr **apNew;` |
|        - |  165 | `#if defined(UNTRUST)` |
|        - |  166 | `	if( pHash == 0 ){` |
|        - |  167 | `		return SXERR_EMPTY;` |
|        - |  168 | `	}` |
|        - |  169 | `#endif` |
|        - |  170 | `	/* Allocate a new table */` |
|    42504 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    42504 |  172 | `	if( apNew == 0 ){` |
|      ! 0 |  173 | `		return SXERR_MEM;` |
|        - |  174 | `	}` |
|    42504 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    42504 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    42504 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    42504 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    42504 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    42504 |  180 | `	pHash->nEntry = 0;` |
|    42504 |  181 | `	pHash->apBucket = apNew;` |
|    42504 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    42504 |  183 | `	return SXRET_OK;` |
|    21253 |  184 |  |
|     5948 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|        2 |  186 |  |
|        - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|        - |  188 | `#if defined(UNTRUST)` |
|        - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|        - |  190 | `		return SXERR_EMPTY;` |
|        - |  191 | `	}` |
|        - |  192 | `#endif` |
|     5950 |  193 | `	pEntry = pHash->pList;` |
|     3191 |  194 | `	for(;;){` |
|     6384 |  195 | `		if( pHash->nEntry == 0 ){` |
|     5950 |  196 | `			break;` |
|        - |  197 | `		}` |
|      436 |  198 | `		pNext = pEntry->pNext;` |
|      436 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      436 |  200 | `		pEntry = pNext;` |
|      436 |  201 | `		pHash->nEntry--;` |
|        2 |  202 | `	}` |
|     5950 |  203 | `	if( pHash->apBucket ){` |
|     5950 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     2974 |  205 | `	}` |
|     5950 |  206 | `	pHash->apBucket = 0;` |
|     5950 |  207 | `	pHash->nBucketSize = 0;` |
|     5950 |  208 | `	pHash->pAllocator = 0;` |
|     5950 |  209 | `	return SXRET_OK;` |
|        2 |  210 |  |
|  4384432 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|        2 |  212 |  |
|        - |  213 | `	SyHashEntry_Pr *pEntry;` |
|        - |  214 | `	sxu32 nHash;` |
|        - |  215 |  |
|  4384434 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  4384434 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  4259333 |  218 | `	for(;;){` |
|  8550821 |  219 | `		if( pEntry == 0 ){` |
|  2420806 |  220 | `			break;` |
|        - |  221 | `		}` |
|  7111824 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|  1963632 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|  1963630 |  224 | `				return pEntry;` |
|        - |  225 | `		}` |
|  4166389 |  226 | `		pEntry = pEntry->pNextCollide;` |
|        2 |  227 | `	}` |
|        - |  228 | `	/* Entry not found */` |
|  2420806 |  229 | `	return 0;` |
|  2192236 |  230 |  |
|  4406548 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|        2 |  232 |  |
|        - |  233 | `	SyHashEntry_Pr *pEntry;` |
|        - |  234 | `#if defined(UNTRUST)` |
|        - |  235 | `	if( INVALID_HASH(pHash) ){` |
|        - |  236 | `		return 0;` |
|        - |  237 | `	}` |
|        - |  238 | `#endif` |
|  4406550 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|        - |  240 | `		/* Don't bother hashing,return immediately */` |
|    22124 |  241 | `		return 0;` |
|        - |  242 | `	}` |
|  4384428 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  4384428 |  244 | `	if( pEntry == 0 ){` |
|  2420806 |  245 | `		return 0;` |
|        - |  246 | `	}` |
|  1963624 |  247 | `	return (SyHashEntry *)pEntry;` |
|  2203294 |  248 |  |
|    40440 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|        2 |  250 |  |
|        - |  251 | `	sxi32 rc;` |
|    40442 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    28694 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|    14348 |  254 | `	}else{` |
|    11749 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|        - |  256 | `	}` |
|    40442 |  257 | `	if( pEntry->pNextCollide ){` |
|     3423 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     1711 |  259 | `	}` |
|    40442 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    40442 |  261 | `	pHash->nEntry--;` |
|    40442 |  262 | `	if( ppUserData ){` |
|        - |  263 | `		/* Write a pointer to the user data */` |
|      ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|      ! 0 |  265 | `	}` |
|        - |  266 | `	/* Release the entry */` |
|    40442 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    40442 |  268 | `	return rc;` |
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
|    40434 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|        2 |  287 |  |
|    40436 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|        - |  289 | `	sxi32 rc;` |
|        - |  290 | `#if defined(UNTRUST)` |
|        - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|        - |  292 | `		return SXERR_CORRUPT;` |
|        - |  293 | `	}` |
|        - |  294 | `#endif` |
|    40436 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    40436 |  296 | `	return rc;` |
|        2 |  297 |  |
|    64612 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|        2 |  299 |  |
|        - |  300 | `#if defined(UNTRUST)` |
|        - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|        - |  302 | `		return SXERR_CORRUPT;` |
|        - |  303 | `	}` |
|        - |  304 | `#endif` |
|    64614 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    64614 |  306 | `	return SXRET_OK;` |
|        2 |  307 |  |
|   366126 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|        2 |  309 |  |
|        - |  310 | `	SyHashEntry_Pr *pEntry;` |
|        - |  311 | `#if defined(UNTRUST)` |
|        - |  312 | `	if( INVALID_HASH(pHash) ){` |
|        - |  313 | `		return 0;` |
|        - |  314 | `	}` |
|        - |  315 | `#endif` |
|   366128 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    64180 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    64180 |  318 | `		return 0;` |
|        - |  319 | `	}` |
|   301950 |  320 | `	pEntry = pHash->pCurrent;` |
|        - |  321 | `	/* Advance the cursor */` |
|   301950 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|        - |  323 | `	/* Return the current entry */` |
|   301950 |  324 | `	return (SyHashEntry *)pEntry;` |
|   183065 |  325 |  |
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
|     7686 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|        2 |  349 |  |
|     7688 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|        - |  351 | `	SyHashEntry_Pr *pEntry;` |
|        - |  352 | `	SyHashEntry_Pr **apNew;` |
|        - |  353 | `	sxu32 n,iBucket;` |
|        - |  354 |  |
|        - |  355 | `	/* Allocate a new larger table */` |
|     7688 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     7688 |  357 | `	if( apNew == 0 ){` |
|        - |  358 | `		/* Not so fatal,simply a performance hit */` |
|      ! 0 |  359 | `		return SXRET_OK;` |
|        - |  360 | `	}` |
|        - |  361 | `	/* Zero the new table */` |
|     7688 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|        - |  363 | `	/* Rehash all entries */` |
|  1045928 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|  1038242 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - |  366 | `		/* Install in the new bucket */` |
|  1038242 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|  1038242 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|  1038242 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   498639 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|   249318 |  371 | `		}` |
|  1038242 |  372 | `		apNew[iBucket] = pEntry;` |
|        - |  373 | `		/* Point to the next entry */` |
|  1038242 |  374 | `		pEntry = pEntry->pNext;` |
|   519122 |  375 | `	}` |
|        - |  376 | `	/* Release the old table and reflect the change */` |
|     7688 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     7688 |  378 | `	pHash->apBucket = apNew;` |
|     7688 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     7688 |  380 | `	return SXRET_OK;` |
|     3845 |  381 |  |
|   875420 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|        2 |  383 |  |
|   875422 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|        - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   875422 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   875422 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   628340 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   314145 |  389 | `	}` |
|   875422 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|        - |  391 | `	/* Link to the entry list */` |
|   875422 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   875422 |  393 | `	if( pHash->nEntry == 0 ){` |
|    28136 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    14067 |  395 | `	}` |
|   875422 |  396 | `	pHash->nEntry++;` |
|   875422 |  397 | `	return SXRET_OK;` |
|        2 |  398 |  |
|   875420 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|        2 |  400 |  |
|        - |  401 | `	SyHashEntry_Pr *pEntry;` |
|        - |  402 | `	sxi32 rc;` |
|        - |  403 | `#if defined(UNTRUST)` |
|        - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|        - |  405 | `		return SXERR_CORRUPT;` |
|        - |  406 | `	}` |
|        - |  407 | `#endif` |
|   875422 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     7688 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     7688 |  410 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  411 | `			return rc;` |
|        - |  412 | `		}` |
|     3843 |  413 | `	}` |
|        - |  414 | `	/* Allocate a new hash entry */` |
|   875422 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   875422 |  416 | `	if( pEntry == 0 ){` |
|      ! 0 |  417 | `		return SXERR_MEM;` |
|        - |  418 | `	}` |
|        - |  419 | `	/* Zero the entry */` |
|   875422 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   875422 |  421 | `	pEntry->pHash = pHash;` |
|   875422 |  422 | `	pEntry->pKey = pKey;` |
|   875422 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   875422 |  424 | `	pEntry->pUserData = pUserData;` |
|   875422 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|        - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   875422 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   875422 |  428 | `	return rc;` |
|   437712 |  429 |  |
|    50070 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|        2 |  431 |  |
|        - |  432 | `#if defined(UNTRUST)` |
|        - |  433 | `	if( INVALID_HASH(pHash) ){` |
|        - |  434 | `		return 0;` |
|        - |  435 | `	}` |
|        - |  436 | `#endif` |
|        - |  437 | `	/* Last inserted entry */` |
|    50072 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|        2 |  439 |  |
|        - |  440 |  |
