# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 293/304 lines (96.38%)

[Root index](../../index.md) | [Directory index](index.md)

|       Hits | Line | Source |
| ---------: | ---: | :--- |
|          - |    1 | `/**` |
|          - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|          - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|          - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|          - |    5 | ` */` |
|          - |    6 | `#include "sxtypes.h"` |
|          - |    7 | `#include "sxmacros.h"` |
|          - |    8 | `#include "sxset.h"` |
|          - |    9 | `#include "sxmem.h"` |
|          - |   10 | `#include "sxhashtable.h"` |
|          - |   11 | `#include "sxhash.h"` |
|          - |   12 | `#include "sxstr.h"` |
|          - |   13 |  |
|  162003768 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  162003773 |   16 | `	pSet->nSize = 0 ;` |
|  162003773 |   17 | `	pSet->nUsed = 0;` |
|  162003773 |   18 | `	pSet->nCursor = 0;` |
|  162003773 |   19 | `	pSet->eSize = ElemSize;` |
|  162003773 |   20 | `	pSet->pAllocator = pAllocator;` |
|  162003773 |   21 | `	pSet->pBase =  0;` |
|  162003773 |   22 | `	pSet->pUserData = 0;` |
|  162003773 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  368855513 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  368855518 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   21521353 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   21521353 |   33 | `		if( pSet->nSize <= 0 ){` |
|   18362101 |   34 | `			pSet->nSize = 4;` |
|    9181048 |   35 | `		}` |
|   21521353 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   21521353 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   21521353 |   40 | `		pSet->pBase = pNew;` |
|   21521353 |   41 | `		pSet->nSize <<= 1;` |
|   10760674 |   42 | `	}` |
|  368855518 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2909602868 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  368855518 |   45 | `	pSet->nUsed++;` |
|  368855518 |   46 | `	return SXRET_OK;` |
|  184427785 |   47 | `}` |
|   18298980 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   18298985 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   18298985 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   18298985 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   18298985 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   18298985 |   60 | `	pSet->nSize = nItem;` |
|   18298985 |   61 | `	return SXRET_OK;` |
|    9149495 |   62 | `}` |
|   27218647 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   27218652 |   65 | `	pSet->nUsed   = 0;` |
|   27218652 |   66 | `	pSet->nCursor = 0;` |
|   27218652 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      70252 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      70257 |   71 | `	pSet->nCursor = 0;` |
|      70257 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      74434 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      74439 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30391 |   79 | `		pSet->nCursor = 0;` |
|      30391 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      44053 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      44053 |   83 | `	if( ppEntry ){` |
|      44053 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      22024 |   85 | `	}` |
|      44053 |   86 | `	pSet->nCursor++;` |
|      44053 |   87 | `	return SXRET_OK;` |
|      37222 |   88 | `}` |
|          - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|          8 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|          1 |   91 | `{` |
|          - |   92 | `	register unsigned char *zSrc;` |
|          9 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          3 |   94 | `		return 0;` |
|          - |   95 | `	}` |
|          7 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|          7 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|          5 |   98 | `}` |
|          - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    2751578 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2751583 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1199 |  103 | `		pSet->nUsed = nNewSize;` |
|        597 |  104 | `	}` |
|    2751583 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   56265650 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   56265655 |  109 | `	sxi32 rc = SXRET_OK;` |
|   56265655 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   30643375 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   15321685 |  112 | `	}` |
|   56265655 |  113 | `	pSet->pBase = 0;` |
|   56265655 |  114 | `	pSet->nUsed = 0;` |
|   56265655 |  115 | `	pSet->nCursor = 0;` |
|   56265655 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   65483938 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   65483943 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19217 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   65464731 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   65464731 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   32741974 |  126 | `}` |
|    9402618 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9402623 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2232807 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7169821 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7169821 |  135 | `	pSet->nUsed--;` |
|    7169821 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7169821 |  137 | `	return pData;` |
|    4701314 |  138 | `}` |
|   34387675 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   34387680 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   34387658 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   34387658 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   17194018 |  148 | `}` |
|          - |  149 | `/* Private hash entry */` |
|          - |  150 | `struct SyHashEntry_Pr` |
|          - |  151 | `{` |
|          - |  152 | `	const void *pKey; /* Hash key */` |
|          - |  153 | `	sxu32 nKeyLen;    /* Key length */` |
|          - |  154 | `	void *pUserData;  /* User private data */` |
|          - |  155 | `	/* Private fields */` |
|          - |  156 | `	sxu32 nHash;` |
|          - |  157 | `	SyHash *pHash;` |
|          - |  158 | `	SyHashEntry_Pr *pNext,*pPrev; /* Next and previous entry in the list */` |
|          - |  159 | `	SyHashEntry_Pr *pNextCollide,*pPrevCollide; /* Collision list */` |
|          - |  160 | `};` |
|          - |  161 | `#define INVALID_HASH(H) ((H)->apBucket == 0)` |
|    1770016 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1770021 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1770021 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1770021 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1770021 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1770021 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1770021 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1770021 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1770021 |  180 | `	pHash->nEntry = 0;` |
|    1770021 |  181 | `	pHash->apBucket = apNew;` |
|    1770021 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1770021 |  183 | `	return SXRET_OK;` |
|     885013 |  184 | `}` |
|     412488 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     412493 |  193 | `	pEntry = pHash->pList;` |
|     220740 |  194 | `	for(;;){` |
|     441485 |  195 | `		if( pHash->nEntry == 0 ){` |
|     412493 |  196 | `			break;` |
|          - |  197 | `		}` |
|      28997 |  198 | `		pNext = pEntry->pNext;` |
|      28997 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      28997 |  200 | `		pEntry = pNext;` |
|      28997 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     412493 |  203 | `	if( pHash->apBucket ){` |
|     412493 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     206244 |  205 | `	}` |
|     412493 |  206 | `	pHash->apBucket = 0;` |
|     412493 |  207 | `	pHash->nBucketSize = 0;` |
|     412493 |  208 | `	pHash->pAllocator = 0;` |
|     412493 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   67302243 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   67302248 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   67302248 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   61085859 |  218 | `	for(;;){` |
|  122317497 |  219 | `		if( pEntry == 0 ){` |
|   24360006 |  220 | `			break;` |
|          - |  221 | `		}` |
|  119430488 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   42946266 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   42942247 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   55015254 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   24360006 |  229 | `	return 0;` |
|   33651410 |  230 | `}` |
|   74308469 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   74308474 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    7006693 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   67301786 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   67301786 |  244 | `	if( pEntry == 0 ){` |
|   24359988 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   42941803 |  247 | `	return (SyHashEntry *)pEntry;` |
|   37154523 |  248 | `}` |
|     437246 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     437251 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     356845 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     178425 |  254 | `	}else{` |
|      80411 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     437251 |  257 | `	if( pEntry->pNextCollide ){` |
|       4374 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2185 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     437251 |  261 | `	if( pHash->pLast == pEntry ){` |
|     427263 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     213629 |  263 | `	}` |
|     437251 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     437251 |  265 | `	pHash->nEntry--;` |
|     437251 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     437251 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     437251 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        462 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        467 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        467 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        449 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        449 |  288 | `	return rc;` |
|        236 |  289 | `}` |
|     436802 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     436807 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     436807 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     436807 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2836534 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2836539 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2836539 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   21206074 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   21206079 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2836273 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2836273 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18369811 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18369811 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18369811 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10603042 |  329 | `}` |
|         14 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|          1 |  331 | `{` |
|          - |  332 | `	SyHashEntry_Pr *pEntry;` |
|          - |  333 | `	sxi32 rc;` |
|          - |  334 | `	sxu32 n;` |
|          - |  335 | `#if defined(UNTRUST)` |
|          - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|          - |  337 | `		return 0;` |
|          - |  338 | `	}` |
|          - |  339 | `#endif` |
|         15 |  340 | `	pEntry = pHash->pList;` |
|       4099 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4085 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4085 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4085 |  348 | `		pEntry = pEntry->pNext;` |
|       2043 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|      91878 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      91883 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      91883 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      91883 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      91883 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14425067 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14333189 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14333189 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14333189 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14333189 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    6871097 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3435852 |  375 | `		}` |
|   14333189 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14333189 |  378 | `		pEntry = pEntry->pNext;` |
|    7166597 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      91883 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      91883 |  382 | `	pHash->apBucket = apNew;` |
|      91883 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      91883 |  384 | `	return SXRET_OK;` |
|      45944 |  385 | `}` |
|   18479622 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   18479627 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   18479627 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   18479627 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11716718 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5858168 |  393 | `	}` |
|   18479627 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   18479627 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   18479575 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   18479627 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     976155 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     976155 |  408 | `		pHash->pLast = pEntry;` |
|     488075 |  409 | `	}` |
|   18479627 |  410 | `	pHash->nEntry++;` |
|   18479627 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   18479622 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   18479627 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      91883 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      91883 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45939 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   18479627 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   18479627 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   18479627 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   18479627 |  435 | `	pEntry->pHash = pHash;` |
|   18479627 |  436 | `	pEntry->pKey = pKey;` |
|   18479627 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   18479627 |  438 | `	pEntry->pUserData = pUserData;` |
|   18479627 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   18479627 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   18479627 |  442 | `	return rc;` |
|    9239816 |  443 | `}` |
|   18479488 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   18479493 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        134 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          2 |  455 | `{` |
|        136 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          2 |  457 | `}` |
|     476536 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     476541 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
