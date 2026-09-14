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
|  173737886 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  173737891 |   16 | `	pSet->nSize = 0 ;` |
|  173737891 |   17 | `	pSet->nUsed = 0;` |
|  173737891 |   18 | `	pSet->nCursor = 0;` |
|  173737891 |   19 | `	pSet->eSize = ElemSize;` |
|  173737891 |   20 | `	pSet->pAllocator = pAllocator;` |
|  173737891 |   21 | `	pSet->pBase =  0;` |
|  173737891 |   22 | `	pSet->pUserData = 0;` |
|  173737891 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  393179288 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  393179293 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   22639220 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   22639220 |   33 | `		if( pSet->nSize <= 0 ){` |
|   19283078 |   34 | `			pSet->nSize = 4;` |
|    9642211 |   35 | `		}` |
|   22639220 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   22639220 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   22639220 |   40 | `		pSet->pBase = pNew;` |
|   22639220 |   41 | `		pSet->nSize <<= 1;` |
|   11320282 |   42 | `	}` |
|  393179293 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3101005731 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  393179293 |   45 | `	pSet->nUsed++;` |
|  393179293 |   46 | `	return SXRET_OK;` |
|  196592109 |   47 | `}` |
|   19558612 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   19558617 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   19558617 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   19558617 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   19558617 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   19558617 |   60 | `	pSet->nSize = nItem;` |
|   19558617 |   61 | `	return SXRET_OK;` |
|    9779311 |   62 | `}` |
|   29127371 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   29127376 |   65 | `	pSet->nUsed   = 0;` |
|   29127376 |   66 | `	pSet->nCursor = 0;` |
|   29127376 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      71490 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      71495 |   71 | `	pSet->nCursor = 0;` |
|      71495 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      71912 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      71917 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30947 |   79 | `		pSet->nCursor = 0;` |
|      30947 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      40975 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      40975 |   83 | `	if( ppEntry ){` |
|      40975 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      20485 |   85 | `	}` |
|      40975 |   86 | `	pSet->nCursor++;` |
|      40975 |   87 | `	return SXRET_OK;` |
|      35961 |   88 | `}` |
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
|    3026984 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3026989 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1203 |  103 | `		pSet->nUsed = nNewSize;` |
|        599 |  104 | `	}` |
|    3026989 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   59740092 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   59740097 |  109 | `	sxi32 rc = SXRET_OK;` |
|   59740097 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   32321762 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   16161553 |  112 | `	}` |
|   59740097 |  113 | `	pSet->pBase = 0;` |
|   59740097 |  114 | `	pSet->nUsed = 0;` |
|   59740097 |  115 | `	pSet->nCursor = 0;` |
|   59740097 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   69914414 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   69914419 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19539 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   69894885 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   69894885 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   34957212 |  126 | `}` |
|    9735203 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9735208 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2236315 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7498898 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7498898 |  135 | `	pSet->nUsed--;` |
|    7498898 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7498898 |  137 | `	return pData;` |
|    4868056 |  138 | `}` |
|   36017110 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   36017115 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   36017063 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   36017063 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   18012704 |  148 | `}` |
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
|    1931984 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1931989 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1931989 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1931989 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1931989 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1931989 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1931989 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1931989 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1931989 |  180 | `	pHash->nEntry = 0;` |
|    1931989 |  181 | `	pHash->apBucket = apNew;` |
|    1931989 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1931989 |  183 | `	return SXRET_OK;` |
|     966072 |  184 | `}` |
|     458324 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     458329 |  193 | `	pEntry = pHash->pList;` |
|     243962 |  194 | `	for(;;){` |
|     487779 |  195 | `		if( pHash->nEntry == 0 ){` |
|     458329 |  196 | `			break;` |
|          - |  197 | `		}` |
|      29455 |  198 | `		pNext = pEntry->pNext;` |
|      29455 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      29455 |  200 | `		pEntry = pNext;` |
|      29455 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     458329 |  203 | `	if( pHash->apBucket ){` |
|     458329 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     229237 |  205 | `	}` |
|     458329 |  206 | `	pHash->apBucket = 0;` |
|     458329 |  207 | `	pHash->nBucketSize = 0;` |
|     458329 |  208 | `	pHash->pAllocator = 0;` |
|     458329 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   73077370 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   73077375 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   73077375 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   66054506 |  218 | `	for(;;){` |
|  131794198 |  219 | `		if( pEntry == 0 ){` |
|   26973761 |  220 | `			break;` |
|          - |  221 | `		}` |
|  127871361 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   46107664 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   46103619 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   58716828 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   26973761 |  229 | `	return 0;` |
|   36545329 |  230 | `}` |
|   80550522 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   80550527 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    7473597 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   73076935 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   73076935 |  244 | `	if( pEntry == 0 ){` |
|   26973743 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   46103197 |  247 | `	return (SyHashEntry *)pEntry;` |
|   40281980 |  248 | `}` |
|     489078 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     489083 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     399925 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     200340 |  254 | `	}else{` |
|      89163 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     489083 |  257 | `	if( pEntry->pNextCollide ){` |
|       4660 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2327 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     489083 |  261 | `	if( pHash->pLast == pEntry ){` |
|     478999 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     239947 |  263 | `	}` |
|     489083 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     489083 |  265 | `	pHash->nEntry--;` |
|     489083 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     489083 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     489083 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        440 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        445 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        445 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        427 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        427 |  288 | `	return rc;` |
|        225 |  289 | `}` |
|     488656 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     488661 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     488661 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     488661 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3123532 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3123537 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3123537 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   24005414 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   24005419 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3123271 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3123271 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   20882153 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   20882153 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   20882153 |  328 | `	return (SyHashEntry *)pEntry;` |
|   12002712 |  329 | `}` |
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
|       4653 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4639 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4639 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4639 |  348 | `		pEntry = pEntry->pNext;` |
|       2320 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|     100332 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100337 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100337 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100337 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100337 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18631601 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18531269 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18531269 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18531269 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18531269 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8911569 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4455370 |  375 | `		}` |
|   18531269 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18531269 |  378 | `		pEntry = pEntry->pNext;` |
|    9265637 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100337 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100337 |  382 | `	pHash->apBucket = apNew;` |
|     100337 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100337 |  384 | `	return SXRET_OK;` |
|      50171 |  385 | `}` |
|   20787632 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   20787637 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   20787637 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   20787637 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   13268100 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    6634273 |  393 | `	}` |
|   20787637 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   20787637 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         63 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         63 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         63 |  401 | `		pHash->pLast = pEntry;` |
|         33 |  402 | `	}else{` |
|   20787577 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   20787637 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1070331 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1070331 |  408 | `		pHash->pLast = pEntry;` |
|     535238 |  409 | `	}` |
|   20787637 |  410 | `	pHash->nEntry++;` |
|   20787637 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   20787632 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   20787637 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100337 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100337 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50166 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   20787637 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   20787637 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   20787637 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   20787637 |  435 | `	pEntry->pHash = pHash;` |
|   20787637 |  436 | `	pEntry->pKey = pKey;` |
|   20787637 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   20787637 |  438 | `	pEntry->pUserData = pUserData;` |
|   20787637 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   20787637 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   20787637 |  442 | `	return rc;` |
|   10394271 |  443 | `}` |
|   20787480 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   20787485 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     529124 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     529129 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
