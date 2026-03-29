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
|  12230884 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  12230886 |   16 | `	pSet->nSize = 0 ;` |
|  12230886 |   17 | `	pSet->nUsed = 0;` |
|  12230886 |   18 | `	pSet->nCursor = 0;` |
|  12230886 |   19 | `	pSet->eSize = ElemSize;` |
|  12230886 |   20 | `	pSet->pAllocator = pAllocator;` |
|  12230886 |   21 | `	pSet->pBase =  0;` |
|  12230886 |   22 | `	pSet->pUserData = 0;` |
|  12230886 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  20076000 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  20076002 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3632180 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3632180 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3537512 |   34 | `			pSet->nSize = 4;` |
|   1768755 |   35 | `		}` |
|   3632180 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3632180 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3632180 |   40 | `		pSet->pBase = pNew;` |
|   3632180 |   41 | `		pSet->nSize <<= 1;` |
|   1816089 |   42 | `	}` |
|  20076002 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 149310178 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  20076002 |   45 | `	pSet->nUsed++;` |
|  20076002 |   46 | `	return SXRET_OK;` |
|  10038024 |   47 |  |
|    645422 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    645424 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    645424 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    645424 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    645424 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    645424 |   60 | `	pSet->nSize = nItem;` |
|    645424 |   61 | `	return SXRET_OK;` |
|    322713 |   62 |  |
|   1106854 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1106856 |   65 | `	pSet->nUsed   = 0;` |
|   1106856 |   66 | `	pSet->nCursor = 0;` |
|   1106856 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     40710 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     40712 |   71 | `	pSet->nCursor = 0;` |
|     40712 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     44592 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     44594 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     16614 |   79 | `		pSet->nCursor = 0;` |
|     16614 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     27982 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     27982 |   83 | `	if( ppEntry ){` |
|     27982 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     13990 |   85 | `	}` |
|     27982 |   86 | `	pSet->nCursor++;` |
|     27982 |   87 | `	return SXRET_OK;` |
|     22298 |   88 |  |
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
|     79276 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     79278 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     79278 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   7623344 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   7623346 |  109 | `	sxi32 rc = SXRET_OK;` |
|   7623346 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3948526 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1974262 |  112 | `	}` |
|   7623346 |  113 | `	pSet->pBase = 0;` |
|   7623346 |  114 | `	pSet->nUsed = 0;` |
|   7623346 |  115 | `	pSet->nCursor = 0;` |
|   7623346 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4046850 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4046852 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4046762 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4046762 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2023427 |  126 |  |
|   3166832 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3166834 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2137378 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1029458 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1029458 |  135 | `	pSet->nUsed--;` |
|   1029458 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1029458 |  137 | `	return pData;` |
|   1583418 |  138 |  |
|   9676047 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   9676049 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   9676049 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   9676049 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4838256 |  148 |  |
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
|    136814 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    136816 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    136816 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    136816 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    136816 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    136816 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    136816 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    136816 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    136816 |  180 | `	pHash->nEntry = 0;` |
|    136816 |  181 | `	pHash->apBucket = apNew;` |
|    136816 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    136816 |  183 | `	return SXRET_OK;` |
|     68409 |  184 |  |
|     27016 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     27018 |  193 | `	pEntry = pHash->pList;` |
|     15178 |  194 | `	for(;;){` |
|     30358 |  195 | `		if( pHash->nEntry == 0 ){` |
|     27018 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3342 |  198 | `		pNext = pEntry->pNext;` |
|      3342 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3342 |  200 | `		pEntry = pNext;` |
|      3342 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     27018 |  203 | `	if( pHash->apBucket ){` |
|     27018 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     13508 |  205 | `	}` |
|     27018 |  206 | `	pHash->apBucket = 0;` |
|     27018 |  207 | `	pHash->nBucketSize = 0;` |
|     27018 |  208 | `	pHash->pAllocator = 0;` |
|     27018 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  10012138 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  10012140 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  10012140 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   8785575 |  218 | `	for(;;){` |
|  17610348 |  219 | `		if( pEntry == 0 ){` |
|   5486962 |  220 | `			break;` |
|         - |  221 | `		}` |
|  14385847 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   4525182 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   4525180 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   7598210 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   5486962 |  229 | `	return 0;` |
|   5006335 |  230 |  |
|  10092678 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  10092680 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     80548 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  10012134 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  10012134 |  244 | `	if( pEntry == 0 ){` |
|   5486962 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   4525174 |  247 | `	return (SyHashEntry *)pEntry;` |
|   5046605 |  248 |  |
|     78184 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     78186 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     59164 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     29583 |  254 | `	}else{` |
|     19024 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     78186 |  257 | `	if( pEntry->pNextCollide ){` |
|      4133 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2066 |  259 | `	}` |
|     78186 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     78186 |  261 | `	pHash->nEntry--;` |
|     78186 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     78186 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     78186 |  268 | `	return rc;` |
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
|     78178 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     78180 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     78180 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     78180 |  296 | `	return rc;` |
|         2 |  297 |  |
|    166704 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    166706 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    166706 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   1193296 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   1193298 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    166272 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    166272 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   1027028 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   1027028 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   1027028 |  324 | `	return (SyHashEntry *)pEntry;` |
|    596650 |  325 |  |
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
|     16898 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     16900 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     16900 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     16900 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     16900 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2328484 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2311586 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2311586 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2311586 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2311586 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1109914 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    554966 |  371 | `		}` |
|   2311586 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2311586 |  374 | `		pEntry = pEntry->pNext;` |
|   1155794 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     16900 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     16900 |  378 | `	pHash->apBucket = apNew;` |
|     16900 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     16900 |  380 | `	return SXRET_OK;` |
|      8451 |  381 |  |
|   2110368 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2110370 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2110370 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2110370 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1409608 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    704803 |  389 | `	}` |
|   2110370 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2110370 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2110370 |  393 | `	if( pHash->nEntry == 0 ){` |
|     84294 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     42146 |  395 | `	}` |
|   2110370 |  396 | `	pHash->nEntry++;` |
|   2110370 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2110368 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2110370 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     16900 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     16900 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      8449 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2110370 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2110370 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2110370 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2110370 |  421 | `	pEntry->pHash = pHash;` |
|   2110370 |  422 | `	pEntry->pKey = pKey;` |
|   2110370 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2110370 |  424 | `	pEntry->pUserData = pUserData;` |
|   2110370 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2110370 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2110370 |  428 | `	return rc;` |
|   1055186 |  429 |  |
|     99696 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     99698 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
