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
|  11636452 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  11636454 |   16 | `	pSet->nSize = 0 ;` |
|  11636454 |   17 | `	pSet->nUsed = 0;` |
|  11636454 |   18 | `	pSet->nCursor = 0;` |
|  11636454 |   19 | `	pSet->eSize = ElemSize;` |
|  11636454 |   20 | `	pSet->pAllocator = pAllocator;` |
|  11636454 |   21 | `	pSet->pBase =  0;` |
|  11636454 |   22 | `	pSet->pUserData = 0;` |
|  11636454 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  18860270 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  18860272 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3543734 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3543734 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3458526 |   34 | `			pSet->nSize = 4;` |
|   1729262 |   35 | `		}` |
|   3543734 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3543734 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3543734 |   40 | `		pSet->pBase = pNew;` |
|   3543734 |   41 | `		pSet->nSize <<= 1;` |
|   1771866 |   42 | `	}` |
|  18860272 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 140394024 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  18860272 |   45 | `	pSet->nUsed++;` |
|  18860272 |   46 | `	return SXRET_OK;` |
|   9430159 |   47 |  |
|    580580 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    580582 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    580582 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    580582 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    580582 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    580582 |   60 | `	pSet->nSize = nItem;` |
|    580582 |   61 | `	return SXRET_OK;` |
|    290292 |   62 |  |
|   1058460 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1058462 |   65 | `	pSet->nUsed   = 0;` |
|   1058462 |   66 | `	pSet->nCursor = 0;` |
|   1058462 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     39460 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     39462 |   71 | `	pSet->nCursor = 0;` |
|     39462 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     43310 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     43312 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     16006 |   79 | `		pSet->nCursor = 0;` |
|     16006 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     27308 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     27308 |   83 | `	if( ppEntry ){` |
|     27308 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     13653 |   85 | `	}` |
|     27308 |   86 | `	pSet->nCursor++;` |
|     27308 |   87 | `	return SXRET_OK;` |
|     21657 |   88 |  |
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
|     70714 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     70716 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     70716 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   7402990 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   7402992 |  109 | `	sxi32 rc = SXRET_OK;` |
|   7402992 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3830528 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1915263 |  112 | `	}` |
|   7402992 |  113 | `	pSet->pBase = 0;` |
|   7402992 |  114 | `	pSet->nUsed = 0;` |
|   7402992 |  115 | `	pSet->nCursor = 0;` |
|   7402992 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3756504 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3756506 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3756416 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3756416 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1878254 |  126 |  |
|   3123350 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3123352 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2132774 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    990580 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    990580 |  135 | `	pSet->nUsed--;` |
|    990580 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    990580 |  137 | `	return pData;` |
|   1561677 |  138 |  |
|   9762863 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   9762865 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   9762865 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   9762865 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4881681 |  148 |  |
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
|     99540 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     99542 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     99542 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     99542 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     99542 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     99542 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     99542 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     99542 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     99542 |  180 | `	pHash->nEntry = 0;` |
|     99542 |  181 | `	pHash->apBucket = apNew;` |
|     99542 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     99542 |  183 | `	return SXRET_OK;` |
|     49772 |  184 |  |
|     11948 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     11950 |  193 | `	pEntry = pHash->pList;` |
|      7446 |  194 | `	for(;;){` |
|     14894 |  195 | `		if( pHash->nEntry == 0 ){` |
|     11950 |  196 | `			break;` |
|         - |  197 | `		}` |
|      2946 |  198 | `		pNext = pEntry->pNext;` |
|      2946 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      2946 |  200 | `		pEntry = pNext;` |
|      2946 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     11950 |  203 | `	if( pHash->apBucket ){` |
|     11950 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      5974 |  205 | `	}` |
|     11950 |  206 | `	pHash->apBucket = 0;` |
|     11950 |  207 | `	pHash->nBucketSize = 0;` |
|     11950 |  208 | `	pHash->pAllocator = 0;` |
|     11950 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   9915162 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   9915164 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   9915164 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   8577383 |  218 | `	for(;;){` |
|  17003661 |  219 | `		if( pEntry == 0 ){` |
|   5386440 |  220 | `			break;` |
|         - |  221 | `		}` |
|  13881455 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   4528728 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   4528726 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   7088499 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   5386440 |  229 | `	return 0;` |
|   4957847 |  230 |  |
|   9971056 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   9971058 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     55902 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   9915158 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   9915158 |  244 | `	if( pEntry == 0 ){` |
|   5386440 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   4528720 |  247 | `	return (SyHashEntry *)pEntry;` |
|   4985794 |  248 |  |
|     74534 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     74536 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     56190 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     28096 |  254 | `	}else{` |
|     18348 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     74536 |  257 | `	if( pEntry->pNextCollide ){` |
|      4125 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2062 |  259 | `	}` |
|     74536 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     74536 |  261 | `	pHash->nEntry--;` |
|     74536 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     74536 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     74536 |  268 | `	return rc;` |
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
|     74528 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     74530 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     74530 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     74530 |  296 | `	return rc;` |
|         2 |  297 |  |
|    140298 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    140300 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    140300 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    975736 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    975738 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    139866 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    139866 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    835874 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    835874 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    835874 |  324 | `	return (SyHashEntry *)pEntry;` |
|    487870 |  325 |  |
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
|      1617 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1607 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1607 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1607 |  344 | `		pEntry = pEntry->pNext;` |
|       804 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     14856 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     14858 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     14858 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     14858 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     14858 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2044106 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2029250 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2029250 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2029250 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2029250 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    974381 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    487189 |  371 | `		}` |
|   2029250 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2029250 |  374 | `		pEntry = pEntry->pNext;` |
|   1014626 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     14858 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     14858 |  378 | `	pHash->apBucket = apNew;` |
|     14858 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     14858 |  380 | `	return SXRET_OK;` |
|      7430 |  381 |  |
|   1830040 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1830042 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1830042 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1830042 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1238352 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    619171 |  389 | `	}` |
|   1830042 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1830042 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1830042 |  393 | `	if( pHash->nEntry == 0 ){` |
|     71538 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     35768 |  395 | `	}` |
|   1830042 |  396 | `	pHash->nEntry++;` |
|   1830042 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1830040 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1830042 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     14858 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     14858 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      7428 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1830042 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1830042 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1830042 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1830042 |  421 | `	pEntry->pHash = pHash;` |
|   1830042 |  422 | `	pEntry->pKey = pKey;` |
|   1830042 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1830042 |  424 | `	pEntry->pUserData = pUserData;` |
|   1830042 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1830042 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1830042 |  428 | `	return rc;` |
|    915022 |  429 |  |
|     93366 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     93368 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
