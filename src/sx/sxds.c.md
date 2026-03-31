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
|  12748112 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  12748114 |   16 | `	pSet->nSize = 0 ;` |
|  12748114 |   17 | `	pSet->nUsed = 0;` |
|  12748114 |   18 | `	pSet->nCursor = 0;` |
|  12748114 |   19 | `	pSet->eSize = ElemSize;` |
|  12748114 |   20 | `	pSet->pAllocator = pAllocator;` |
|  12748114 |   21 | `	pSet->pBase =  0;` |
|  12748114 |   22 | `	pSet->pUserData = 0;` |
|  12748114 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  21006162 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  21006164 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3690632 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3690632 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3588682 |   34 | `			pSet->nSize = 4;` |
|   1794340 |   35 | `		}` |
|   3690632 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3690632 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3690632 |   40 | `		pSet->pBase = pNew;` |
|   3690632 |   41 | `		pSet->nSize <<= 1;` |
|   1845315 |   42 | `	}` |
|  21006164 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 156111576 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  21006164 |   45 | `	pSet->nUsed++;` |
|  21006164 |   46 | `	return SXRET_OK;` |
|  10503105 |   47 |  |
|    696530 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    696532 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    696532 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    696532 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    696532 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    696532 |   60 | `	pSet->nSize = nItem;` |
|    696532 |   61 | `	return SXRET_OK;` |
|    348267 |   62 |  |
|   1161682 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1161684 |   65 | `	pSet->nUsed   = 0;` |
|   1161684 |   66 | `	pSet->nCursor = 0;` |
|   1161684 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     41510 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     41512 |   71 | `	pSet->nCursor = 0;` |
|     41512 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     45392 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     45394 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     17014 |   79 | `		pSet->nCursor = 0;` |
|     17014 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     28382 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     28382 |   83 | `	if( ppEntry ){` |
|     28382 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     14190 |   85 | `	}` |
|     28382 |   86 | `	pSet->nCursor++;` |
|     28382 |   87 | `	return SXRET_OK;` |
|     22698 |   88 |  |
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
|     86076 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     86078 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     86078 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   7797702 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   7797704 |  109 | `	sxi32 rc = SXRET_OK;` |
|   7797704 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4030914 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2015456 |  112 | `	}` |
|   7797704 |  113 | `	pSet->pBase = 0;` |
|   7797704 |  114 | `	pSet->nUsed = 0;` |
|   7797704 |  115 | `	pSet->nCursor = 0;` |
|   7797704 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4203544 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4203546 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4203456 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4203456 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2101774 |  126 |  |
|   3190488 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3190490 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2140136 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1050356 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1050356 |  135 | `	pSet->nUsed--;` |
|   1050356 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1050356 |  137 | `	return pData;` |
|   1595246 |  138 |  |
|   9847402 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   9847404 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   9847404 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   9847404 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4923930 |  148 |  |
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
|    147648 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    147650 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    147650 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    147650 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    147650 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    147650 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    147650 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    147650 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    147650 |  180 | `	pHash->nEntry = 0;` |
|    147650 |  181 | `	pHash->apBucket = apNew;` |
|    147650 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    147650 |  183 | `	return SXRET_OK;` |
|     73826 |  184 |  |
|     28196 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     28198 |  193 | `	pEntry = pHash->pList;` |
|     15793 |  194 | `	for(;;){` |
|     31588 |  195 | `		if( pHash->nEntry == 0 ){` |
|     28198 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3392 |  198 | `		pNext = pEntry->pNext;` |
|      3392 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3392 |  200 | `		pEntry = pNext;` |
|      3392 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     28198 |  203 | `	if( pHash->apBucket ){` |
|     28198 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     14098 |  205 | `	}` |
|     28198 |  206 | `	pHash->apBucket = 0;` |
|     28198 |  207 | `	pHash->nBucketSize = 0;` |
|     28198 |  208 | `	pHash->pAllocator = 0;` |
|     28198 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  10454548 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  10454550 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  10454550 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   9094096 |  218 | `	for(;;){` |
|  18261608 |  219 | `		if( pEntry == 0 ){` |
|   5745118 |  220 | `			break;` |
|         - |  221 | `		}` |
|  14871078 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   4709436 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   4709434 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   7807060 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   5745118 |  229 | `	return 0;` |
|   5227540 |  230 |  |
|  10541566 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  10541568 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     87028 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  10454542 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  10454542 |  244 | `	if( pEntry == 0 ){` |
|   5745118 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   4709426 |  247 | `	return (SyHashEntry *)pEntry;` |
|   5271049 |  248 |  |
|     80604 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     80606 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     61160 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     30581 |  254 | `	}else{` |
|     19448 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     80606 |  257 | `	if( pEntry->pNextCollide ){` |
|      4133 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2066 |  259 | `	}` |
|     80606 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     80606 |  261 | `	pHash->nEntry--;` |
|     80606 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     80606 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     80606 |  268 | `	return rc;` |
|         2 |  269 |  |
|         8 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|        10 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        10 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|        10 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        10 |  284 | `	return rc;` |
|         6 |  285 |  |
|     80596 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     80598 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     80598 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     80598 |  296 | `	return rc;` |
|         2 |  297 |  |
|    200404 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    200406 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    200406 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   1479452 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   1479454 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    199972 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    199972 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   1279484 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   1279484 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   1279484 |  324 | `	return (SyHashEntry *)pEntry;` |
|    739728 |  325 |  |
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
|      1619 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1609 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1609 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1609 |  344 | `		pEntry = pEntry->pNext;` |
|       805 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     18284 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     18286 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     18286 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     18286 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     18286 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2518798 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2500514 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2500514 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2500514 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2500514 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1200643 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    600323 |  371 | `		}` |
|   2500514 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2500514 |  374 | `		pEntry = pEntry->pNext;` |
|   1250258 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     18286 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     18286 |  378 | `	pHash->apBucket = apNew;` |
|     18286 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     18286 |  380 | `	return SXRET_OK;` |
|      9144 |  381 |  |
|   2280998 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2281000 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2281000 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2281000 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1524084 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    762039 |  389 | `	}` |
|   2281000 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2281000 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2281000 |  393 | `	if( pHash->nEntry == 0 ){` |
|     91118 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     45558 |  395 | `	}` |
|   2281000 |  396 | `	pHash->nEntry++;` |
|   2281000 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2280998 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2281000 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     18286 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     18286 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      9142 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2281000 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2281000 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2281000 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2281000 |  421 | `	pEntry->pHash = pHash;` |
|   2281000 |  422 | `	pEntry->pKey = pKey;` |
|   2281000 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2281000 |  424 | `	pEntry->pUserData = pUserData;` |
|   2281000 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2281000 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2281000 |  428 | `	return rc;` |
|   1140501 |  429 |  |
|    103906 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    103908 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
