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
|  11325204 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         1 |   15 |  |
|  11325205 |   16 | `	pSet->nSize = 0 ;` |
|  11325205 |   17 | `	pSet->nUsed = 0;` |
|  11325205 |   18 | `	pSet->nCursor = 0;` |
|  11325205 |   19 | `	pSet->eSize = ElemSize;` |
|  11325205 |   20 | `	pSet->pAllocator = pAllocator;` |
|  11325205 |   21 | `	pSet->pBase =  0;` |
|  11325205 |   22 | `	pSet->pUserData = 0;` |
|  11325205 |   23 | `	return SXRET_OK;` |
|         1 |   24 |  |
|  22411962 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         1 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  22411963 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   1324405 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   1324405 |   33 | `		if( pSet->nSize <= 0 ){` |
|   1153231 |   34 | `			pSet->nSize = 4;` |
|    576615 |   35 | `		}` |
|   1324405 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   1324405 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   1324405 |   40 | `		pSet->pBase = pNew;` |
|   1324405 |   41 | `		pSet->nSize <<= 1;` |
|    662202 |   42 | `	}` |
|  22411963 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 169015991 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  22411963 |   45 | `	pSet->nUsed++;` |
|  22411963 |   46 | `	return SXRET_OK;` |
|  11205982 |   47 |  |
|   1305466 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         1 |   49 |  |
|   1305467 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1305467 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1305467 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1305467 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1305467 |   60 | `	pSet->nSize = nItem;` |
|   1305467 |   61 | `	return SXRET_OK;` |
|    652734 |   62 |  |
|   1148454 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         1 |   64 |  |
|   1148455 |   65 | `	pSet->nUsed   = 0;` |
|   1148455 |   66 | `	pSet->nCursor = 0;` |
|   1148455 |   67 | `	return SXRET_OK;` |
|         1 |   68 |  |
|       106 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         1 |   70 |  |
|       107 |   71 | `	pSet->nCursor = 0;` |
|       107 |   72 | `	return SXRET_OK;` |
|         1 |   73 |  |
|       304 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         1 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|       305 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|        71 |   79 | `		pSet->nCursor = 0;` |
|        71 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|       235 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|       235 |   83 | `	if( ppEntry ){` |
|       235 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|       117 |   85 | `	}` |
|       235 |   86 | `	pSet->nCursor++;` |
|       235 |   87 | `	return SXRET_OK;` |
|       153 |   88 |  |
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
|    181940 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         1 |  101 |  |
|    181941 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        19 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|    181941 |  105 | `	return SXRET_OK;` |
|         1 |  106 |  |
|   3337192 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         1 |  108 |  |
|   3337193 |  109 | `	sxi32 rc = SXRET_OK;` |
|   3337193 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   1792989 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|    896494 |  112 | `	}` |
|   3337193 |  113 | `	pSet->pBase = 0;` |
|   3337193 |  114 | `	pSet->nUsed = 0;` |
|   3337193 |  115 | `	pSet->nCursor = 0;` |
|   3337193 |  116 | `	return rc;` |
|         1 |  117 |  |
|   3792742 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         1 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3792743 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        93 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3792651 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3792651 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1896372 |  126 |  |
|    495924 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         1 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|    495925 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    188863 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    307063 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    307063 |  135 | `	pSet->nUsed--;` |
|    307063 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    307063 |  137 | `	return pData;` |
|    247963 |  138 |  |
|    946404 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         1 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|    946405 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|    946405 |  146 | `	zBase = (const char *)pSet->pBase;` |
|    946405 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|    473203 |  148 |  |
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
|    162768 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         1 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    162769 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    162769 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    162769 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    162769 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    162769 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    162769 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    162769 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    162769 |  180 | `	pHash->nEntry = 0;` |
|    162769 |  181 | `	pHash->apBucket = apNew;` |
|    162769 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    162769 |  183 | `	return SXRET_OK;` |
|     81385 |  184 |  |
|      1432 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         1 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|      1433 |  193 | `	pEntry = pHash->pList;` |
|       804 |  194 | `	for(;;){` |
|      1609 |  195 | `		if( pHash->nEntry == 0 ){` |
|      1433 |  196 | `			break;` |
|         - |  197 | `		}` |
|       177 |  198 | `		pNext = pEntry->pNext;` |
|       177 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|       177 |  200 | `		pEntry = pNext;` |
|       177 |  201 | `		pHash->nEntry--;` |
|         1 |  202 | `	}` |
|      1433 |  203 | `	if( pHash->apBucket ){` |
|      1433 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|       716 |  205 | `	}` |
|      1433 |  206 | `	pHash->apBucket = 0;` |
|      1433 |  207 | `	pHash->nBucketSize = 0;` |
|      1433 |  208 | `	pHash->pAllocator = 0;` |
|      1433 |  209 | `	return SXRET_OK;` |
|         1 |  210 |  |
|   6549372 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         1 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   6549373 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   6549373 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   7511673 |  218 | `	for(;;){` |
|  15013547 |  219 | `		if( pEntry == 0 ){` |
|   4513475 |  220 | `			break;` |
|         - |  221 | `		}` |
|  11518022 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   2035898 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   2035899 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   8464175 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         1 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   4513475 |  229 | `	return 0;` |
|   3274687 |  230 |  |
|   6623374 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         1 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   6623375 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     74009 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   6549367 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   6549367 |  244 | `	if( pEntry == 0 ){` |
|   4513475 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   2035893 |  247 | `	return (SyHashEntry *)pEntry;` |
|   3311688 |  248 |  |
|      2118 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         1 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|      2119 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|      2051 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      1026 |  254 | `	}else{` |
|        69 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|      2119 |  257 | `	if( pEntry->pNextCollide ){` |
|         3 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|         1 |  259 | `	}` |
|      2119 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|      2119 |  261 | `	pHash->nEntry--;` |
|      2119 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|      2119 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      2119 |  268 | `	return rc;` |
|         1 |  269 |  |
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
|      2112 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         1 |  287 |  |
|      2113 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|      2113 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|      2113 |  296 | `	return rc;` |
|         1 |  297 |  |
|    137576 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         1 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    137577 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    137577 |  306 | `	return SXRET_OK;` |
|         1 |  307 |  |
|    761690 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         1 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    761691 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    137143 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    137143 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    624549 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    624549 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    624549 |  324 | `	return (SyHashEntry *)pEntry;` |
|    380846 |  325 |  |
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
|      1373 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1363 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1363 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1363 |  344 | `		pEntry = pEntry->pNext;` |
|       682 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     40992 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         1 |  349 |  |
|     40993 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     40993 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     40993 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     40993 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   5674849 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   5633857 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   5633857 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   5633857 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   5633857 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   2704872 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|   1352434 |  371 | `		}` |
|   5633857 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   5633857 |  374 | `		pEntry = pEntry->pNext;` |
|   2816929 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     40993 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     40993 |  378 | `	pHash->apBucket = apNew;` |
|     40993 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     40993 |  380 | `	return SXRET_OK;` |
|     20497 |  381 |  |
|   4351780 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         1 |  383 |  |
|   4351781 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   4351781 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4351781 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   3253855 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1626984 |  389 | `	}` |
|   4351781 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   4351781 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   4351781 |  393 | `	if( pHash->nEntry == 0 ){` |
|    104415 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     52207 |  395 | `	}` |
|   4351781 |  396 | `	pHash->nEntry++;` |
|   4351781 |  397 | `	return SXRET_OK;` |
|         1 |  398 |  |
|   4351780 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         1 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   4351781 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     40993 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     40993 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     20496 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   4351781 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4351781 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   4351781 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4351781 |  421 | `	pEntry->pHash = pHash;` |
|   4351781 |  422 | `	pEntry->pKey = pKey;` |
|   4351781 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   4351781 |  424 | `	pEntry->pUserData = pUserData;` |
|   4351781 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4351781 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   4351781 |  428 | `	return rc;` |
|   2175891 |  429 |  |
|     57854 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         1 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     57855 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         1 |  439 |  |
|         - |  440 |  |
