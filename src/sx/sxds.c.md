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
|  10904346 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  10904348 |   16 | `	pSet->nSize = 0 ;` |
|  10904348 |   17 | `	pSet->nUsed = 0;` |
|  10904348 |   18 | `	pSet->nCursor = 0;` |
|  10904348 |   19 | `	pSet->eSize = ElemSize;` |
|  10904348 |   20 | `	pSet->pAllocator = pAllocator;` |
|  10904348 |   21 | `	pSet->pBase =  0;` |
|  10904348 |   22 | `	pSet->pUserData = 0;` |
|  10904348 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  17294730 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  17294732 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3435624 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3435624 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3363758 |   34 | `			pSet->nSize = 4;` |
|   1681878 |   35 | `		}` |
|   3435624 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3435624 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3435624 |   40 | `		pSet->pBase = pNew;` |
|   3435624 |   41 | `		pSet->nSize <<= 1;` |
|   1717811 |   42 | `	}` |
|  17294732 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 129455092 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  17294732 |   45 | `	pSet->nUsed++;` |
|  17294732 |   46 | `	return SXRET_OK;` |
|   8647389 |   47 |  |
|    504034 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    504036 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    504036 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    504036 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    504036 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    504036 |   60 | `	pSet->nSize = nItem;` |
|    504036 |   61 | `	return SXRET_OK;` |
|    252019 |   62 |  |
|    965072 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|    965074 |   65 | `	pSet->nUsed   = 0;` |
|    965074 |   66 | `	pSet->nCursor = 0;` |
|    965074 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     38140 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     38142 |   71 | `	pSet->nCursor = 0;` |
|     38142 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     41924 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     41926 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     15398 |   79 | `		pSet->nCursor = 0;` |
|     15398 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     26530 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     26530 |   83 | `	if( ppEntry ){` |
|     26530 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     13264 |   85 | `	}` |
|     26530 |   86 | `	pSet->nCursor++;` |
|     26530 |   87 | `	return SXRET_OK;` |
|     20964 |   88 |  |
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
|     63828 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     63830 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     63830 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   7143182 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   7143184 |  109 | `	sxi32 rc = SXRET_OK;` |
|   7143184 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3678116 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1839057 |  112 | `	}` |
|   7143184 |  113 | `	pSet->pBase = 0;` |
|   7143184 |  114 | `	pSet->nUsed = 0;` |
|   7143184 |  115 | `	pSet->nCursor = 0;` |
|   7143184 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3508134 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3508136 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3508046 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3508046 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1754069 |  126 |  |
|   3067268 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3067270 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2128448 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    938824 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    938824 |  135 | `	pSet->nUsed--;` |
|    938824 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    938824 |  137 | `	return pData;` |
|   1533636 |  138 |  |
|   9324352 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   9324354 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   9324354 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   9324354 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4662407 |  148 |  |
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
|     90264 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     90266 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     90266 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     90266 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     90266 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     90266 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     90266 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     90266 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     90266 |  180 | `	pHash->nEntry = 0;` |
|     90266 |  181 | `	pHash->apBucket = apNew;` |
|     90266 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     90266 |  183 | `	return SXRET_OK;` |
|     45134 |  184 |  |
|     11170 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     11172 |  193 | `	pEntry = pHash->pList;` |
|      6817 |  194 | `	for(;;){` |
|     13636 |  195 | `		if( pHash->nEntry == 0 ){` |
|     11172 |  196 | `			break;` |
|         - |  197 | `		}` |
|      2466 |  198 | `		pNext = pEntry->pNext;` |
|      2466 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      2466 |  200 | `		pEntry = pNext;` |
|      2466 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     11172 |  203 | `	if( pHash->apBucket ){` |
|     11172 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      5585 |  205 | `	}` |
|     11172 |  206 | `	pHash->apBucket = 0;` |
|     11172 |  207 | `	pHash->nBucketSize = 0;` |
|     11172 |  208 | `	pHash->pAllocator = 0;` |
|     11172 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   9069436 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   9069438 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   9069438 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   7790690 |  218 | `	for(;;){` |
|  15656285 |  219 | `		if( pEntry == 0 ){` |
|   4938314 |  220 | `			break;` |
|         - |  221 | `		}` |
|  12783405 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   4131128 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   4131126 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   6586849 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   4938314 |  229 | `	return 0;` |
|   4534984 |  230 |  |
|   9120306 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   9120308 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     50878 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   9069432 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   9069432 |  244 | `	if( pEntry == 0 ){` |
|   4938314 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   4131120 |  247 | `	return (SyHashEntry *)pEntry;` |
|   4560419 |  248 |  |
|     70596 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     70598 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     53006 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     26504 |  254 | `	}else{` |
|     17594 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     70598 |  257 | `	if( pEntry->pNextCollide ){` |
|      4123 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2061 |  259 | `	}` |
|     70598 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     70598 |  261 | `	pHash->nEntry--;` |
|     70598 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     70598 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     70598 |  268 | `	return rc;` |
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
|     70590 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     70592 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     70592 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     70592 |  296 | `	return rc;` |
|         2 |  297 |  |
|    129532 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    129534 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    129534 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    899762 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    899764 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    129100 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    129100 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    770666 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    770666 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    770666 |  324 | `	return (SyHashEntry *)pEntry;` |
|    449883 |  325 |  |
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
|      1607 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1597 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1597 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1597 |  344 | `		pEntry = pEntry->pNext;` |
|       799 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     13240 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     13242 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     13242 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     13242 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     13242 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1819482 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1806242 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1806242 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1806242 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1806242 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    867341 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    433664 |  371 | `		}` |
|   1806242 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1806242 |  374 | `		pEntry = pEntry->pNext;` |
|    903122 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     13242 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     13242 |  378 | `	pHash->apBucket = apNew;` |
|     13242 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     13242 |  380 | `	return SXRET_OK;` |
|      6622 |  381 |  |
|   1629974 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1629976 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1629976 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1629976 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1097077 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    548548 |  389 | `	}` |
|   1629976 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1629976 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1629976 |  393 | `	if( pHash->nEntry == 0 ){` |
|     64872 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     32435 |  395 | `	}` |
|   1629976 |  396 | `	pHash->nEntry++;` |
|   1629976 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1629974 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1629976 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     13242 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     13242 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      6620 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1629976 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1629976 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1629976 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1629976 |  421 | `	pEntry->pHash = pHash;` |
|   1629976 |  422 | `	pEntry->pKey = pKey;` |
|   1629976 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1629976 |  424 | `	pEntry->pUserData = pUserData;` |
|   1629976 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1629976 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1629976 |  428 | `	return rc;` |
|    814989 |  429 |  |
|     87344 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     87346 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
