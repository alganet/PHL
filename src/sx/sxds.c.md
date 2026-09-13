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
|  172878220 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  172878225 |   16 | `	pSet->nSize = 0 ;` |
|  172878225 |   17 | `	pSet->nUsed = 0;` |
|  172878225 |   18 | `	pSet->nCursor = 0;` |
|  172878225 |   19 | `	pSet->eSize = ElemSize;` |
|  172878225 |   20 | `	pSet->pAllocator = pAllocator;` |
|  172878225 |   21 | `	pSet->pBase =  0;` |
|  172878225 |   22 | `	pSet->pUserData = 0;` |
|  172878225 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  391711014 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  391711019 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   22569560 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   22569560 |   33 | `		if( pSet->nSize <= 0 ){` |
|   19225686 |   34 | `			pSet->nSize = 4;` |
|    9613515 |   35 | `		}` |
|   22569560 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   22569560 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   22569560 |   40 | `		pSet->pBase = pNew;` |
|   22569560 |   41 | `		pSet->nSize <<= 1;` |
|   11285452 |   42 | `	}` |
|  391711019 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3089563151 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  391711019 |   45 | `	pSet->nUsed++;` |
|  391711019 |   46 | `	return SXRET_OK;` |
|  195857972 |   47 | `}` |
|   19465202 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   19465207 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   19465207 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   19465207 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   19465207 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   19465207 |   60 | `	pSet->nSize = nItem;` |
|   19465207 |   61 | `	return SXRET_OK;` |
|    9732606 |   62 | `}` |
|   28977407 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   28977412 |   65 | `	pSet->nUsed   = 0;` |
|   28977412 |   66 | `	pSet->nCursor = 0;` |
|   28977412 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      71182 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      71187 |   71 | `	pSet->nCursor = 0;` |
|      71187 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      71600 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      71605 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30815 |   79 | `		pSet->nCursor = 0;` |
|      30815 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      40795 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      40795 |   83 | `	if( ppEntry ){` |
|      40795 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      20395 |   85 | `	}` |
|      40795 |   86 | `	pSet->nCursor++;` |
|      40795 |   87 | `	return SXRET_OK;` |
|      35805 |   88 | `}` |
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
|    3010604 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3010609 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1199 |  103 | `		pSet->nUsed = nNewSize;` |
|        597 |  104 | `	}` |
|    3010609 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   59500086 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   59500091 |  109 | `	sxi32 rc = SXRET_OK;` |
|   59500091 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   32195554 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   16098449 |  112 | `	}` |
|   59500091 |  113 | `	pSet->pBase = 0;` |
|   59500091 |  114 | `	pSet->nUsed = 0;` |
|   59500091 |  115 | `	pSet->nCursor = 0;` |
|   59500091 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   69682282 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   69682287 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19559 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   69662733 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   69662733 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   34841146 |  126 | `}` |
|    9707865 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9707870 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2236309 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7471566 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7471566 |  135 | `	pSet->nUsed--;` |
|    7471566 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7471566 |  137 | `	return pData;` |
|    4854387 |  138 | `}` |
|   35922583 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   35922588 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   35922536 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   35922536 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   17965439 |  148 | `}` |
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
|    1906172 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1906177 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1906177 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1906177 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1906177 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1906177 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1906177 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1906177 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1906177 |  180 | `	pHash->nEntry = 0;` |
|    1906177 |  181 | `	pHash->apBucket = apNew;` |
|    1906177 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1906177 |  183 | `	return SXRET_OK;` |
|     953166 |  184 | `}` |
|     442730 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     442735 |  193 | `	pEntry = pHash->pList;` |
|     236118 |  194 | `	for(;;){` |
|     472091 |  195 | `		if( pHash->nEntry == 0 ){` |
|     442735 |  196 | `			break;` |
|          - |  197 | `		}` |
|      29361 |  198 | `		pNext = pEntry->pNext;` |
|      29361 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      29361 |  200 | `		pEntry = pNext;` |
|      29361 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     442735 |  203 | `	if( pHash->apBucket ){` |
|     442735 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     221440 |  205 | `	}` |
|     442735 |  206 | `	pHash->apBucket = 0;` |
|     442735 |  207 | `	pHash->nBucketSize = 0;` |
|     442735 |  208 | `	pHash->pAllocator = 0;` |
|     442735 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   72763782 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   72763787 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   72763787 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   65484040 |  218 | `	for(;;){` |
|  130948412 |  219 | `		if( pEntry == 0 ){` |
|   26759257 |  220 | `			break;` |
|          - |  221 | `		}` |
|  127190539 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   46008584 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   46004535 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   58184630 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   26759257 |  229 | `	return 0;` |
|   36388535 |  230 | `}` |
|   80205986 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   80205991 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    7442645 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   72763351 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   72763351 |  244 | `	if( pEntry == 0 ){` |
|   26759239 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   46004117 |  247 | `	return (SyHashEntry *)pEntry;` |
|   40109712 |  248 | `}` |
|     488508 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     488513 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     399471 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     200113 |  254 | `	}else{` |
|      89047 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     488513 |  257 | `	if( pEntry->pNextCollide ){` |
|       4630 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2313 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     488513 |  261 | `	if( pHash->pLast == pEntry ){` |
|     478467 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     239681 |  263 | `	}` |
|     488513 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     488513 |  265 | `	pHash->nEntry--;` |
|     488513 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     488513 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     488513 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        436 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        441 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        441 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        423 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        423 |  288 | `	return rc;` |
|        223 |  289 | `}` |
|     488090 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     488095 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     488095 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     488095 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3113538 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3113543 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3113543 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   23945990 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   23945995 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3113277 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3113277 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   20832723 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   20832723 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   20832723 |  328 | `	return (SyHashEntry *)pEntry;` |
|   11973000 |  329 | `}` |
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
|       4563 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4549 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4549 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4549 |  348 | `		pEntry = pEntry->pNext;` |
|       2275 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|     100440 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100445 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100445 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100445 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100445 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18652061 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18551621 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18551621 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18551621 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18551621 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8892566 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4446388 |  375 | `		}` |
|   18551621 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18551621 |  378 | `		pEntry = pEntry->pNext;` |
|    9275813 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100445 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100445 |  382 | `	pHash->apBucket = apNew;` |
|     100445 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100445 |  384 | `	return SXRET_OK;` |
|      50225 |  385 | `}` |
|   20583798 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   20583803 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   20583803 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   20583803 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   13130347 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    6565161 |  393 | `	}` |
|   20583803 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   20583803 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         63 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         63 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         63 |  401 | `		pHash->pLast = pEntry;` |
|         33 |  402 | `	}else{` |
|   20583743 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   20583803 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1067241 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1067241 |  408 | `		pHash->pLast = pEntry;` |
|     533693 |  409 | `	}` |
|   20583803 |  410 | `	pHash->nEntry++;` |
|   20583803 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   20583798 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   20583803 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100445 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100445 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50220 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   20583803 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   20583803 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   20583803 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   20583803 |  435 | `	pEntry->pHash = pHash;` |
|   20583803 |  436 | `	pEntry->pKey = pKey;` |
|   20583803 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   20583803 |  438 | `	pEntry->pUserData = pUserData;` |
|   20583803 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   20583803 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   20583803 |  442 | `	return rc;` |
|   10292354 |  443 | `}` |
|   20583646 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   20583651 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        152 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          4 |  455 | `{` |
|        156 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          4 |  457 | `}` |
|     528600 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     528605 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
