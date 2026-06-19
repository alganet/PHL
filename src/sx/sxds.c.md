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
|  17677990 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  17677992 |   16 | `	pSet->nSize = 0 ;` |
|  17677992 |   17 | `	pSet->nUsed = 0;` |
|  17677992 |   18 | `	pSet->nCursor = 0;` |
|  17677992 |   19 | `	pSet->eSize = ElemSize;` |
|  17677992 |   20 | `	pSet->pAllocator = pAllocator;` |
|  17677992 |   21 | `	pSet->pBase =  0;` |
|  17677992 |   22 | `	pSet->pUserData = 0;` |
|  17677992 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  29013548 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  29013550 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4326930 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4326930 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4185538 |   34 | `			pSet->nSize = 4;` |
|   2092768 |   35 | `		}` |
|   4326930 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4326930 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4326930 |   40 | `		pSet->pBase = pNew;` |
|   4326930 |   41 | `		pSet->nSize <<= 1;` |
|   2163464 |   42 | `	}` |
|  29013550 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 217207512 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  29013550 |   45 | `	pSet->nUsed++;` |
|  29013550 |   46 | `	return SXRET_OK;` |
|  14506798 |   47 |  |
|   1165850 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|   1165852 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1165852 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1165852 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1165852 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1165852 |   60 | `	pSet->nSize = nItem;` |
|   1165852 |   61 | `	return SXRET_OK;` |
|    582927 |   62 |  |
|   1648028 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1648030 |   65 | `	pSet->nUsed   = 0;` |
|   1648030 |   66 | `	pSet->nCursor = 0;` |
|   1648030 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     54058 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     54060 |   71 | `	pSet->nCursor = 0;` |
|     54060 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     58236 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     58238 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     22280 |   79 | `		pSet->nCursor = 0;` |
|     22280 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     35960 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     35960 |   83 | `	if( ppEntry ){` |
|     35960 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     17979 |   85 | `	}` |
|     35960 |   86 | `	pSet->nCursor++;` |
|     35960 |   87 | `	return SXRET_OK;` |
|     29120 |   88 |  |
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
|    197024 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    197026 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       116 |  103 | `		pSet->nUsed = nNewSize;` |
|        57 |  104 | `	}` |
|    197026 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   9464510 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   9464512 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9464512 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4740474 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2370236 |  112 | `	}` |
|   9464512 |  113 | `	pSet->pBase = 0;` |
|   9464512 |  114 | `	pSet->nUsed = 0;` |
|   9464512 |  115 | `	pSet->nCursor = 0;` |
|   9464512 |  116 | `	return rc;` |
|         2 |  117 |  |
|   5343938 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5343940 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       112 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5343830 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5343830 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2671971 |  126 |  |
|   3454208 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3454210 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2152070 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1302142 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1302142 |  135 | `	pSet->nUsed--;` |
|   1302142 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1302142 |  137 | `	return pData;` |
|   1727106 |  138 |  |
|  12611555 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  12611557 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  12611557 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  12611557 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6305910 |  148 |  |
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
|    506278 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    506280 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    506280 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    506280 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    506280 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    506280 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    506280 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    506280 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    506280 |  180 | `	pHash->nEntry = 0;` |
|    506280 |  181 | `	pHash->apBucket = apNew;` |
|    506280 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    506280 |  183 | `	return SXRET_OK;` |
|    253141 |  184 |  |
|     93360 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     93362 |  193 | `	pEntry = pHash->pList;` |
|     49820 |  194 | `	for(;;){` |
|     99642 |  195 | `		if( pHash->nEntry == 0 ){` |
|     93362 |  196 | `			break;` |
|         - |  197 | `		}` |
|      6282 |  198 | `		pNext = pEntry->pNext;` |
|      6282 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      6282 |  200 | `		pEntry = pNext;` |
|      6282 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     93362 |  203 | `	if( pHash->apBucket ){` |
|     93362 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     46680 |  205 | `	}` |
|     93362 |  206 | `	pHash->apBucket = 0;` |
|     93362 |  207 | `	pHash->nBucketSize = 0;` |
|     93362 |  208 | `	pHash->pAllocator = 0;` |
|     93362 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  15884884 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  15884886 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  15884886 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  14041069 |  218 | `	for(;;){` |
|  28057507 |  219 | `		if( pEntry == 0 ){` |
|   8362498 |  220 | `			break;` |
|         - |  221 | `		}` |
|  23456075 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   7522392 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   7522390 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  12172623 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   8362498 |  229 | `	return 0;` |
|   7942708 |  230 |  |
|  16628834 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  16628836 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    744114 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  15884724 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  15884724 |  244 | `	if( pEntry == 0 ){` |
|   8362498 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   7522228 |  247 | `	return (SyHashEntry *)pEntry;` |
|   8314683 |  248 |  |
|    111634 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    111636 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     85452 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     42727 |  254 | `	}else{` |
|     26186 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    111636 |  257 | `	if( pEntry->pNextCollide ){` |
|      4995 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2497 |  259 | `	}` |
|    111636 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    111636 |  261 | `	pHash->nEntry--;` |
|    111636 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    111636 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    111636 |  268 | `	return rc;` |
|         2 |  269 |  |
|       162 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       164 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       164 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       164 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       164 |  284 | `	return rc;` |
|        83 |  285 |  |
|    111472 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|    111474 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    111474 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    111474 |  296 | `	return rc;` |
|         2 |  297 |  |
|   1030222 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|   1030224 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   1030224 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   6421228 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   6421230 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1029788 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   1029788 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   5391444 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   5391444 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   5391444 |  324 | `	return (SyHashEntry *)pEntry;` |
|   3210616 |  325 |  |
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
|     26272 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     26274 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     26274 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     26274 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     26274 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3338754 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3312482 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3312482 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3312482 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3312482 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1577721 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    788816 |  371 | `		}` |
|   3312482 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3312482 |  374 | `		pEntry = pEntry->pNext;` |
|   1656242 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     26274 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     26274 |  378 | `	pHash->apBucket = apNew;` |
|     26274 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     26274 |  380 | `	return SXRET_OK;` |
|     13138 |  381 |  |
|   4284786 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   4284788 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   4284788 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4284788 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2393100 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1196580 |  389 | `	}` |
|   4284788 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   4284788 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   4284788 |  393 | `	if( pHash->nEntry == 0 ){` |
|    273546 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    136772 |  395 | `	}` |
|   4284788 |  396 | `	pHash->nEntry++;` |
|   4284788 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   4284786 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   4284788 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     26274 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     26274 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     13136 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   4284788 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4284788 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   4284788 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4284788 |  421 | `	pEntry->pHash = pHash;` |
|   4284788 |  422 | `	pEntry->pKey = pKey;` |
|   4284788 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   4284788 |  424 | `	pEntry->pUserData = pUserData;` |
|   4284788 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4284788 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   4284788 |  428 | `	return rc;` |
|   2142395 |  429 |  |
|    141116 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    141118 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
