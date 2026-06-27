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
|  19211496 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 |  |
|  19211501 |   16 | `	pSet->nSize = 0 ;` |
|  19211501 |   17 | `	pSet->nUsed = 0;` |
|  19211501 |   18 | `	pSet->nCursor = 0;` |
|  19211501 |   19 | `	pSet->eSize = ElemSize;` |
|  19211501 |   20 | `	pSet->pAllocator = pAllocator;` |
|  19211501 |   21 | `	pSet->pBase =  0;` |
|  19211501 |   22 | `	pSet->pUserData = 0;` |
|  19211501 |   23 | `	return SXRET_OK;` |
|         5 |   24 |  |
|  31596707 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  31596712 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4562711 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4562711 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4405037 |   34 | `			pSet->nSize = 4;` |
|   2202516 |   35 | `		}` |
|   4562711 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4562711 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4562711 |   40 | `		pSet->pBase = pNew;` |
|   4562711 |   41 | `		pSet->nSize <<= 1;` |
|   2281353 |   42 | `	}` |
|  31596712 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 236250016 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  31596712 |   45 | `	pSet->nUsed++;` |
|  31596712 |   46 | `	return SXRET_OK;` |
|  15798402 |   47 |  |
|   1303302 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 |  |
|   1303307 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1303307 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1303307 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1303307 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1303307 |   60 | `	pSet->nSize = nItem;` |
|   1303307 |   61 | `	return SXRET_OK;` |
|    651656 |   62 |  |
|   1800323 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 |  |
|   1800328 |   65 | `	pSet->nUsed   = 0;` |
|   1800328 |   66 | `	pSet->nCursor = 0;` |
|   1800328 |   67 | `	return SXRET_OK;` |
|         5 |   68 |  |
|     57408 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 |  |
|     57413 |   71 | `	pSet->nCursor = 0;` |
|     57413 |   72 | `	return SXRET_OK;` |
|         5 |   73 |  |
|     61614 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     61619 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     23773 |   79 | `		pSet->nCursor = 0;` |
|     23773 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     37851 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     37851 |   83 | `	if( ppEntry ){` |
|     37851 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     18923 |   85 | `	}` |
|     37851 |   86 | `	pSet->nCursor++;` |
|     37851 |   87 | `	return SXRET_OK;` |
|     30812 |   88 |  |
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
|    221134 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 |  |
|    221139 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       139 |  103 | `		pSet->nUsed = nNewSize;` |
|        67 |  104 | `	}` |
|    221139 |  105 | `	return SXRET_OK;` |
|         5 |  106 |  |
|   9990582 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 |  |
|   9990587 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9990587 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4996355 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2498175 |  112 | `	}` |
|   9990587 |  113 | `	pSet->pBase = 0;` |
|   9990587 |  114 | `	pSet->nUsed = 0;` |
|   9990587 |  115 | `	pSet->nCursor = 0;` |
|   9990587 |  116 | `	return rc;` |
|         5 |  117 |  |
|   5754720 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5754725 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       131 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5754599 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5754599 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2877365 |  126 |  |
|   3585294 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3585299 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2181519 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1403785 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1403785 |  135 | `	pSet->nUsed--;` |
|   1403785 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1403785 |  137 | `	return pData;` |
|   1792652 |  138 |  |
|  13334287 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  13334292 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13334292 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13334292 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6667475 |  148 |  |
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
|    566360 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    566365 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    566365 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    566365 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    566365 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    566365 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    566365 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    566365 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    566365 |  180 | `	pHash->nEntry = 0;` |
|    566365 |  181 | `	pHash->apBucket = apNew;` |
|    566365 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    566365 |  183 | `	return SXRET_OK;` |
|    283185 |  184 |  |
|    102816 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    102821 |  193 | `	pEntry = pHash->pList;` |
|     55053 |  194 | `	for(;;){` |
|    110111 |  195 | `		if( pHash->nEntry == 0 ){` |
|    102821 |  196 | `			break;` |
|         - |  197 | `		}` |
|      7295 |  198 | `		pNext = pEntry->pNext;` |
|      7295 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      7295 |  200 | `		pEntry = pNext;` |
|      7295 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    102821 |  203 | `	if( pHash->apBucket ){` |
|    102821 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     51408 |  205 | `	}` |
|    102821 |  206 | `	pHash->apBucket = 0;` |
|    102821 |  207 | `	pHash->nBucketSize = 0;` |
|    102821 |  208 | `	pHash->pAllocator = 0;` |
|    102821 |  209 | `	return SXRET_OK;` |
|         5 |  210 |  |
|  17446656 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  17446661 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  17446661 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  15560757 |  218 | `	for(;;){` |
|  31247957 |  219 | `		if( pEntry == 0 ){` |
|   9259293 |  220 | `			break;` |
|         - |  221 | `		}` |
|  26082094 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   8187372 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   8187373 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  13801301 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   9259293 |  229 | `	return 0;` |
|   8723855 |  230 |  |
|  18283146 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  18283151 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    836697 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  17446459 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  17446459 |  244 | `	if( pEntry == 0 ){` |
|   9259293 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   8187171 |  247 | `	return (SyHashEntry *)pEntry;` |
|   9142100 |  248 |  |
|    123036 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    123041 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     94847 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     47426 |  254 | `	}else{` |
|     28199 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    123041 |  257 | `	if( pEntry->pNextCollide ){` |
|      5039 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2519 |  259 | `	}` |
|    123041 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    123041 |  261 | `	pHash->nEntry--;` |
|    123041 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    123041 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    123041 |  268 | `	return rc;` |
|         5 |  269 |  |
|       202 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       207 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       207 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       207 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       207 |  284 | `	return rc;` |
|       106 |  285 |  |
|    122834 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  287 |  |
|    122839 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    122839 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    122839 |  296 | `	return rc;` |
|         5 |  297 |  |
|   1149356 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|   1149361 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   1149361 |  306 | `	return SXRET_OK;` |
|         5 |  307 |  |
|   7264144 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   7264149 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1148909 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   1148909 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   6115245 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   6115245 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   6115245 |  324 | `	return (SyHashEntry *)pEntry;` |
|   3632077 |  325 |  |
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
|      1995 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1985 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1985 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1985 |  344 | `		pEntry = pEntry->pNext;` |
|       993 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     29496 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  349 |  |
|     29501 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     29501 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     29501 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     29501 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3748829 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3719333 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3719333 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3719333 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3719333 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1784304 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    892131 |  371 | `		}` |
|   3719333 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3719333 |  374 | `		pEntry = pEntry->pNext;` |
|   1859669 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     29501 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     29501 |  378 | `	pHash->apBucket = apNew;` |
|     29501 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     29501 |  380 | `	return SXRET_OK;` |
|     14753 |  381 |  |
|   4946068 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         5 |  383 |  |
|   4946073 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   4946073 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4946073 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2795947 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1397942 |  389 | `	}` |
|   4946073 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   4946073 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   4946073 |  393 | `	if( pHash->nEntry == 0 ){` |
|    310117 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    155056 |  395 | `	}` |
|   4946073 |  396 | `	pHash->nEntry++;` |
|   4946073 |  397 | `	return SXRET_OK;` |
|         5 |  398 |  |
|   4946068 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   4946073 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     29501 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     29501 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     14748 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   4946073 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4946073 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   4946073 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4946073 |  421 | `	pEntry->pHash = pHash;` |
|   4946073 |  422 | `	pEntry->pKey = pKey;` |
|   4946073 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   4946073 |  424 | `	pEntry->pUserData = pUserData;` |
|   4946073 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4946073 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   4946073 |  428 | `	return rc;` |
|   2473039 |  429 |  |
|    159430 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    159435 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  439 |  |
|         - |  440 |  |
