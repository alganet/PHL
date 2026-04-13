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
|  15119508 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  15119510 |   16 | `	pSet->nSize = 0 ;` |
|  15119510 |   17 | `	pSet->nUsed = 0;` |
|  15119510 |   18 | `	pSet->nCursor = 0;` |
|  15119510 |   19 | `	pSet->eSize = ElemSize;` |
|  15119510 |   20 | `	pSet->pAllocator = pAllocator;` |
|  15119510 |   21 | `	pSet->pBase =  0;` |
|  15119510 |   22 | `	pSet->pUserData = 0;` |
|  15119510 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  24559812 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  24559814 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3969314 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3969314 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3854204 |   34 | `			pSet->nSize = 4;` |
|   1927101 |   35 | `		}` |
|   3969314 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3969314 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3969314 |   40 | `		pSet->pBase = pNew;` |
|   3969314 |   41 | `		pSet->nSize <<= 1;` |
|   1984656 |   42 | `	}` |
|  24559814 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 183251366 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  24559814 |   45 | `	pSet->nUsed++;` |
|  24559814 |   46 | `	return SXRET_OK;` |
|  12279930 |   47 |  |
|    906872 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    906874 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    906874 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    906874 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    906874 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    906874 |   60 | `	pSet->nSize = nItem;` |
|    906874 |   61 | `	return SXRET_OK;` |
|    453438 |   62 |  |
|   1385678 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1385680 |   65 | `	pSet->nUsed   = 0;` |
|   1385680 |   66 | `	pSet->nCursor = 0;` |
|   1385680 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     48028 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     48030 |   71 | `	pSet->nCursor = 0;` |
|     48030 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     52110 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     52112 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     19738 |   79 | `		pSet->nCursor = 0;` |
|     19738 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     32376 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     32376 |   83 | `	if( ppEntry ){` |
|     32376 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     16187 |   85 | `	}` |
|     32376 |   86 | `	pSet->nCursor++;` |
|     32376 |   87 | `	return SXRET_OK;` |
|     26057 |   88 |  |
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
|    150826 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    150828 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|    150828 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8642570 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8642572 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8642572 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4393076 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2196537 |  112 | `	}` |
|   8642572 |  113 | `	pSet->pBase = 0;` |
|   8642572 |  114 | `	pSet->nUsed = 0;` |
|   8642572 |  115 | `	pSet->nCursor = 0;` |
|   8642572 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4722602 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4722604 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       108 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4722498 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4722498 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2361303 |  126 |  |
|   3321848 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3321850 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2146400 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1175452 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1175452 |  135 | `	pSet->nUsed--;` |
|   1175452 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1175452 |  137 | `	return pData;` |
|   1660926 |  138 |  |
|  11258036 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  11258038 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  11258038 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  11258038 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5629188 |  148 |  |
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
|    272922 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    272924 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    272924 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    272924 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    272924 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    272924 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    272924 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    272924 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    272924 |  180 | `	pHash->nEntry = 0;` |
|    272924 |  181 | `	pHash->apBucket = apNew;` |
|    272924 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    272924 |  183 | `	return SXRET_OK;` |
|    136463 |  184 |  |
|     81220 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     81222 |  193 | `	pEntry = pHash->pList;` |
|     42735 |  194 | `	for(;;){` |
|     85472 |  195 | `		if( pHash->nEntry == 0 ){` |
|     81222 |  196 | `			break;` |
|         - |  197 | `		}` |
|      4252 |  198 | `		pNext = pEntry->pNext;` |
|      4252 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      4252 |  200 | `		pEntry = pNext;` |
|      4252 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     81222 |  203 | `	if( pHash->apBucket ){` |
|     81222 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     40610 |  205 | `	}` |
|     81222 |  206 | `	pHash->apBucket = 0;` |
|     81222 |  207 | `	pHash->nBucketSize = 0;` |
|     81222 |  208 | `	pHash->pAllocator = 0;` |
|     81222 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  12430594 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  12430596 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  12430596 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  11243209 |  218 | `	for(;;){` |
|  22556311 |  219 | `		if( pEntry == 0 ){` |
|   6859392 |  220 | `			break;` |
|         - |  221 | `		}` |
|  18482393 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5571208 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5571206 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  10125717 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   6859392 |  229 | `	return 0;` |
|   6215563 |  230 |  |
|  12931318 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  12931320 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    500830 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  12430492 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  12430492 |  244 | `	if( pEntry == 0 ){` |
|   6859392 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5571102 |  247 | `	return (SyHashEntry *)pEntry;` |
|   6465925 |  248 |  |
|     93872 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     93874 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     71400 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     35701 |  254 | `	}else{` |
|     22476 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     93874 |  257 | `	if( pEntry->pNextCollide ){` |
|      4723 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2361 |  259 | `	}` |
|     93874 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     93874 |  261 | `	pHash->nEntry--;` |
|     93874 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     93874 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     93874 |  268 | `	return rc;` |
|         2 |  269 |  |
|       104 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       106 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       106 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       106 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       106 |  284 | `	return rc;` |
|        54 |  285 |  |
|     93768 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     93770 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     93770 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     93770 |  296 | `	return rc;` |
|         2 |  297 |  |
|    326324 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    326326 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    326326 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2591056 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2591058 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    325892 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    325892 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2265168 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2265168 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2265168 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1295530 |  325 |  |
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
|     23602 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     23604 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     23604 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     23604 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     23604 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2998164 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2974562 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2974562 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2974562 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2974562 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1421488 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    710818 |  371 | `		}` |
|   2974562 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2974562 |  374 | `		pEntry = pEntry->pNext;` |
|   1487282 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     23604 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     23604 |  378 | `	pHash->apBucket = apNew;` |
|     23604 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     23604 |  380 | `	return SXRET_OK;` |
|     11803 |  381 |  |
|   3047664 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   3047666 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   3047666 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   3047666 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1973819 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    986886 |  389 | `	}` |
|   3047666 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   3047666 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   3047666 |  393 | `	if( pHash->nEntry == 0 ){` |
|    136216 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     68107 |  395 | `	}` |
|   3047666 |  396 | `	pHash->nEntry++;` |
|   3047666 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   3047664 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   3047666 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     23604 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     23604 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     11801 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   3047666 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   3047666 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   3047666 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   3047666 |  421 | `	pEntry->pHash = pHash;` |
|   3047666 |  422 | `	pEntry->pKey = pKey;` |
|   3047666 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   3047666 |  424 | `	pEntry->pUserData = pUserData;` |
|   3047666 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   3047666 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   3047666 |  428 | `	return rc;` |
|   1523834 |  429 |  |
|    120260 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    120262 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
