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
|  16498286 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  16498288 |   16 | `	pSet->nSize = 0 ;` |
|  16498288 |   17 | `	pSet->nUsed = 0;` |
|  16498288 |   18 | `	pSet->nCursor = 0;` |
|  16498288 |   19 | `	pSet->eSize = ElemSize;` |
|  16498288 |   20 | `	pSet->pAllocator = pAllocator;` |
|  16498288 |   21 | `	pSet->pBase =  0;` |
|  16498288 |   22 | `	pSet->pUserData = 0;` |
|  16498288 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  27067988 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  27067990 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4120988 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4120988 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3987606 |   34 | `			pSet->nSize = 4;` |
|   1993802 |   35 | `		}` |
|   4120988 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4120988 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4120988 |   40 | `		pSet->pBase = pNew;` |
|   4120988 |   41 | `		pSet->nSize <<= 1;` |
|   2060493 |   42 | `	}` |
|  27067990 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 201922736 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  27067990 |   45 | `	pSet->nUsed++;` |
|  27067990 |   46 | `	return SXRET_OK;` |
|  13534018 |   47 |  |
|   1068636 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|   1068638 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1068638 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1068638 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1068638 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1068638 |   60 | `	pSet->nSize = nItem;` |
|   1068638 |   61 | `	return SXRET_OK;` |
|    534320 |   62 |  |
|   1558736 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1558738 |   65 | `	pSet->nUsed   = 0;` |
|   1558738 |   66 | `	pSet->nCursor = 0;` |
|   1558738 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     51316 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     51318 |   71 | `	pSet->nCursor = 0;` |
|     51318 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     55398 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     55400 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     21130 |   79 | `		pSet->nCursor = 0;` |
|     21130 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     34272 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     34272 |   83 | `	if( ppEntry ){` |
|     34272 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     17135 |   85 | `	}` |
|     34272 |   86 | `	pSet->nCursor++;` |
|     34272 |   87 | `	return SXRET_OK;` |
|     27701 |   88 |  |
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
|    185750 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    185752 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       110 |  103 | `		pSet->nUsed = nNewSize;` |
|        54 |  104 | `	}` |
|    185752 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   9092206 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   9092208 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9092208 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4593706 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2296852 |  112 | `	}` |
|   9092208 |  113 | `	pSet->pBase = 0;` |
|   9092208 |  114 | `	pSet->nUsed = 0;` |
|   9092208 |  115 | `	pSet->nCursor = 0;` |
|   9092208 |  116 | `	return rc;` |
|         2 |  117 |  |
|   5122182 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5122184 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       108 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5122078 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5122078 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2561093 |  126 |  |
|   3380930 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3380932 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2148872 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1232062 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1232062 |  135 | `	pSet->nUsed--;` |
|   1232062 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1232062 |  137 | `	return pData;` |
|   1690467 |  138 |  |
|  11924646 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  11924648 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  11924648 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  11924648 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5962505 |  148 |  |
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
|    298582 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    298584 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    298584 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    298584 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    298584 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    298584 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    298584 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    298584 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    298584 |  180 | `	pHash->nEntry = 0;` |
|    298584 |  181 | `	pHash->apBucket = apNew;` |
|    298584 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    298584 |  183 | `	return SXRET_OK;` |
|    149293 |  184 |  |
|     87398 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     87400 |  193 | `	pEntry = pHash->pList;` |
|     46392 |  194 | `	for(;;){` |
|     92786 |  195 | `		if( pHash->nEntry == 0 ){` |
|     87400 |  196 | `			break;` |
|         - |  197 | `		}` |
|      5388 |  198 | `		pNext = pEntry->pNext;` |
|      5388 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      5388 |  200 | `		pEntry = pNext;` |
|      5388 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     87400 |  203 | `	if( pHash->apBucket ){` |
|     87400 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     43699 |  205 | `	}` |
|     87400 |  206 | `	pHash->apBucket = 0;` |
|     87400 |  207 | `	pHash->nBucketSize = 0;` |
|     87400 |  208 | `	pHash->pAllocator = 0;` |
|     87400 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  13421464 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  13421466 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  13421466 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  12027373 |  218 | `	for(;;){` |
|  23952783 |  219 | `		if( pEntry == 0 ){` |
|   7325002 |  220 | `			break;` |
|         - |  221 | `		}` |
|  19675885 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   6096468 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   6096466 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  10531319 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   7325002 |  229 | `	return 0;` |
|   6710998 |  230 |  |
|  13986976 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  13986978 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    565658 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  13421322 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  13421322 |  244 | `	if( pEntry == 0 ){` |
|   7325002 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   6096322 |  247 | `	return (SyHashEntry *)pEntry;` |
|   6993754 |  248 |  |
|    105898 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    105900 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     80966 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     40484 |  254 | `	}else{` |
|     24936 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    105900 |  257 | `	if( pEntry->pNextCollide ){` |
|      4945 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2472 |  259 | `	}` |
|    105900 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    105900 |  261 | `	pHash->nEntry--;` |
|    105900 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    105900 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    105900 |  268 | `	return rc;` |
|         2 |  269 |  |
|       144 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       146 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       146 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       146 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       146 |  284 | `	return rc;` |
|        74 |  285 |  |
|    105754 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|    105756 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    105756 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    105756 |  296 | `	return rc;` |
|         2 |  297 |  |
|    373044 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    373046 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    373046 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2919128 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2919130 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    372610 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    372610 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2546522 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2546522 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2546522 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1459566 |  325 |  |
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
|      1797 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1787 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1787 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1787 |  344 | `		pEntry = pEntry->pNext;` |
|       894 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     24680 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     24682 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     24682 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     24682 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     24682 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3133834 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3109154 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3109154 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3109154 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3109154 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1488748 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    744356 |  371 | `		}` |
|   3109154 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3109154 |  374 | `		pEntry = pEntry->pNext;` |
|   1554578 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     24682 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     24682 |  378 | `	pHash->apBucket = apNew;` |
|     24682 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     24682 |  380 | `	return SXRET_OK;` |
|     12342 |  381 |  |
|   3262102 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   3262104 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   3262104 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   3262104 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2100294 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1050136 |  389 | `	}` |
|   3262104 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   3262104 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   3262104 |  393 | `	if( pHash->nEntry == 0 ){` |
|    148140 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     74069 |  395 | `	}` |
|   3262104 |  396 | `	pHash->nEntry++;` |
|   3262104 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   3262102 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   3262104 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     24682 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     24682 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     12340 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   3262104 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   3262104 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   3262104 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   3262104 |  421 | `	pEntry->pHash = pHash;` |
|   3262104 |  422 | `	pEntry->pKey = pKey;` |
|   3262104 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   3262104 |  424 | `	pEntry->pUserData = pUserData;` |
|   3262104 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   3262104 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   3262104 |  428 | `	return rc;` |
|   1631053 |  429 |  |
|    133470 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    133472 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
