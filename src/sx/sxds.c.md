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
|  18784290 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 |  |
|  18784295 |   16 | `	pSet->nSize = 0 ;` |
|  18784295 |   17 | `	pSet->nUsed = 0;` |
|  18784295 |   18 | `	pSet->nCursor = 0;` |
|  18784295 |   19 | `	pSet->eSize = ElemSize;` |
|  18784295 |   20 | `	pSet->pAllocator = pAllocator;` |
|  18784295 |   21 | `	pSet->pBase =  0;` |
|  18784295 |   22 | `	pSet->pUserData = 0;` |
|  18784295 |   23 | `	return SXRET_OK;` |
|         5 |   24 |  |
|  30830487 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  30830492 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4510781 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4510781 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4358155 |   34 | `			pSet->nSize = 4;` |
|   2179075 |   35 | `		}` |
|   4510781 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4510781 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4510781 |   40 | `		pSet->pBase = pNew;` |
|   4510781 |   41 | `		pSet->nSize <<= 1;` |
|   2255388 |   42 | `	}` |
|  30830492 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 230482316 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  30830492 |   45 | `	pSet->nUsed++;` |
|  30830492 |   46 | `	return SXRET_OK;` |
|  15415291 |   47 |  |
|   1260102 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 |  |
|   1260107 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1260107 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1260107 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1260107 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1260107 |   60 | `	pSet->nSize = nItem;` |
|   1260107 |   61 | `	return SXRET_OK;` |
|    630056 |   62 |  |
|   1758883 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 |  |
|   1758888 |   65 | `	pSet->nUsed   = 0;` |
|   1758888 |   66 | `	pSet->nCursor = 0;` |
|   1758888 |   67 | `	return SXRET_OK;` |
|         5 |   68 |  |
|     56960 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 |  |
|     56965 |   71 | `	pSet->nCursor = 0;` |
|     56965 |   72 | `	return SXRET_OK;` |
|         5 |   73 |  |
|     61166 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     61171 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     23549 |   79 | `		pSet->nCursor = 0;` |
|     23549 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     37627 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     37627 |   83 | `	if( ppEntry ){` |
|     37627 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     18811 |   85 | `	}` |
|     37627 |   86 | `	pSet->nCursor++;` |
|     37627 |   87 | `	return SXRET_OK;` |
|     30588 |   88 |  |
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
|    213352 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 |  |
|    213357 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       118 |  103 | `		pSet->nUsed = nNewSize;` |
|        57 |  104 | `	}` |
|    213357 |  105 | `	return SXRET_OK;` |
|         5 |  106 |  |
|   9854806 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 |  |
|   9854811 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9854811 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4931929 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2465962 |  112 | `	}` |
|   9854811 |  113 | `	pSet->pBase = 0;` |
|   9854811 |  114 | `	pSet->nUsed = 0;` |
|   9854811 |  115 | `	pSet->nCursor = 0;` |
|   9854811 |  116 | `	return rc;` |
|         5 |  117 |  |
|   5631240 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5631245 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       131 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5631119 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5631119 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2815625 |  126 |  |
|   3565452 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3565457 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2178003 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1387459 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1387459 |  135 | `	pSet->nUsed--;` |
|   1387459 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1387459 |  137 | `	return pData;` |
|   1782731 |  138 |  |
|  13210859 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  13210864 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13210864 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13210864 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6605762 |  148 |  |
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
|    547698 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    547703 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    547703 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    547703 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    547703 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    547703 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    547703 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    547703 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    547703 |  180 | `	pHash->nEntry = 0;` |
|    547703 |  181 | `	pHash->apBucket = apNew;` |
|    547703 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    547703 |  183 | `	return SXRET_OK;` |
|    273854 |  184 |  |
|    100452 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    100457 |  193 | `	pEntry = pHash->pList;` |
|     53669 |  194 | `	for(;;){` |
|    107343 |  195 | `		if( pHash->nEntry == 0 ){` |
|    100457 |  196 | `			break;` |
|         - |  197 | `		}` |
|      6891 |  198 | `		pNext = pEntry->pNext;` |
|      6891 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      6891 |  200 | `		pEntry = pNext;` |
|      6891 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    100457 |  203 | `	if( pHash->apBucket ){` |
|    100457 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     50226 |  205 | `	}` |
|    100457 |  206 | `	pHash->apBucket = 0;` |
|    100457 |  207 | `	pHash->nBucketSize = 0;` |
|    100457 |  208 | `	pHash->pAllocator = 0;` |
|    100457 |  209 | `	return SXRET_OK;` |
|         5 |  210 |  |
|  17065478 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  17065483 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  17065483 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  15182549 |  218 | `	for(;;){` |
|  30442214 |  219 | `		if( pEntry == 0 ){` |
|   9046983 |  220 | `			break;` |
|         - |  221 | `		}` |
|  25404233 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   8018504 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   8018505 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  13376736 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   9046983 |  229 | `	return 0;` |
|   8533254 |  230 |  |
|  17872948 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  17872953 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    807673 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  17065285 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  17065285 |  244 | `	if( pEntry == 0 ){` |
|   9046983 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   8018307 |  247 | `	return (SyHashEntry *)pEntry;` |
|   8936989 |  248 |  |
|    120414 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    120419 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     92601 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     46303 |  254 | `	}else{` |
|     27823 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    120419 |  257 | `	if( pEntry->pNextCollide ){` |
|      5039 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2519 |  259 | `	}` |
|    120419 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    120419 |  261 | `	pHash->nEntry--;` |
|    120419 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    120419 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    120419 |  268 | `	return rc;` |
|         5 |  269 |  |
|       198 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       203 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       203 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       203 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       203 |  284 | `	return rc;` |
|       104 |  285 |  |
|    120216 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  287 |  |
|    120221 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    120221 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    120221 |  296 | `	return rc;` |
|         5 |  297 |  |
|   1113830 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|   1113835 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   1113835 |  306 | `	return SXRET_OK;` |
|         5 |  307 |  |
|   7048434 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   7048439 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1113385 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   1113385 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   5935059 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   5935059 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   5935059 |  324 | `	return (SyHashEntry *)pEntry;` |
|   3524222 |  325 |  |
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
|      1991 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1981 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1981 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1981 |  344 | `		pEntry = pEntry->pNext;` |
|       991 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     28424 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  349 |  |
|     28429 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     28429 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     28429 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     28429 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3612013 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3583589 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3583589 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3583589 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3583589 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1719180 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    859539 |  371 | `		}` |
|   3583589 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3583589 |  374 | `		pEntry = pEntry->pNext;` |
|   1791797 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     28429 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     28429 |  378 | `	pHash->apBucket = apNew;` |
|     28429 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     28429 |  380 | `	return SXRET_OK;` |
|     14217 |  381 |  |
|   4765334 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         5 |  383 |  |
|   4765339 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   4765339 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4765339 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2690017 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1345091 |  389 | `	}` |
|   4765339 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   4765339 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   4765339 |  393 | `	if( pHash->nEntry == 0 ){` |
|    299423 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    149709 |  395 | `	}` |
|   4765339 |  396 | `	pHash->nEntry++;` |
|   4765339 |  397 | `	return SXRET_OK;` |
|         5 |  398 |  |
|   4765334 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   4765339 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     28429 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     28429 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     14212 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   4765339 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4765339 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   4765339 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4765339 |  421 | `	pEntry->pHash = pHash;` |
|   4765339 |  422 | `	pEntry->pKey = pKey;` |
|   4765339 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   4765339 |  424 | `	pEntry->pUserData = pUserData;` |
|   4765339 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4765339 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   4765339 |  428 | `	return rc;` |
|   2382672 |  429 |  |
|    155416 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    155421 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  439 |  |
|         - |  440 |  |
