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
|   9548272 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|   9548274 |   16 | `	pSet->nSize = 0 ;` |
|   9548274 |   17 | `	pSet->nUsed = 0;` |
|   9548274 |   18 | `	pSet->nCursor = 0;` |
|   9548274 |   19 | `	pSet->eSize = ElemSize;` |
|   9548274 |   20 | `	pSet->pAllocator = pAllocator;` |
|   9548274 |   21 | `	pSet->pBase =  0;` |
|   9548274 |   22 | `	pSet->pUserData = 0;` |
|   9548274 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  15072706 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  15072708 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3157658 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3157658 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3101578 |   34 | `			pSet->nSize = 4;` |
|   1550788 |   35 | `		}` |
|   3157658 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3157658 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3157658 |   40 | `		pSet->pBase = pNew;` |
|   3157658 |   41 | `		pSet->nSize <<= 1;` |
|   1578828 |   42 | `	}` |
|  15072708 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 114453880 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  15072708 |   45 | `	pSet->nUsed++;` |
|  15072708 |   46 | `	return SXRET_OK;` |
|   7536377 |   47 |  |
|    402566 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    402568 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    402568 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    402568 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    402568 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    402568 |   60 | `	pSet->nSize = nItem;` |
|    402568 |   61 | `	return SXRET_OK;` |
|    201285 |   62 |  |
|    798154 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|    798156 |   65 | `	pSet->nUsed   = 0;` |
|    798156 |   66 | `	pSet->nCursor = 0;` |
|    798156 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     32666 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     32668 |   71 | `	pSet->nCursor = 0;` |
|     32668 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     35714 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     35716 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     13038 |   79 | `		pSet->nCursor = 0;` |
|     13038 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     22680 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     22680 |   83 | `	if( ppEntry ){` |
|     22680 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     11339 |   85 | `	}` |
|     22680 |   86 | `	pSet->nCursor++;` |
|     22680 |   87 | `	return SXRET_OK;` |
|     17859 |   88 |  |
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
|     50790 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     50792 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     50792 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   6537792 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   6537794 |  109 | `	sxi32 rc = SXRET_OK;` |
|   6537794 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3354826 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1677412 |  112 | `	}` |
|   6537794 |  113 | `	pSet->pBase = 0;` |
|   6537794 |  114 | `	pSet->nUsed = 0;` |
|   6537794 |  115 | `	pSet->nCursor = 0;` |
|   6537794 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3205236 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3205238 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3205148 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3205148 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1602620 |  126 |  |
|   2867760 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   2867762 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2119038 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    748726 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    748726 |  135 | `	pSet->nUsed--;` |
|    748726 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    748726 |  137 | `	return pData;` |
|   1433882 |  138 |  |
|   7945872 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   7945874 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   7945874 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   7945874 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   3973136 |  148 |  |
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
|     72220 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     72222 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     72222 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     72222 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     72222 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     72222 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     72222 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     72222 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     72222 |  180 | `	pHash->nEntry = 0;` |
|     72222 |  181 | `	pHash->apBucket = apNew;` |
|     72222 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     72222 |  183 | `	return SXRET_OK;` |
|     36112 |  184 |  |
|      9186 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|      9188 |  193 | `	pEntry = pHash->pList;` |
|      5224 |  194 | `	for(;;){` |
|     10450 |  195 | `		if( pHash->nEntry == 0 ){` |
|      9188 |  196 | `			break;` |
|         - |  197 | `		}` |
|      1264 |  198 | `		pNext = pEntry->pNext;` |
|      1264 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      1264 |  200 | `		pEntry = pNext;` |
|      1264 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|      9188 |  203 | `	if( pHash->apBucket ){` |
|      9188 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      4593 |  205 | `	}` |
|      9188 |  206 | `	pHash->apBucket = 0;` |
|      9188 |  207 | `	pHash->nBucketSize = 0;` |
|      9188 |  208 | `	pHash->pAllocator = 0;` |
|      9188 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   7228982 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   7228984 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   7228984 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   6234890 |  218 | `	for(;;){` |
|  12666514 |  219 | `		if( pEntry == 0 ){` |
|   3913446 |  220 | `			break;` |
|         - |  221 | `		}` |
|  10410709 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   3315542 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   3315540 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   5437532 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   3913446 |  229 | `	return 0;` |
|   3614757 |  230 |  |
|   7269848 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   7269850 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     40874 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   7228978 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   7228978 |  244 | `	if( pEntry == 0 ){` |
|   3913446 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   3315534 |  247 | `	return (SyHashEntry *)pEntry;` |
|   3635190 |  248 |  |
|     59282 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     59284 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     44354 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     22178 |  254 | `	}else{` |
|     14932 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     59284 |  257 | `	if( pEntry->pNextCollide ){` |
|      3565 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      1782 |  259 | `	}` |
|     59284 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     59284 |  261 | `	pHash->nEntry--;` |
|     59284 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     59284 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     59284 |  268 | `	return rc;` |
|         2 |  269 |  |
|         6 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         1 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|         7 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|         7 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|         7 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|         7 |  284 | `	return rc;` |
|         4 |  285 |  |
|     59276 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     59278 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     59278 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     59278 |  296 | `	return rc;` |
|         2 |  297 |  |
|    106580 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    106582 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    106582 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    744826 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    744828 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    106148 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    106148 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    638682 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    638682 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    638682 |  324 | `	return (SyHashEntry *)pEntry;` |
|    372415 |  325 |  |
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
|      1577 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1567 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1567 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1567 |  344 | `		pEntry = pEntry->pNext;` |
|       784 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     10044 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     10046 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     10046 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     10046 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     10046 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1373438 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1363394 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1363394 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1363394 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1363394 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    654748 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    327381 |  371 | `		}` |
|   1363394 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1363394 |  374 | `		pEntry = pEntry->pNext;` |
|    681698 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     10046 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     10046 |  378 | `	pHash->apBucket = apNew;` |
|     10046 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     10046 |  380 | `	return SXRET_OK;` |
|      5024 |  381 |  |
|   1245250 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1245252 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1245252 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1245252 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    827875 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    413917 |  389 | `	}` |
|   1245252 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1245252 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1245252 |  393 | `	if( pHash->nEntry == 0 ){` |
|     51700 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     25849 |  395 | `	}` |
|   1245252 |  396 | `	pHash->nEntry++;` |
|   1245252 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1245250 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1245252 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     10046 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     10046 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      5022 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1245252 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1245252 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1245252 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1245252 |  421 | `	pEntry->pHash = pHash;` |
|   1245252 |  422 | `	pEntry->pKey = pKey;` |
|   1245252 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1245252 |  424 | `	pEntry->pUserData = pUserData;` |
|   1245252 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1245252 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1245252 |  428 | `	return rc;` |
|    622627 |  429 |  |
|     71938 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     71940 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
