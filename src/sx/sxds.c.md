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
|  10158968 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  10158970 |   16 | `	pSet->nSize = 0 ;` |
|  10158970 |   17 | `	pSet->nUsed = 0;` |
|  10158970 |   18 | `	pSet->nCursor = 0;` |
|  10158970 |   19 | `	pSet->eSize = ElemSize;` |
|  10158970 |   20 | `	pSet->pAllocator = pAllocator;` |
|  10158970 |   21 | `	pSet->pBase =  0;` |
|  10158970 |   22 | `	pSet->pUserData = 0;` |
|  10158970 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  16029686 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  16029688 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3289074 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3289074 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3226856 |   34 | `			pSet->nSize = 4;` |
|   1613427 |   35 | `		}` |
|   3289074 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3289074 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3289074 |   40 | `		pSet->pBase = pNew;` |
|   3289074 |   41 | `		pSet->nSize <<= 1;` |
|   1644536 |   42 | `	}` |
|  16029688 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 120750892 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  16029688 |   45 | `	pSet->nUsed++;` |
|  16029688 |   46 | `	return SXRET_OK;` |
|   8014867 |   47 |  |
|    445530 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    445532 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    445532 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    445532 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    445532 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    445532 |   60 | `	pSet->nSize = nItem;` |
|    445532 |   61 | `	return SXRET_OK;` |
|    222767 |   62 |  |
|    875324 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|    875326 |   65 | `	pSet->nUsed   = 0;` |
|    875326 |   66 | `	pSet->nCursor = 0;` |
|    875326 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     35484 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     35486 |   71 | `	pSet->nCursor = 0;` |
|     35486 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     38916 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     38918 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     14246 |   79 | `		pSet->nCursor = 0;` |
|     14246 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     24674 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     24674 |   83 | `	if( ppEntry ){` |
|     24674 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     12336 |   85 | `	}` |
|     24674 |   86 | `	pSet->nCursor++;` |
|     24674 |   87 | `	return SXRET_OK;` |
|     19460 |   88 |  |
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
|     56436 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     56438 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     56438 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   6824368 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   6824370 |  109 | `	sxi32 rc = SXRET_OK;` |
|   6824370 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3505752 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1752875 |  112 | `	}` |
|   6824370 |  113 | `	pSet->pBase = 0;` |
|   6824370 |  114 | `	pSet->nUsed = 0;` |
|   6824370 |  115 | `	pSet->nCursor = 0;` |
|   6824370 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3325220 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3325222 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3325132 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3325132 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1662612 |  126 |  |
|   2967848 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   2967850 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2123090 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    844762 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    844762 |  135 | `	pSet->nUsed--;` |
|    844762 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    844762 |  137 | `	return pData;` |
|   1483926 |  138 |  |
|   8647644 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   8647646 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   8647646 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   8647646 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4324073 |  148 |  |
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
|     80132 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     80134 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     80134 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     80134 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     80134 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     80134 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     80134 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     80134 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     80134 |  180 | `	pHash->nEntry = 0;` |
|     80134 |  181 | `	pHash->apBucket = apNew;` |
|     80134 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     80134 |  183 | `	return SXRET_OK;` |
|     40068 |  184 |  |
|     10126 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     10128 |  193 | `	pEntry = pHash->pList;` |
|      5951 |  194 | `	for(;;){` |
|     11904 |  195 | `		if( pHash->nEntry == 0 ){` |
|     10128 |  196 | `			break;` |
|         - |  197 | `		}` |
|      1778 |  198 | `		pNext = pEntry->pNext;` |
|      1778 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      1778 |  200 | `		pEntry = pNext;` |
|      1778 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     10128 |  203 | `	if( pHash->apBucket ){` |
|     10128 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      5063 |  205 | `	}` |
|     10128 |  206 | `	pHash->apBucket = 0;` |
|     10128 |  207 | `	pHash->nBucketSize = 0;` |
|     10128 |  208 | `	pHash->pAllocator = 0;` |
|     10128 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   8111122 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   8111124 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   8111124 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   7156496 |  218 | `	for(;;){` |
|  14177082 |  219 | `		if( pEntry == 0 ){` |
|   4395832 |  220 | `			break;` |
|         - |  221 | `		}` |
|  11638768 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   3715296 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   3715294 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   6065960 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   4395832 |  229 | `	return 0;` |
|   4055827 |  230 |  |
|   8156352 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   8156354 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     45238 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   8111118 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   8111118 |  244 | `	if( pEntry == 0 ){` |
|   4395832 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   3715288 |  247 | `	return (SyHashEntry *)pEntry;` |
|   4078442 |  248 |  |
|     64890 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     64892 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     48588 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     24295 |  254 | `	}else{` |
|     16306 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     64892 |  257 | `	if( pEntry->pNextCollide ){` |
|      3887 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      1943 |  259 | `	}` |
|     64892 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     64892 |  261 | `	pHash->nEntry--;` |
|     64892 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     64892 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     64892 |  268 | `	return rc;` |
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
|     64884 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     64886 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     64886 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     64886 |  296 | `	return rc;` |
|         2 |  297 |  |
|    116696 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    116698 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    116698 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    812414 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    812416 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    116264 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    116264 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    696154 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    696154 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    696154 |  324 | `	return (SyHashEntry *)pEntry;` |
|    406209 |  325 |  |
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
|     11372 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     11374 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     11374 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     11374 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     11374 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1558030 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1546658 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1546658 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1546658 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1546658 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    742717 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    371360 |  371 | `		}` |
|   1546658 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1546658 |  374 | `		pEntry = pEntry->pNext;` |
|    773330 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     11374 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     11374 |  378 | `	pHash->apBucket = apNew;` |
|     11374 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     11374 |  380 | `	return SXRET_OK;` |
|      5688 |  381 |  |
|   1402588 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1402590 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1402590 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1402590 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    935480 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    467764 |  389 | `	}` |
|   1402590 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1402590 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1402590 |  393 | `	if( pHash->nEntry == 0 ){` |
|     57462 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     28730 |  395 | `	}` |
|   1402590 |  396 | `	pHash->nEntry++;` |
|   1402590 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1402588 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1402590 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     11374 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     11374 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      5686 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1402590 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1402590 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1402590 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1402590 |  421 | `	pEntry->pHash = pHash;` |
|   1402590 |  422 | `	pEntry->pKey = pKey;` |
|   1402590 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1402590 |  424 | `	pEntry->pUserData = pUserData;` |
|   1402590 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1402590 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1402590 |  428 | `	return rc;` |
|    701296 |  429 |  |
|     79218 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     79220 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
