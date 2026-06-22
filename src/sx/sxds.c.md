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
|  18217344 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 |  |
|  18217349 |   16 | `	pSet->nSize = 0 ;` |
|  18217349 |   17 | `	pSet->nUsed = 0;` |
|  18217349 |   18 | `	pSet->nCursor = 0;` |
|  18217349 |   19 | `	pSet->eSize = ElemSize;` |
|  18217349 |   20 | `	pSet->pAllocator = pAllocator;` |
|  18217349 |   21 | `	pSet->pBase =  0;` |
|  18217349 |   22 | `	pSet->pUserData = 0;` |
|  18217349 |   23 | `	return SXRET_OK;` |
|         5 |   24 |  |
|  29925769 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  29925774 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4417073 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4417073 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4270107 |   34 | `			pSet->nSize = 4;` |
|   2135051 |   35 | `		}` |
|   4417073 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4417073 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4417073 |   40 | `		pSet->pBase = pNew;` |
|   4417073 |   41 | `		pSet->nSize <<= 1;` |
|   2208534 |   42 | `	}` |
|  29925774 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 224036450 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  29925774 |   45 | `	pSet->nUsed++;` |
|  29925774 |   46 | `	return SXRET_OK;` |
|  14962932 |   47 |  |
|   1213264 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 |  |
|   1213269 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1213269 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1213269 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1213269 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1213269 |   60 | `	pSet->nSize = nItem;` |
|   1213269 |   61 | `	return SXRET_OK;` |
|    606637 |   62 |  |
|   1701999 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 |  |
|   1702004 |   65 | `	pSet->nUsed   = 0;` |
|   1702004 |   66 | `	pSet->nCursor = 0;` |
|   1702004 |   67 | `	return SXRET_OK;` |
|         5 |   68 |  |
|     55348 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 |  |
|     55353 |   71 | `	pSet->nCursor = 0;` |
|     55353 |   72 | `	return SXRET_OK;` |
|         5 |   73 |  |
|     59532 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     59537 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     22845 |   79 | `		pSet->nCursor = 0;` |
|     22845 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     36697 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     36697 |   83 | `	if( ppEntry ){` |
|     36697 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     18346 |   85 | `	}` |
|     36697 |   86 | `	pSet->nCursor++;` |
|     36697 |   87 | `	return SXRET_OK;` |
|     29771 |   88 |  |
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
|    205374 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 |  |
|    205379 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       118 |  103 | `		pSet->nUsed = nNewSize;` |
|        57 |  104 | `	}` |
|    205379 |  105 | `	return SXRET_OK;` |
|         5 |  106 |  |
|   9636822 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 |  |
|   9636827 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9636827 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4823057 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2411526 |  112 | `	}` |
|   9636827 |  113 | `	pSet->pBase = 0;` |
|   9636827 |  114 | `	pSet->nUsed = 0;` |
|   9636827 |  115 | `	pSet->nCursor = 0;` |
|   9636827 |  116 | `	return rc;` |
|         5 |  117 |  |
|   5498216 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5498221 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       121 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5498105 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5498105 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2749113 |  126 |  |
|   3508070 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3508075 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2174549 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1333531 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1333531 |  135 | `	pSet->nUsed--;` |
|   1333531 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1333531 |  137 | `	return pData;` |
|   1754040 |  138 |  |
|  12895774 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  12895779 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  12895779 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  12895779 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6448147 |  148 |  |
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
|    527382 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    527387 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    527387 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    527387 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    527387 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    527387 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    527387 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    527387 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    527387 |  180 | `	pHash->nEntry = 0;` |
|    527387 |  181 | `	pHash->apBucket = apNew;` |
|    527387 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    527387 |  183 | `	return SXRET_OK;` |
|    263696 |  184 |  |
|     96860 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     96865 |  193 | `	pEntry = pHash->pList;` |
|     51720 |  194 | `	for(;;){` |
|    103445 |  195 | `		if( pHash->nEntry == 0 ){` |
|     96865 |  196 | `			break;` |
|         - |  197 | `		}` |
|      6585 |  198 | `		pNext = pEntry->pNext;` |
|      6585 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      6585 |  200 | `		pEntry = pNext;` |
|      6585 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|     96865 |  203 | `	if( pHash->apBucket ){` |
|     96865 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     48430 |  205 | `	}` |
|     96865 |  206 | `	pHash->apBucket = 0;` |
|     96865 |  207 | `	pHash->nBucketSize = 0;` |
|     96865 |  208 | `	pHash->pAllocator = 0;` |
|     96865 |  209 | `	return SXRET_OK;` |
|         5 |  210 |  |
|  16459406 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  16459411 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  16459411 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  14523262 |  218 | `	for(;;){` |
|  29083217 |  219 | `		if( pEntry == 0 ){` |
|   8667551 |  220 | `			break;` |
|         - |  221 | `		}` |
|  24311348 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   7791864 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   7791865 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  12623811 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   8667551 |  229 | `	return 0;` |
|   8230218 |  230 |  |
|  17236170 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  17236175 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    776963 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  16459217 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  16459217 |  244 | `	if( pEntry == 0 ){` |
|   8667551 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   7791671 |  247 | `	return (SyHashEntry *)pEntry;` |
|   8618600 |  248 |  |
|    116572 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    116577 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     89519 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     44762 |  254 | `	}else{` |
|     27063 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    116577 |  257 | `	if( pEntry->pNextCollide ){` |
|      4945 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2472 |  259 | `	}` |
|    116577 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    116577 |  261 | `	pHash->nEntry--;` |
|    116577 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    116577 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    116577 |  268 | `	return rc;` |
|         5 |  269 |  |
|       194 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       199 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       199 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       199 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       199 |  284 | `	return rc;` |
|       102 |  285 |  |
|    116378 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  287 |  |
|    116383 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    116383 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    116383 |  296 | `	return rc;` |
|         5 |  297 |  |
|   1071758 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|   1071763 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   1071763 |  306 | `	return SXRET_OK;` |
|         5 |  307 |  |
|   6710952 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   6710957 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1071313 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   1071313 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   5639649 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   5639649 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   5639649 |  324 | `	return (SyHashEntry *)pEntry;` |
|   3355481 |  325 |  |
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
|      1911 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1901 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1901 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1901 |  344 | `		pEntry = pEntry->pNext;` |
|       951 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     27288 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  349 |  |
|     27293 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     27293 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     27293 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     27293 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3465533 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3438245 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3438245 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3438245 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3438245 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1637648 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    818829 |  371 | `		}` |
|   3438245 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3438245 |  374 | `		pEntry = pEntry->pNext;` |
|   1719125 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     27293 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     27293 |  378 | `	pHash->apBucket = apNew;` |
|     27293 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     27293 |  380 | `	return SXRET_OK;` |
|     13649 |  381 |  |
|   4468168 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         5 |  383 |  |
|   4468173 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   4468173 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4468173 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2491440 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1245685 |  389 | `	}` |
|   4468173 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   4468173 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   4468173 |  393 | `	if( pHash->nEntry == 0 ){` |
|    287743 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    143869 |  395 | `	}` |
|   4468173 |  396 | `	pHash->nEntry++;` |
|   4468173 |  397 | `	return SXRET_OK;` |
|         5 |  398 |  |
|   4468168 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   4468173 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     27293 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     27293 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     13644 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   4468173 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4468173 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   4468173 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4468173 |  421 | `	pEntry->pHash = pHash;` |
|   4468173 |  422 | `	pEntry->pKey = pKey;` |
|   4468173 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   4468173 |  424 | `	pEntry->pUserData = pUserData;` |
|   4468173 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4468173 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   4468173 |  428 | `	return rc;` |
|   2234089 |  429 |  |
|    150142 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    150147 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  439 |  |
|         - |  440 |  |
