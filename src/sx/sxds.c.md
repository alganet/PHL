# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 289/304 lines (95.07%)

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
|  21436034 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 | `{` |
|  21436039 |   16 | `	pSet->nSize = 0 ;` |
|  21436039 |   17 | `	pSet->nUsed = 0;` |
|  21436039 |   18 | `	pSet->nCursor = 0;` |
|  21436039 |   19 | `	pSet->eSize = ElemSize;` |
|  21436039 |   20 | `	pSet->pAllocator = pAllocator;` |
|  21436039 |   21 | `	pSet->pBase =  0;` |
|  21436039 |   22 | `	pSet->pUserData = 0;` |
|  21436039 |   23 | `	return SXRET_OK;` |
|         5 |   24 | `}` |
|  35978533 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 | `{` |
|         - |   27 | `	unsigned char *zbase;` |
|  35978538 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4997961 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4997961 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4814663 |   34 | `			pSet->nSize = 4;` |
|   2407329 |   35 | `		}` |
|   4997961 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4997961 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4997961 |   40 | `		pSet->pBase = pNew;` |
|   4997961 |   41 | `		pSet->nSize <<= 1;` |
|   2498978 |   42 | `	}` |
|  35978538 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 269310894 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  35978538 |   45 | `	pSet->nUsed++;` |
|  35978538 |   46 | `	return SXRET_OK;` |
|  17989314 |   47 | `}` |
|   1529498 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 | `{` |
|   1529503 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1529503 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1529503 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1529503 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1529503 |   60 | `	pSet->nSize = nItem;` |
|   1529503 |   61 | `	return SXRET_OK;` |
|    764754 |   62 | `}` |
|   2414351 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 | `{` |
|   2414356 |   65 | `	pSet->nUsed   = 0;` |
|   2414356 |   66 | `	pSet->nCursor = 0;` |
|   2414356 |   67 | `	return SXRET_OK;` |
|         5 |   68 | `}` |
|     67262 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 | `{` |
|     67267 |   71 | `	pSet->nCursor = 0;` |
|     67267 |   72 | `	return SXRET_OK;` |
|         5 |   73 | `}` |
|     71420 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 | `{` |
|         - |   76 | `	register unsigned char *zSrc;` |
|     71425 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     28927 |   79 | `		pSet->nCursor = 0;` |
|     28927 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     42503 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     42503 |   83 | `	if( ppEntry ){` |
|     42503 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     21249 |   85 | `	}` |
|     42503 |   86 | `	pSet->nCursor++;` |
|     42503 |   87 | `	return SXRET_OK;` |
|     35715 |   88 | `}` |
|         - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         8 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|         1 |   91 | `{` |
|         - |   92 | `	register unsigned char *zSrc;` |
|         9 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         3 |   94 | `		return 0;` |
|         - |   95 | `	}` |
|         7 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|         7 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|         5 |   98 | `}` |
|         - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    245594 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 | `{` |
|    245599 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       679 |  103 | `		pSet->nUsed = nNewSize;` |
|       337 |  104 | `	}` |
|    245599 |  105 | `	return SXRET_OK;` |
|         5 |  106 | `}` |
|  10971380 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 | `{` |
|  10971385 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10971385 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5533479 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2766737 |  112 | `	}` |
|  10971385 |  113 | `	pSet->pBase = 0;` |
|  10971385 |  114 | `	pSet->nUsed = 0;` |
|  10971385 |  115 | `	pSet->nCursor = 0;` |
|  10971385 |  116 | `	return rc;` |
|         5 |  117 | `}` |
|   6425024 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 | `{` |
|         - |  120 | `	const char *zBase;` |
|   6425029 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       133 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   6424901 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   6424901 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   3212517 |  126 | `}` |
|   3801438 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 | `{` |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3801443 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2193849 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1607599 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1607599 |  135 | `	pSet->nUsed--;` |
|   1607599 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1607599 |  137 | `	return pData;` |
|   1900724 |  138 | `}` |
|  14482594 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	const char *zBase;` |
|  14482599 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  14482599 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  14482599 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   7241625 |  148 | `}` |
|         - |  149 | `/* Private hash entry */` |
|         - |  150 | `struct SyHashEntry_Pr` |
|         - |  151 | `{` |
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
|    688466 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 | `{` |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    688471 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    688471 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    688471 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    688471 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    688471 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    688471 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    688471 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    688471 |  180 | `	pHash->nEntry = 0;` |
|    688471 |  181 | `	pHash->apBucket = apNew;` |
|    688471 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    688471 |  183 | `	return SXRET_OK;` |
|    344238 |  184 | `}` |
|    159318 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 | `{` |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    159323 |  193 | `	pEntry = pHash->pList;` |
|     85938 |  194 | `	for(;;){` |
|    171881 |  195 | `		if( pHash->nEntry == 0 ){` |
|    159323 |  196 | `			break;` |
|         - |  197 | `		}` |
|     12563 |  198 | `		pNext = pEntry->pNext;` |
|     12563 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     12563 |  200 | `		pEntry = pNext;` |
|     12563 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    159323 |  203 | `	if( pHash->apBucket ){` |
|    159323 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     79659 |  205 | `	}` |
|    159323 |  206 | `	pHash->apBucket = 0;` |
|    159323 |  207 | `	pHash->nBucketSize = 0;` |
|    159323 |  208 | `	pHash->pAllocator = 0;` |
|    159323 |  209 | `	return SXRET_OK;` |
|         5 |  210 | `}` |
|  20334348 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 | `{` |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  20334353 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  20334353 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  18418217 |  218 | `	for(;;){` |
|  36922552 |  219 | `		if( pEntry == 0 ){` |
|  10374169 |  220 | `			break;` |
|         - |  221 | `		}` |
|  31528231 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   9960196 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   9960189 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  16588204 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|  10374169 |  229 | `	return 0;` |
|  10167689 |  230 | `}` |
|  21372524 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 | `{` |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  21372529 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|   1038445 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  20334089 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  20334089 |  244 | `	if( pEntry == 0 ){` |
|  10374169 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   9959925 |  247 | `	return (SyHashEntry *)pEntry;` |
|  10686777 |  248 | `}` |
|    189990 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 | `{` |
|         - |  251 | `	sxi32 rc;` |
|    189995 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    150223 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     75114 |  254 | `	}else{` |
|     39777 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    189995 |  257 | `	if( pEntry->pNextCollide ){` |
|      3534 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      1766 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    189995 |  261 | `	if( pHash->pLast == pEntry ){` |
|    183423 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     91709 |  263 | `	}` |
|    189995 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    189995 |  265 | `	pHash->nEntry--;` |
|    189995 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    189995 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    189995 |  272 | `	return rc;` |
|         5 |  273 | `}` |
|       264 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 | `{` |
|         - |  276 | `	SyHashEntry_Pr *pEntry;` |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( INVALID_HASH(pHash) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|       269 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       269 |  284 | `	if( pEntry == 0 ){` |
|       ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|         - |  286 | `	}` |
|       269 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       269 |  288 | `	return rc;` |
|       137 |  289 | `}` |
|    189726 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 | `{` |
|    189731 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    189731 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    189731 |  300 | `	return rc;` |
|         5 |  301 | `}` |
|   1337518 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 | `{` |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1337523 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1337523 |  310 | `	return SXRET_OK;` |
|         5 |  311 | `}` |
|   8404822 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 | `{` |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   8404827 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1337261 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1337261 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   7067571 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   7067571 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   7067571 |  328 | `	return (SyHashEntry *)pEntry;` |
|   4202416 |  329 | `}` |
|        10 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         1 |  331 | `{` |
|         - |  332 | `	SyHashEntry_Pr *pEntry;` |
|         - |  333 | `	sxi32 rc;` |
|         - |  334 | `	sxu32 n;` |
|         - |  335 | `#if defined(UNTRUST)` |
|         - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  337 | `		return 0;` |
|         - |  338 | `	}` |
|         - |  339 | `#endif` |
|        11 |  340 | `	pEntry = pHash->pList;` |
|      2139 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|      2129 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      2129 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|      2129 |  348 | `		pEntry = pEntry->pNext;` |
|      1065 |  349 | `	}` |
|        11 |  350 | `	return SXRET_OK;` |
|         6 |  351 | `}` |
|     36374 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  353 | `{` |
|     36379 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  355 | `	SyHashEntry_Pr *pEntry;` |
|         - |  356 | `	SyHashEntry_Pr **apNew;` |
|         - |  357 | `	sxu32 n,iBucket;` |
|         - |  358 |  |
|         - |  359 | `	/* Allocate a new larger table */` |
|     36379 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     36379 |  361 | `	if( apNew == 0 ){` |
|         - |  362 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  363 | `		return SXRET_OK;` |
|         - |  364 | `	}` |
|         - |  365 | `	/* Zero the new table */` |
|     36379 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  367 | `	/* Rehash all entries */` |
|   4478875 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   4442501 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  370 | `		/* Install in the new bucket */` |
|   4442501 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   4442501 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   4442501 |  373 | `		if( apNew[iBucket] != 0 ){` |
|   2156744 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|   1078333 |  375 | `		}` |
|   4442501 |  376 | `		apNew[iBucket] = pEntry;` |
|         - |  377 | `		/* Point to the next entry */` |
|   4442501 |  378 | `		pEntry = pEntry->pNext;` |
|   2221253 |  379 | `	}` |
|         - |  380 | `	/* Release the old table and reflect the change */` |
|     36379 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     36379 |  382 | `	pHash->apBucket = apNew;` |
|     36379 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     36379 |  384 | `	return SXRET_OK;` |
|     18192 |  385 | `}` |
|   5730074 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 | `{` |
|   5730079 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   5730079 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   5730079 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   3221325 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1610666 |  393 | `	}` |
|   5730079 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   5730079 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        51 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        51 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        51 |  401 | `		pHash->pLast = pEntry;` |
|        26 |  402 | `	}else{` |
|   5730029 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   5730079 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    365883 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    365883 |  408 | `		pHash->pLast = pEntry;` |
|    182939 |  409 | `	}` |
|   5730079 |  410 | `	pHash->nEntry++;` |
|   5730079 |  411 | `	return SXRET_OK;` |
|         5 |  412 | `}` |
|   5730074 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 | `{` |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   5730079 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     36379 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     36379 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     18187 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   5730079 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   5730079 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   5730079 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   5730079 |  435 | `	pEntry->pHash = pHash;` |
|   5730079 |  436 | `	pEntry->pKey = pKey;` |
|   5730079 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   5730079 |  438 | `	pEntry->pUserData = pUserData;` |
|   5730079 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   5730079 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   5730079 |  442 | `	return rc;` |
|   2865042 |  443 | `}` |
|   5729958 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 | `{` |
|   5729963 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|         5 |  447 | `}` |
|         - |  448 | `/*` |
|         - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|         - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|         - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|         - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|         - |  453 | ` */` |
|       116 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  455 | `{` |
|       118 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|         2 |  457 | `}` |
|    229948 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 | `{` |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    229953 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 | `}` |
|         - |  468 |  |
