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
|  14040666 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  14040668 |   16 | `	pSet->nSize = 0 ;` |
|  14040668 |   17 | `	pSet->nUsed = 0;` |
|  14040668 |   18 | `	pSet->nCursor = 0;` |
|  14040668 |   19 | `	pSet->eSize = ElemSize;` |
|  14040668 |   20 | `	pSet->pAllocator = pAllocator;` |
|  14040668 |   21 | `	pSet->pBase =  0;` |
|  14040668 |   22 | `	pSet->pUserData = 0;` |
|  14040668 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  23184654 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  23184656 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3866346 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3866346 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3760602 |   34 | `			pSet->nSize = 4;` |
|   1880300 |   35 | `		}` |
|   3866346 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3866346 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3866346 |   40 | `		pSet->pBase = pNew;` |
|   3866346 |   41 | `		pSet->nSize <<= 1;` |
|   1933172 |   42 | `	}` |
|  23184656 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 172288176 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  23184656 |   45 | `	pSet->nUsed++;` |
|  23184656 |   46 | `	return SXRET_OK;` |
|  11592351 |   47 |  |
|    833222 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    833224 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    833224 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    833224 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    833224 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    833224 |   60 | `	pSet->nSize = nItem;` |
|    833224 |   61 | `	return SXRET_OK;` |
|    416613 |   62 |  |
|   1283806 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1283808 |   65 | `	pSet->nUsed   = 0;` |
|   1283808 |   66 | `	pSet->nCursor = 0;` |
|   1283808 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     44504 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     44506 |   71 | `	pSet->nCursor = 0;` |
|     44506 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     48586 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     48588 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     18234 |   79 | `		pSet->nCursor = 0;` |
|     18234 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     30356 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     30356 |   83 | `	if( ppEntry ){` |
|     30356 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     15177 |   85 | `	}` |
|     30356 |   86 | `	pSet->nCursor++;` |
|     30356 |   87 | `	return SXRET_OK;` |
|     24295 |   88 |  |
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
|    138440 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    138442 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|    138442 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8343760 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8343762 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8343762 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4256694 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2128346 |  112 | `	}` |
|   8343762 |  113 | `	pSet->pBase = 0;` |
|   8343762 |  114 | `	pSet->nUsed = 0;` |
|   8343762 |  115 | `	pSet->nCursor = 0;` |
|   8343762 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4507296 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4507298 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4507208 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4507208 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2253650 |  126 |  |
|   3271922 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3271924 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2142390 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1129536 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1129536 |  135 | `	pSet->nUsed--;` |
|   1129536 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1129536 |  137 | `	return pData;` |
|   1635963 |  138 |  |
|  10772551 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  10772553 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  10772553 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  10772553 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5386484 |  148 |  |
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
|    247780 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    247782 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    247782 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    247782 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    247782 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    247782 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    247782 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    247782 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    247782 |  180 | `	pHash->nEntry = 0;` |
|    247782 |  181 | `	pHash->apBucket = apNew;` |
|    247782 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    247782 |  183 | `	return SXRET_OK;` |
|    123892 |  184 |  |
|     74774 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     74776 |  193 | `	pEntry = pHash->pList;` |
|     39172 |  194 | `	for(;;){` |
|     78346 |  195 | `		if( pHash->nEntry == 0 ){` |
|     74776 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3572 |  198 | `		pNext = pEntry->pNext;` |
|      3572 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3572 |  200 | `		pEntry = pNext;` |
|      3572 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     74776 |  203 | `	if( pHash->apBucket ){` |
|     74776 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     37387 |  205 | `	}` |
|     74776 |  206 | `	pHash->apBucket = 0;` |
|     74776 |  207 | `	pHash->nBucketSize = 0;` |
|     74776 |  208 | `	pHash->pAllocator = 0;` |
|     74776 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  11579552 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  11579554 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  11579554 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  10583759 |  218 | `	for(;;){` |
|  21100488 |  219 | `		if( pEntry == 0 ){` |
|   6381010 |  220 | `			break;` |
|         - |  221 | `		}` |
|  17318622 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5198548 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5198546 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   9520936 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   6381010 |  229 | `	return 0;` |
|   5790042 |  230 |  |
|  12028290 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  12028292 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    448762 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  11579532 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  11579532 |  244 | `	if( pEntry == 0 ){` |
|   6381010 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5198524 |  247 | `	return (SyHashEntry *)pEntry;` |
|   6014411 |  248 |  |
|     85928 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     85930 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     65226 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     32614 |  254 | `	}else{` |
|     20706 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     85930 |  257 | `	if( pEntry->pNextCollide ){` |
|      4479 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2239 |  259 | `	}` |
|     85930 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     85930 |  261 | `	pHash->nEntry--;` |
|     85930 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     85930 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     85930 |  268 | `	return rc;` |
|         2 |  269 |  |
|        22 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|        24 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        24 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|        24 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        24 |  284 | `	return rc;` |
|        13 |  285 |  |
|     85906 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     85908 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     85908 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     85908 |  296 | `	return rc;` |
|         2 |  297 |  |
|    298394 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    298396 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    298396 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2308572 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2308574 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    297962 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    297962 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2010614 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2010614 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2010614 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1154288 |  325 |  |
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
|      1773 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1763 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1763 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1763 |  344 | `		pEntry = pEntry->pNext;` |
|       882 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     21676 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     21678 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     21678 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     21678 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     21678 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2752974 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2731298 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2731298 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2731298 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2731298 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1305298 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    652607 |  371 | `		}` |
|   2731298 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2731298 |  374 | `		pEntry = pEntry->pNext;` |
|   1365650 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     21678 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     21678 |  378 | `	pHash->apBucket = apNew;` |
|     21678 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     21678 |  380 | `	return SXRET_OK;` |
|     10840 |  381 |  |
|   2797004 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2797006 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2797006 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2797006 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1812418 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    906189 |  389 | `	}` |
|   2797006 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2797006 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2797006 |  393 | `	if( pHash->nEntry == 0 ){` |
|    124404 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     62201 |  395 | `	}` |
|   2797006 |  396 | `	pHash->nEntry++;` |
|   2797006 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2797004 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2797006 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     21678 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     21678 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     10838 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2797006 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2797006 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2797006 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2797006 |  421 | `	pEntry->pHash = pHash;` |
|   2797006 |  422 | `	pEntry->pKey = pKey;` |
|   2797006 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2797006 |  424 | `	pEntry->pUserData = pUserData;` |
|   2797006 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2797006 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2797006 |  428 | `	return rc;` |
|   1398504 |  429 |  |
|    110254 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    110256 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
