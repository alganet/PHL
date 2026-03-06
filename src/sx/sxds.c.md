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
|  10102824 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  10102826 |   16 | `	pSet->nSize = 0 ;` |
|  10102826 |   17 | `	pSet->nUsed = 0;` |
|  10102826 |   18 | `	pSet->nCursor = 0;` |
|  10102826 |   19 | `	pSet->eSize = ElemSize;` |
|  10102826 |   20 | `	pSet->pAllocator = pAllocator;` |
|  10102826 |   21 | `	pSet->pBase =  0;` |
|  10102826 |   22 | `	pSet->pUserData = 0;` |
|  10102826 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  15946006 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  15946008 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3274850 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3274850 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3213114 |   34 | `			pSet->nSize = 4;` |
|   1606556 |   35 | `		}` |
|   3274850 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3274850 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3274850 |   40 | `		pSet->pBase = pNew;` |
|   3274850 |   41 | `		pSet->nSize <<= 1;` |
|   1637424 |   42 | `	}` |
|  15946008 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 120226584 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  15946008 |   45 | `	pSet->nUsed++;` |
|  15946008 |   46 | `	return SXRET_OK;` |
|   7973027 |   47 |  |
|    441988 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    441990 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    441990 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    441990 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    441990 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    441990 |   60 | `	pSet->nSize = nItem;` |
|    441990 |   61 | `	return SXRET_OK;` |
|    220996 |   62 |  |
|    868364 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|    868366 |   65 | `	pSet->nUsed   = 0;` |
|    868366 |   66 | `	pSet->nCursor = 0;` |
|    868366 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     35178 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     35180 |   71 | `	pSet->nCursor = 0;` |
|     35180 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     38560 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     38562 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     14118 |   79 | `		pSet->nCursor = 0;` |
|     14118 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     24446 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     24446 |   83 | `	if( ppEntry ){` |
|     24446 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     12222 |   85 | `	}` |
|     24446 |   86 | `	pSet->nCursor++;` |
|     24446 |   87 | `	return SXRET_OK;` |
|     19282 |   88 |  |
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
|     56028 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     56030 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     56030 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   6794178 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   6794180 |  109 | `	sxi32 rc = SXRET_OK;` |
|   6794180 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3489738 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1744868 |  112 | `	}` |
|   6794180 |  113 | `	pSet->pBase = 0;` |
|   6794180 |  114 | `	pSet->nUsed = 0;` |
|   6794180 |  115 | `	pSet->nCursor = 0;` |
|   6794180 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3315832 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3315834 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3315744 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3315744 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1657918 |  126 |  |
|   2956038 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   2956040 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2122790 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    833252 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    833252 |  135 | `	pSet->nUsed--;` |
|    833252 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    833252 |  137 | `	return pData;` |
|   1478021 |  138 |  |
|   8581808 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   8581810 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   8581810 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   8581810 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4291146 |  148 |  |
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
|     79556 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     79558 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     79558 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     79558 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     79558 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     79558 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     79558 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     79558 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     79558 |  180 | `	pHash->nEntry = 0;` |
|     79558 |  181 | `	pHash->apBucket = apNew;` |
|     79558 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     79558 |  183 | `	return SXRET_OK;` |
|     39780 |  184 |  |
|     10054 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     10056 |  193 | `	pEntry = pHash->pList;` |
|      5897 |  194 | `	for(;;){` |
|     11796 |  195 | `		if( pHash->nEntry == 0 ){` |
|     10056 |  196 | `			break;` |
|         - |  197 | `		}` |
|      1742 |  198 | `		pNext = pEntry->pNext;` |
|      1742 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      1742 |  200 | `		pEntry = pNext;` |
|      1742 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     10056 |  203 | `	if( pHash->apBucket ){` |
|     10056 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      5027 |  205 | `	}` |
|     10056 |  206 | `	pHash->apBucket = 0;` |
|     10056 |  207 | `	pHash->nBucketSize = 0;` |
|     10056 |  208 | `	pHash->pAllocator = 0;` |
|     10056 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   8046036 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   8046038 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   8046038 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   7034125 |  218 | `	for(;;){` |
|  14114504 |  219 | `		if( pEntry == 0 ){` |
|   4360378 |  220 | `			break;` |
|         - |  221 | `		}` |
|  11596828 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   3685664 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   3685662 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   6068468 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   4360378 |  229 | `	return 0;` |
|   4023284 |  230 |  |
|   8090948 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   8090950 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     44920 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   8046032 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   8046032 |  244 | `	if( pEntry == 0 ){` |
|   4360378 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   3685656 |  247 | `	return (SyHashEntry *)pEntry;` |
|   4045740 |  248 |  |
|     64368 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     64370 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     48218 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     24110 |  254 | `	}else{` |
|     16154 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     64370 |  257 | `	if( pEntry->pNextCollide ){` |
|      3811 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      1905 |  259 | `	}` |
|     64370 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     64370 |  261 | `	pHash->nEntry--;` |
|     64370 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     64370 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     64370 |  268 | `	return rc;` |
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
|     64362 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     64364 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     64364 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     64364 |  296 | `	return rc;` |
|         2 |  297 |  |
|    115884 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    115886 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    115886 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    807220 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    807222 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    115452 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    115452 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    691772 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    691772 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    691772 |  324 | `	return (SyHashEntry *)pEntry;` |
|    403612 |  325 |  |
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
|     11276 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     11278 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     11278 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     11278 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     11278 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1544686 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1533410 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1533410 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1533410 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1533410 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    736359 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    368185 |  371 | `		}` |
|   1533410 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1533410 |  374 | `		pEntry = pEntry->pNext;` |
|    766706 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     11278 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     11278 |  378 | `	pHash->apBucket = apNew;` |
|     11278 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     11278 |  380 | `	return SXRET_OK;` |
|      5640 |  381 |  |
|   1391100 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1391102 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1391102 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1391102 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    927637 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    463842 |  389 | `	}` |
|   1391102 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1391102 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1391102 |  393 | `	if( pHash->nEntry == 0 ){` |
|     57042 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     28520 |  395 | `	}` |
|   1391102 |  396 | `	pHash->nEntry++;` |
|   1391102 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1391100 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1391102 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     11278 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     11278 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      5638 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1391102 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1391102 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1391102 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1391102 |  421 | `	pEntry->pHash = pHash;` |
|   1391102 |  422 | `	pEntry->pKey = pKey;` |
|   1391102 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1391102 |  424 | `	pEntry->pUserData = pUserData;` |
|   1391102 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1391102 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1391102 |  428 | `	return rc;` |
|    695552 |  429 |  |
|     78576 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     78578 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
