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
|  17447734 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  17447736 |   16 | `	pSet->nSize = 0 ;` |
|  17447736 |   17 | `	pSet->nUsed = 0;` |
|  17447736 |   18 | `	pSet->nCursor = 0;` |
|  17447736 |   19 | `	pSet->eSize = ElemSize;` |
|  17447736 |   20 | `	pSet->pAllocator = pAllocator;` |
|  17447736 |   21 | `	pSet->pBase =  0;` |
|  17447736 |   22 | `	pSet->pUserData = 0;` |
|  17447736 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  28616562 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  28616564 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4275014 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4275014 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4134304 |   34 | `			pSet->nSize = 4;` |
|   2067151 |   35 | `		}` |
|   4275014 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4275014 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4275014 |   40 | `		pSet->pBase = pNew;` |
|   4275014 |   41 | `		pSet->nSize <<= 1;` |
|   2137506 |   42 | `	}` |
|  28616564 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 214103974 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  28616564 |   45 | `	pSet->nUsed++;` |
|  28616564 |   46 | `	return SXRET_OK;` |
|  14308305 |   47 |  |
|   1160204 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|   1160206 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1160206 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1160206 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1160206 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1160206 |   60 | `	pSet->nSize = nItem;` |
|   1160206 |   61 | `	return SXRET_OK;` |
|    580104 |   62 |  |
|   1638918 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1638920 |   65 | `	pSet->nUsed   = 0;` |
|   1638920 |   66 | `	pSet->nCursor = 0;` |
|   1638920 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     53742 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     53744 |   71 | `	pSet->nCursor = 0;` |
|     53744 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     57902 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     57904 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     22146 |   79 | `		pSet->nCursor = 0;` |
|     22146 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     35760 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     35760 |   83 | `	if( ppEntry ){` |
|     35760 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     17879 |   85 | `	}` |
|     35760 |   86 | `	pSet->nCursor++;` |
|     35760 |   87 | `	return SXRET_OK;` |
|     28953 |   88 |  |
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
|    196140 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    196142 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       110 |  103 | `		pSet->nUsed = nNewSize;` |
|        54 |  104 | `	}` |
|    196142 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   9356620 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   9356622 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9356622 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4727170 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2363584 |  112 | `	}` |
|   9356622 |  113 | `	pSet->pBase = 0;` |
|   9356622 |  114 | `	pSet->nUsed = 0;` |
|   9356622 |  115 | `	pSet->nCursor = 0;` |
|   9356622 |  116 | `	return rc;` |
|         2 |  117 |  |
|   5327530 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5327532 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       112 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5327422 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5327422 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2663767 |  126 |  |
|   3447288 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3447290 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2151614 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1295678 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1295678 |  135 | `	pSet->nUsed--;` |
|   1295678 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1295678 |  137 | `	return pData;` |
|   1723646 |  138 |  |
|  12562123 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  12562125 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  12562125 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  12562125 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6281194 |  148 |  |
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
|    381402 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    381404 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    381404 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    381404 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    381404 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    381404 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    381404 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    381404 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    381404 |  180 | `	pHash->nEntry = 0;` |
|    381404 |  181 | `	pHash->apBucket = apNew;` |
|    381404 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    381404 |  183 | `	return SXRET_OK;` |
|    190703 |  184 |  |
|     92548 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     92550 |  193 | `	pEntry = pHash->pList;` |
|     49323 |  194 | `	for(;;){` |
|     98648 |  195 | `		if( pHash->nEntry == 0 ){` |
|     92550 |  196 | `			break;` |
|         - |  197 | `		}` |
|      6100 |  198 | `		pNext = pEntry->pNext;` |
|      6100 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      6100 |  200 | `		pEntry = pNext;` |
|      6100 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     92550 |  203 | `	if( pHash->apBucket ){` |
|     92550 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     46274 |  205 | `	}` |
|     92550 |  206 | `	pHash->apBucket = 0;` |
|     92550 |  207 | `	pHash->nBucketSize = 0;` |
|     92550 |  208 | `	pHash->pAllocator = 0;` |
|     92550 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  14265508 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  14265510 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  14265510 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  12765882 |  218 | `	for(;;){` |
|  25476093 |  219 | `		if( pEntry == 0 ){` |
|   7754530 |  220 | `			break;` |
|         - |  221 | `		}` |
|  20976925 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   6510984 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   6510982 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  11210585 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   7754530 |  229 | `	return 0;` |
|   7133020 |  230 |  |
|  14891782 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  14891784 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    626430 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  14265356 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  14265356 |  244 | `	if( pEntry == 0 ){` |
|   7754530 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   6510828 |  247 | `	return (SyHashEntry *)pEntry;` |
|   7446157 |  248 |  |
|    110930 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    110932 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     84894 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     42448 |  254 | `	}else{` |
|     26040 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    110932 |  257 | `	if( pEntry->pNextCollide ){` |
|      4933 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2466 |  259 | `	}` |
|    110932 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    110932 |  261 | `	pHash->nEntry--;` |
|    110932 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    110932 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    110932 |  268 | `	return rc;` |
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
|    110776 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|    110778 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    110778 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    110778 |  296 | `	return rc;` |
|         2 |  297 |  |
|    625382 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    625384 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    625384 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   3560914 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   3560916 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    624948 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    624948 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2935970 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2935970 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2935970 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1780459 |  325 |  |
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
|      1899 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1889 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1889 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1889 |  344 | `		pEntry = pEntry->pNext;` |
|       945 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     26138 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     26140 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     26140 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     26140 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     26140 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3321916 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3295778 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3295778 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3295778 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3295778 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1569935 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    784863 |  371 | `		}` |
|   3295778 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3295778 |  374 | `		pEntry = pEntry->pNext;` |
|   1647890 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     26140 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     26140 |  378 | `	pHash->apBucket = apNew;` |
|     26140 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     26140 |  380 | `	return SXRET_OK;` |
|     13071 |  381 |  |
|   3571548 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   3571550 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   3571550 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   3571550 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2299707 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1149845 |  389 | `	}` |
|   3571550 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   3571550 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   3571550 |  393 | `	if( pHash->nEntry == 0 ){` |
|    181350 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     90674 |  395 | `	}` |
|   3571550 |  396 | `	pHash->nEntry++;` |
|   3571550 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   3571548 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   3571550 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     26140 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     26140 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     13069 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   3571550 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   3571550 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   3571550 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   3571550 |  421 | `	pEntry->pHash = pHash;` |
|   3571550 |  422 | `	pEntry->pKey = pKey;` |
|   3571550 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   3571550 |  424 | `	pEntry->pUserData = pUserData;` |
|   3571550 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   3571550 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   3571550 |  428 | `	return rc;` |
|   1785776 |  429 |  |
|    140258 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    140260 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
