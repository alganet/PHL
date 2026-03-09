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
|  10493632 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  10493634 |   16 | `	pSet->nSize = 0 ;` |
|  10493634 |   17 | `	pSet->nUsed = 0;` |
|  10493634 |   18 | `	pSet->nCursor = 0;` |
|  10493634 |   19 | `	pSet->eSize = ElemSize;` |
|  10493634 |   20 | `	pSet->pAllocator = pAllocator;` |
|  10493634 |   21 | `	pSet->pBase =  0;` |
|  10493634 |   22 | `	pSet->pUserData = 0;` |
|  10493634 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  16620298 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  16620300 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3360210 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3360210 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3293046 |   34 | `			pSet->nSize = 4;` |
|   1646522 |   35 | `		}` |
|   3360210 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3360210 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3360210 |   40 | `		pSet->pBase = pNew;` |
|   3360210 |   41 | `		pSet->nSize <<= 1;` |
|   1680104 |   42 | `	}` |
|  16620300 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 124823672 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  16620300 |   45 | `	pSet->nUsed++;` |
|  16620300 |   46 | `	return SXRET_OK;` |
|   8310173 |   47 |  |
|    471480 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    471482 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    471482 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    471482 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    471482 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    471482 |   60 | `	pSet->nSize = nItem;` |
|    471482 |   61 | `	return SXRET_OK;` |
|    235742 |   62 |  |
|    915394 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|    915396 |   65 | `	pSet->nUsed   = 0;` |
|    915396 |   66 | `	pSet->nCursor = 0;` |
|    915396 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     36708 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     36710 |   71 | `	pSet->nCursor = 0;` |
|     36710 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     40308 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     40310 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     14774 |   79 | `		pSet->nCursor = 0;` |
|     14774 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     25538 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     25538 |   83 | `	if( ppEntry ){` |
|     25538 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     12768 |   85 | `	}` |
|     25538 |   86 | `	pSet->nCursor++;` |
|     25538 |   87 | `	return SXRET_OK;` |
|     20156 |   88 |  |
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
|     59432 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     59434 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     59434 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   6974366 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   6974368 |  109 | `	sxi32 rc = SXRET_OK;` |
|   6974368 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3588752 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1794375 |  112 | `	}` |
|   6974368 |  113 | `	pSet->pBase = 0;` |
|   6974368 |  114 | `	pSet->nUsed = 0;` |
|   6974368 |  115 | `	pSet->nCursor = 0;` |
|   6974368 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3414414 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3414416 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3414326 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3414326 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1707209 |  126 |  |
|   3015760 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3015762 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2125152 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    890612 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    890612 |  135 | `	pSet->nUsed--;` |
|    890612 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    890612 |  137 | `	return pData;` |
|   1507882 |  138 |  |
|   8944178 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   8944180 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   8944180 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   8944180 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4472307 |  148 |  |
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
|     84266 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     84268 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     84268 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     84268 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     84268 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     84268 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     84268 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     84268 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     84268 |  180 | `	pHash->nEntry = 0;` |
|     84268 |  181 | `	pHash->apBucket = apNew;` |
|     84268 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     84268 |  183 | `	return SXRET_OK;` |
|     42135 |  184 |  |
|     10564 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     10566 |  193 | `	pEntry = pHash->pList;` |
|      6310 |  194 | `	for(;;){` |
|     12622 |  195 | `		if( pHash->nEntry == 0 ){` |
|     10566 |  196 | `			break;` |
|         - |  197 | `		}` |
|      2058 |  198 | `		pNext = pEntry->pNext;` |
|      2058 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      2058 |  200 | `		pEntry = pNext;` |
|      2058 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     10566 |  203 | `	if( pHash->apBucket ){` |
|     10566 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      5282 |  205 | `	}` |
|     10566 |  206 | `	pHash->apBucket = 0;` |
|     10566 |  207 | `	pHash->nBucketSize = 0;` |
|     10566 |  208 | `	pHash->pAllocator = 0;` |
|     10566 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   8505368 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   8505370 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   8505370 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   7339274 |  218 | `	for(;;){` |
|  14688565 |  219 | `		if( pEntry == 0 ){` |
|   4618820 |  220 | `			break;` |
|         - |  221 | `		}` |
|  12012892 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   3886554 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   3886552 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   6183197 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   4618820 |  229 | `	return 0;` |
|   4252950 |  230 |  |
|   8552844 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   8552846 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     47484 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   8505364 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   8505364 |  244 | `	if( pEntry == 0 ){` |
|   4618820 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   3886546 |  247 | `	return (SyHashEntry *)pEntry;` |
|   4276688 |  248 |  |
|     67416 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     67418 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     50526 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     25264 |  254 | `	}else{` |
|     16894 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     67418 |  257 | `	if( pEntry->pNextCollide ){` |
|      4039 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2019 |  259 | `	}` |
|     67418 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     67418 |  261 | `	pHash->nEntry--;` |
|     67418 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     67418 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     67418 |  268 | `	return rc;` |
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
|     67410 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     67412 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     67412 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     67412 |  296 | `	return rc;` |
|         2 |  297 |  |
|    121856 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    121858 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    121858 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    847302 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    847304 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    121424 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    121424 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    725882 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    725882 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    725882 |  324 | `	return (SyHashEntry *)pEntry;` |
|    423653 |  325 |  |
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
|      1579 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1569 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1569 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1569 |  344 | `		pEntry = pEntry->pNext;` |
|       785 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     12076 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     12078 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     12078 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     12078 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     12078 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1655886 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1643810 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1643810 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1643810 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1643810 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    789374 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    394690 |  371 | `		}` |
|   1643810 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1643810 |  374 | `		pEntry = pEntry->pNext;` |
|    821906 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     12078 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     12078 |  378 | `	pHash->apBucket = apNew;` |
|     12078 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     12078 |  380 | `	return SXRET_OK;` |
|      6040 |  381 |  |
|   1495908 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1495910 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1495910 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1495910 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1002746 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    501318 |  389 | `	}` |
|   1495910 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1495910 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1495910 |  393 | `	if( pHash->nEntry == 0 ){` |
|     60454 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     30226 |  395 | `	}` |
|   1495910 |  396 | `	pHash->nEntry++;` |
|   1495910 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1495908 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1495910 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     12078 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     12078 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      6038 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1495910 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1495910 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1495910 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1495910 |  421 | `	pEntry->pHash = pHash;` |
|   1495910 |  422 | `	pEntry->pKey = pKey;` |
|   1495910 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1495910 |  424 | `	pEntry->pUserData = pUserData;` |
|   1495910 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1495910 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1495910 |  428 | `	return rc;` |
|    747956 |  429 |  |
|     82626 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     82628 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
