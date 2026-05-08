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
|  16669266 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  16669268 |   16 | `	pSet->nSize = 0 ;` |
|  16669268 |   17 | `	pSet->nUsed = 0;` |
|  16669268 |   18 | `	pSet->nCursor = 0;` |
|  16669268 |   19 | `	pSet->eSize = ElemSize;` |
|  16669268 |   20 | `	pSet->pAllocator = pAllocator;` |
|  16669268 |   21 | `	pSet->pBase =  0;` |
|  16669268 |   22 | `	pSet->pUserData = 0;` |
|  16669268 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  27338684 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  27338686 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4150012 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4150012 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4014990 |   34 | `			pSet->nSize = 4;` |
|   2007494 |   35 | `		}` |
|   4150012 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4150012 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4150012 |   40 | `		pSet->pBase = pNew;` |
|   4150012 |   41 | `		pSet->nSize <<= 1;` |
|   2075005 |   42 | `	}` |
|  27338686 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 203771036 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  27338686 |   45 | `	pSet->nUsed++;` |
|  27338686 |   46 | `	return SXRET_OK;` |
|  13669366 |   47 |  |
|   1082300 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|   1082302 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1082302 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1082302 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1082302 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1082302 |   60 | `	pSet->nSize = nItem;` |
|   1082302 |   61 | `	return SXRET_OK;` |
|    541152 |   62 |  |
|   1582092 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1582094 |   65 | `	pSet->nUsed   = 0;` |
|   1582094 |   66 | `	pSet->nCursor = 0;` |
|   1582094 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     52236 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     52238 |   71 | `	pSet->nCursor = 0;` |
|     52238 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     56318 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     56320 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     21498 |   79 | `		pSet->nCursor = 0;` |
|     21498 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     34824 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     34824 |   83 | `	if( ppEntry ){` |
|     34824 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     17411 |   85 | `	}` |
|     34824 |   86 | `	pSet->nCursor++;` |
|     34824 |   87 | `	return SXRET_OK;` |
|     28161 |   88 |  |
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
|    187910 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    187912 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       110 |  103 | `		pSet->nUsed = nNewSize;` |
|        54 |  104 | `	}` |
|    187912 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   9160616 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   9160618 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9160618 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4629070 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2314534 |  112 | `	}` |
|   9160618 |  113 | `	pSet->pBase = 0;` |
|   9160618 |  114 | `	pSet->nUsed = 0;` |
|   9160618 |  115 | `	pSet->nCursor = 0;` |
|   9160618 |  116 | `	return rc;` |
|         2 |  117 |  |
|   5160398 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5160400 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       108 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5160294 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5160294 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2580201 |  126 |  |
|   3400482 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3400484 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2149590 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1250896 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1250896 |  135 | `	pSet->nUsed--;` |
|   1250896 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1250896 |  137 | `	return pData;` |
|   1700243 |  138 |  |
|  12058518 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  12058520 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  12058520 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  12058520 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6029414 |  148 |  |
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
|    302892 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    302894 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    302894 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    302894 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    302894 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    302894 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    302894 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    302894 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    302894 |  180 | `	pHash->nEntry = 0;` |
|    302894 |  181 | `	pHash->apBucket = apNew;` |
|    302894 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    302894 |  183 | `	return SXRET_OK;` |
|    151448 |  184 |  |
|     89186 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     89188 |  193 | `	pEntry = pHash->pList;` |
|     47447 |  194 | `	for(;;){` |
|     94896 |  195 | `		if( pHash->nEntry == 0 ){` |
|     89188 |  196 | `			break;` |
|         - |  197 | `		}` |
|      5710 |  198 | `		pNext = pEntry->pNext;` |
|      5710 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      5710 |  200 | `		pEntry = pNext;` |
|      5710 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     89188 |  203 | `	if( pHash->apBucket ){` |
|     89188 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     44593 |  205 | `	}` |
|     89188 |  206 | `	pHash->apBucket = 0;` |
|     89188 |  207 | `	pHash->nBucketSize = 0;` |
|     89188 |  208 | `	pHash->pAllocator = 0;` |
|     89188 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  13591382 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  13591384 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  13591384 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  12014158 |  218 | `	for(;;){` |
|  24056107 |  219 | `		if( pEntry == 0 ){` |
|   7420674 |  220 | `			break;` |
|         - |  221 | `		}` |
|  19720660 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   6170714 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   6170712 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  10464725 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   7420674 |  229 | `	return 0;` |
|   6795957 |  230 |  |
|  14164204 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  14164206 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    572972 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  13591236 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  13591236 |  244 | `	if( pEntry == 0 ){` |
|   7420674 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   6170564 |  247 | `	return (SyHashEntry *)pEntry;` |
|   7082368 |  248 |  |
|    108060 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    108062 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     82712 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     41357 |  254 | `	}else{` |
|     25352 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    108062 |  257 | `	if( pEntry->pNextCollide ){` |
|      4759 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2379 |  259 | `	}` |
|    108062 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    108062 |  261 | `	pHash->nEntry--;` |
|    108062 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    108062 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    108062 |  268 | `	return rc;` |
|         2 |  269 |  |
|       148 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       150 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       150 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       150 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       150 |  284 | `	return rc;` |
|        76 |  285 |  |
|    107912 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|    107914 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    107914 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    107914 |  296 | `	return rc;` |
|         2 |  297 |  |
|    378600 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    378602 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    378602 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2977092 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2977094 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    378166 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    378166 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2598930 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2598930 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2598930 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1488548 |  325 |  |
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
|      1801 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1791 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1791 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1791 |  344 | `		pEntry = pEntry->pNext;` |
|       896 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     24986 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     24988 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     24988 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     24988 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     24988 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3173308 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3148322 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3148322 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3148322 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3148322 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1504921 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    752386 |  371 | `		}` |
|   3148322 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3148322 |  374 | `		pEntry = pEntry->pNext;` |
|   1574162 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     24988 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     24988 |  378 | `	pHash->apBucket = apNew;` |
|     24988 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     24988 |  380 | `	return SXRET_OK;` |
|     12495 |  381 |  |
|   3308616 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   3308618 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   3308618 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   3308618 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2131850 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1065986 |  389 | `	}` |
|   3308618 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   3308618 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   3308618 |  393 | `	if( pHash->nEntry == 0 ){` |
|    150248 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     75123 |  395 | `	}` |
|   3308618 |  396 | `	pHash->nEntry++;` |
|   3308618 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   3308616 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   3308618 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     24988 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     24988 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     12493 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   3308618 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   3308618 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   3308618 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   3308618 |  421 | `	pEntry->pHash = pHash;` |
|   3308618 |  422 | `	pEntry->pKey = pKey;` |
|   3308618 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   3308618 |  424 | `	pEntry->pUserData = pUserData;` |
|   3308618 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   3308618 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   3308618 |  428 | `	return rc;` |
|   1654310 |  429 |  |
|    136000 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    136002 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
