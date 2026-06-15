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
|  17623022 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  17623024 |   16 | `	pSet->nSize = 0 ;` |
|  17623024 |   17 | `	pSet->nUsed = 0;` |
|  17623024 |   18 | `	pSet->nCursor = 0;` |
|  17623024 |   19 | `	pSet->eSize = ElemSize;` |
|  17623024 |   20 | `	pSet->pAllocator = pAllocator;` |
|  17623024 |   21 | `	pSet->pBase =  0;` |
|  17623024 |   22 | `	pSet->pUserData = 0;` |
|  17623024 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  28916300 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  28916302 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4319368 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4319368 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4178596 |   34 | `			pSet->nSize = 4;` |
|   2089297 |   35 | `		}` |
|   4319368 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4319368 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4319368 |   40 | `		pSet->pBase = pNew;` |
|   4319368 |   41 | `		pSet->nSize <<= 1;` |
|   2159683 |   42 | `	}` |
|  28916302 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 216494320 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  28916302 |   45 | `	pSet->nUsed++;` |
|  28916302 |   46 | `	return SXRET_OK;` |
|  14458174 |   47 |  |
|   1160504 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|   1160506 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1160506 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1160506 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1160506 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1160506 |   60 | `	pSet->nSize = nItem;` |
|   1160506 |   61 | `	return SXRET_OK;` |
|    580254 |   62 |  |
|   1641480 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1641482 |   65 | `	pSet->nUsed   = 0;` |
|   1641482 |   66 | `	pSet->nCursor = 0;` |
|   1641482 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     53942 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     53944 |   71 | `	pSet->nCursor = 0;` |
|     53944 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     58102 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     58104 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     22226 |   79 | `		pSet->nCursor = 0;` |
|     22226 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     35880 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     35880 |   83 | `	if( ppEntry ){` |
|     35880 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     17939 |   85 | `	}` |
|     35880 |   86 | `	pSet->nCursor++;` |
|     35880 |   87 | `	return SXRET_OK;` |
|     29053 |   88 |  |
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
|    196140 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    196142 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       110 |  103 | `		pSet->nUsed = nNewSize;` |
|        54 |  104 | `	}` |
|    196142 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   9445436 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   9445438 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9445438 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4730978 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2365488 |  112 | `	}` |
|   9445438 |  113 | `	pSet->pBase = 0;` |
|   9445438 |  114 | `	pSet->nUsed = 0;` |
|   9445438 |  115 | `	pSet->nCursor = 0;` |
|   9445438 |  116 | `	return rc;` |
|         2 |  117 |  |
|   5328604 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5328606 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       112 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5328496 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5328496 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2664304 |  126 |  |
|   3450650 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3450652 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2151634 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1299020 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1299020 |  135 | `	pSet->nUsed--;` |
|   1299020 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1299020 |  137 | `	return pData;` |
|   1725327 |  138 |  |
|  12585831 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  12585833 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  12585833 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  12585833 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6293054 |  148 |  |
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
|    503880 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    503882 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    503882 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    503882 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    503882 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    503882 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    503882 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    503882 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    503882 |  180 | `	pHash->nEntry = 0;` |
|    503882 |  181 | `	pHash->apBucket = apNew;` |
|    503882 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    503882 |  183 | `	return SXRET_OK;` |
|    251942 |  184 |  |
|     92800 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     92802 |  193 | `	pEntry = pHash->pList;` |
|     49521 |  194 | `	for(;;){` |
|     99044 |  195 | `		if( pHash->nEntry == 0 ){` |
|     92802 |  196 | `			break;` |
|         - |  197 | `		}` |
|      6244 |  198 | `		pNext = pEntry->pNext;` |
|      6244 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      6244 |  200 | `		pEntry = pNext;` |
|      6244 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     92802 |  203 | `	if( pHash->apBucket ){` |
|     92802 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     46400 |  205 | `	}` |
|     92802 |  206 | `	pHash->apBucket = 0;` |
|     92802 |  207 | `	pHash->nBucketSize = 0;` |
|     92802 |  208 | `	pHash->pAllocator = 0;` |
|     92802 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  15831286 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  15831288 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  15831288 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  13982631 |  218 | `	for(;;){` |
|  28070965 |  219 | `		if( pEntry == 0 ){` |
|   8333732 |  220 | `			break;` |
|         - |  221 | `		}` |
|  23485883 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   7497560 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   7497558 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  12239679 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   8333732 |  229 | `	return 0;` |
|   7915909 |  230 |  |
|  16580106 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  16580108 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    748976 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  15831134 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  15831134 |  244 | `	if( pEntry == 0 ){` |
|   8333732 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   7497404 |  247 | `	return (SyHashEntry *)pEntry;` |
|   8290319 |  248 |  |
|    111220 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    111222 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     85104 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     42553 |  254 | `	}else{` |
|     26120 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    111222 |  257 | `	if( pEntry->pNextCollide ){` |
|      4931 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2465 |  259 | `	}` |
|    111222 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    111222 |  261 | `	pHash->nEntry--;` |
|    111222 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    111222 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    111222 |  268 | `	return rc;` |
|         2 |  269 |  |
|       154 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       156 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       156 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       156 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       156 |  284 | `	return rc;` |
|        79 |  285 |  |
|    111066 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|    111068 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    111068 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    111068 |  296 | `	return rc;` |
|         2 |  297 |  |
|   1025278 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|   1025280 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   1025280 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   6386528 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   6386530 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1024844 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   1024844 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   5361688 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   5361688 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   5361688 |  324 | `	return (SyHashEntry *)pEntry;` |
|   3193266 |  325 |  |
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
|     26138 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     26140 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     26140 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     26140 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     26140 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3321916 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3295778 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3295778 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3295778 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3295778 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1569947 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    784964 |  371 | `		}` |
|   3295778 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3295778 |  374 | `		pEntry = pEntry->pNext;` |
|   1647890 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     26140 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     26140 |  378 | `	pHash->apBucket = apNew;` |
|     26140 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     26140 |  380 | `	return SXRET_OK;` |
|     13071 |  381 |  |
|   4264636 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   4264638 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   4264638 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4264638 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2381559 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1190760 |  389 | `	}` |
|   4264638 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   4264638 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   4264638 |  393 | `	if( pHash->nEntry == 0 ){` |
|    272314 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    136156 |  395 | `	}` |
|   4264638 |  396 | `	pHash->nEntry++;` |
|   4264638 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   4264636 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   4264638 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     26140 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     26140 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     13069 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   4264638 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4264638 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   4264638 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4264638 |  421 | `	pEntry->pHash = pHash;` |
|   4264638 |  422 | `	pEntry->pKey = pKey;` |
|   4264638 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   4264638 |  424 | `	pEntry->pUserData = pUserData;` |
|   4264638 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4264638 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   4264638 |  428 | `	return rc;` |
|   2132320 |  429 |  |
|    140548 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    140550 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
