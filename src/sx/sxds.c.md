# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 272/287 lines (94.77%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "sxtypes.h"` |
|        - |    7 | `#include "sxmacros.h"` |
|        - |    8 | `#include "sxset.h"` |
|        - |    9 | `#include "sxmem.h"` |
|        - |   10 | `#include "sxhashtable.h"` |
|        - |   11 | `#include "sxhash.h"` |
|        - |   12 | `#include "sxstr.h"` |
|        - |   13 |  |
|  4643546 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|        2 |   15 |  |
|  4643548 |   16 | `	pSet->nSize = 0 ;` |
|  4643548 |   17 | `	pSet->nUsed = 0;` |
|  4643548 |   18 | `	pSet->nCursor = 0;` |
|  4643548 |   19 | `	pSet->eSize = ElemSize;` |
|  4643548 |   20 | `	pSet->pAllocator = pAllocator;` |
|  4643548 |   21 | `	pSet->pBase =  0;` |
|  4643548 |   22 | `	pSet->pUserData = 0;` |
|  4643548 |   23 | `	return SXRET_OK;` |
|        2 |   24 |  |
|  7599076 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|        2 |   26 |  |
|        - |   27 | `	unsigned char *zbase;` |
|  7599078 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|        - |   29 | `		void *pNew;` |
|   951244 |   30 | `		if( pSet->pAllocator == 0 ){` |
|      ! 0 |   31 | `			return  SXERR_LOCKED;` |
|        - |   32 | `		}` |
|   951244 |   33 | `		if( pSet->nSize <= 0 ){` |
|   904200 |   34 | `			pSet->nSize = 4;` |
|   452099 |   35 | `		}` |
|   951244 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   951244 |   37 | `		if( pNew == 0 ){` |
|      ! 0 |   38 | `			return SXERR_MEM;` |
|        - |   39 | `		}` |
|   951244 |   40 | `		pSet->pBase = pNew;` |
|   951244 |   41 | `		pSet->nSize <<= 1;` |
|   475621 |   42 | `	}` |
|  7599078 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 50153518 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  7599078 |   45 | `	pSet->nUsed++;` |
|  7599078 |   46 | `	return SXRET_OK;` |
|  3799562 |   47 |  |
|   338950 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|        2 |   49 |  |
|   338952 |   50 | `	if( pSet->nSize > 0 ){` |
|      ! 0 |   51 | `		return SXERR_LOCKED;` |
|        - |   52 | `	}` |
|   338952 |   53 | `	if( nItem < 8 ){` |
|      ! 0 |   54 | `		nItem = 8;` |
|      ! 0 |   55 | `	}` |
|   338952 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   338952 |   57 | `	if( pSet->pBase == 0 ){` |
|      ! 0 |   58 | `		return SXERR_MEM;` |
|        - |   59 | `	}` |
|   338952 |   60 | `	pSet->nSize = nItem;` |
|   338952 |   61 | `	return SXRET_OK;` |
|   169477 |   62 |  |
|   708930 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|        2 |   64 |  |
|   708932 |   65 | `	pSet->nUsed   = 0;` |
|   708932 |   66 | `	pSet->nCursor = 0;` |
|   708932 |   67 | `	return SXRET_OK;` |
|        2 |   68 |  |
|    30870 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|        2 |   70 |  |
|    30872 |   71 | `	pSet->nCursor = 0;` |
|    30872 |   72 | `	return SXRET_OK;` |
|        2 |   73 |  |
|    33770 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|        2 |   75 |  |
|        - |   76 | `	register unsigned char *zSrc;` |
|    33772 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|        - |   78 | `		/* Reset cursor */` |
|    12206 |   79 | `		pSet->nCursor = 0;` |
|    12206 |   80 | `		return SXERR_EOF;` |
|        - |   81 | `	}` |
|    21568 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|    21568 |   83 | `	if( ppEntry ){` |
|    21568 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|    10783 |   85 | `	}` |
|    21568 |   86 | `	pSet->nCursor++;` |
|    21568 |   87 | `	return SXRET_OK;` |
|    16887 |   88 |  |
|        - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        8 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|        1 |   91 |  |
|        - |   92 | `	register unsigned char *zSrc;` |
|        9 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|        3 |   94 | `		return 0;` |
|        - |   95 | `	}` |
|        7 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|        7 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|        5 |   98 |  |
|        - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    41938 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|        2 |  101 |  |
|    41940 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       20 |  103 | `		pSet->nUsed = nNewSize;` |
|        9 |  104 | `	}` |
|    41940 |  105 | `	return SXRET_OK;` |
|        2 |  106 |  |
|  2083648 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|        2 |  108 |  |
|  2083650 |  109 | `	sxi32 rc = SXRET_OK;` |
|  2083650 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|  1120680 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   560339 |  112 | `	}` |
|  2083650 |  113 | `	pSet->pBase = 0;` |
|  2083650 |  114 | `	pSet->nUsed = 0;` |
|  2083650 |  115 | `	pSet->nCursor = 0;` |
|  2083650 |  116 | `	return rc;` |
|        2 |  117 |  |
|   985416 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|        2 |  119 |  |
|        - |  120 | `	const char *zBase;` |
|   985418 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       92 |  122 | `		return 0;` |
|        - |  123 | `	}` |
|   985328 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   985328 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   492710 |  126 |  |
|   706330 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|        2 |  128 |  |
|        - |  129 | `	const char *zBase;` |
|        - |  130 | `	void *pData;` |
|   706332 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    76184 |  132 | `		return 0;` |
|        - |  133 | `	}` |
|   630150 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   630150 |  135 | `	pSet->nUsed--;` |
|   630150 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   630150 |  137 | `	return pData;` |
|   353167 |  138 |  |
|  5243947 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|        2 |  140 |  |
|        - |  141 | `	const char *zBase;` |
|  5243949 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|        - |  143 | `		/* Out of range */` |
|      ! 0 |  144 | `		return 0;` |
|        - |  145 | `	}` |
|  5243949 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  5243949 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|  2622153 |  148 |  |
|        - |  149 | `/* Private hash entry */` |
|        - |  150 | `struct SyHashEntry_Pr` |
|        - |  151 |  |
|        - |  152 | `	const void *pKey; /* Hash key */` |
|        - |  153 | `	sxu32 nKeyLen;    /* Key length */` |
|        - |  154 | `	void *pUserData;  /* User private data */` |
|        - |  155 | `	/* Private fields */` |
|        - |  156 | `	sxu32 nHash;` |
|        - |  157 | `	SyHash *pHash;` |
|        - |  158 | `	SyHashEntry_Pr *pNext,*pPrev; /* Next and previous entry in the list */` |
|        - |  159 | `	SyHashEntry_Pr *pNextCollide,*pPrevCollide; /* Collision list */` |
|        - |  160 | `};` |
|        - |  161 | `#define INVALID_HASH(H) ((H)->apBucket == 0)` |
|    56608 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|        2 |  163 |  |
|        - |  164 | `	SyHashEntry_Pr **apNew;` |
|        - |  165 | `#if defined(UNTRUST)` |
|        - |  166 | `	if( pHash == 0 ){` |
|        - |  167 | `		return SXERR_EMPTY;` |
|        - |  168 | `	}` |
|        - |  169 | `#endif` |
|        - |  170 | `	/* Allocate a new table */` |
|    56610 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    56610 |  172 | `	if( apNew == 0 ){` |
|      ! 0 |  173 | `		return SXERR_MEM;` |
|        - |  174 | `	}` |
|    56610 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    56610 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    56610 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    56610 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    56610 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    56610 |  180 | `	pHash->nEntry = 0;` |
|    56610 |  181 | `	pHash->apBucket = apNew;` |
|    56610 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    56610 |  183 | `	return SXRET_OK;` |
|    28306 |  184 |  |
|     8148 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|        2 |  186 |  |
|        - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|        - |  188 | `#if defined(UNTRUST)` |
|        - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|        - |  190 | `		return SXERR_EMPTY;` |
|        - |  191 | `	}` |
|        - |  192 | `#endif` |
|     8150 |  193 | `	pEntry = pHash->pList;` |
|     4333 |  194 | `	for(;;){` |
|     8668 |  195 | `		if( pHash->nEntry == 0 ){` |
|     8150 |  196 | `			break;` |
|        - |  197 | `		}` |
|      520 |  198 | `		pNext = pEntry->pNext;` |
|      520 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      520 |  200 | `		pEntry = pNext;` |
|      520 |  201 | `		pHash->nEntry--;` |
|        2 |  202 | `	}` |
|     8150 |  203 | `	if( pHash->apBucket ){` |
|     8150 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     4074 |  205 | `	}` |
|     8150 |  206 | `	pHash->apBucket = 0;` |
|     8150 |  207 | `	pHash->nBucketSize = 0;` |
|     8150 |  208 | `	pHash->pAllocator = 0;` |
|     8150 |  209 | `	return SXRET_OK;` |
|        2 |  210 |  |
|  6226296 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|        2 |  212 |  |
|        - |  213 | `	SyHashEntry_Pr *pEntry;` |
|        - |  214 | `	sxu32 nHash;` |
|        - |  215 |  |
|  6226298 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  6226298 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  5531599 |  218 | `	for(;;){` |
| 10984657 |  219 | `		if( pEntry == 0 ){` |
|  3351620 |  220 | `			break;` |
|        - |  221 | `		}` |
|  9070248 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|  2874682 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|  2874680 |  224 | `				return pEntry;` |
|        - |  225 | `		}` |
|  4758361 |  226 | `		pEntry = pEntry->pNextCollide;` |
|        2 |  227 | `	}` |
|        - |  228 | `	/* Entry not found */` |
|  3351620 |  229 | `	return 0;` |
|  3113414 |  230 |  |
|  6258312 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|        2 |  232 |  |
|        - |  233 | `	SyHashEntry_Pr *pEntry;` |
|        - |  234 | `#if defined(UNTRUST)` |
|        - |  235 | `	if( INVALID_HASH(pHash) ){` |
|        - |  236 | `		return 0;` |
|        - |  237 | `	}` |
|        - |  238 | `#endif` |
|  6258314 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|        - |  240 | `		/* Don't bother hashing,return immediately */` |
|    32024 |  241 | `		return 0;` |
|        - |  242 | `	}` |
|  6226292 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  6226292 |  244 | `	if( pEntry == 0 ){` |
|  3351620 |  245 | `		return 0;` |
|        - |  246 | `	}` |
|  2874674 |  247 | `	return (SyHashEntry *)pEntry;` |
|  3129422 |  248 |  |
|    54104 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|        2 |  250 |  |
|        - |  251 | `	sxi32 rc;` |
|    54106 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    40168 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|    20085 |  254 | `	}else{` |
|    13940 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|        - |  256 | `	}` |
|    54106 |  257 | `	if( pEntry->pNextCollide ){` |
|     3425 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     1712 |  259 | `	}` |
|    54106 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    54106 |  261 | `	pHash->nEntry--;` |
|    54106 |  262 | `	if( ppUserData ){` |
|        - |  263 | `		/* Write a pointer to the user data */` |
|      ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|      ! 0 |  265 | `	}` |
|        - |  266 | `	/* Release the entry */` |
|    54106 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    54106 |  268 | `	return rc;` |
|        2 |  269 |  |
|        6 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|        1 |  271 |  |
|        - |  272 | `	SyHashEntry_Pr *pEntry;` |
|        - |  273 | `	sxi32 rc;` |
|        - |  274 | `#if defined(UNTRUST)` |
|        - |  275 | `	if( INVALID_HASH(pHash) ){` |
|        - |  276 | `		return SXERR_CORRUPT;` |
|        - |  277 | `	}` |
|        - |  278 | `#endif` |
|        7 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        7 |  280 | `	if( pEntry == 0 ){` |
|      ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|        - |  282 | `	}` |
|        7 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        7 |  284 | `	return rc;` |
|        4 |  285 |  |
|    54098 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|        2 |  287 |  |
|    54100 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|        - |  289 | `	sxi32 rc;` |
|        - |  290 | `#if defined(UNTRUST)` |
|        - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|        - |  292 | `		return SXERR_CORRUPT;` |
|        - |  293 | `	}` |
|        - |  294 | `#endif` |
|    54100 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    54100 |  296 | `	return rc;` |
|        2 |  297 |  |
|    86008 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|        2 |  299 |  |
|        - |  300 | `#if defined(UNTRUST)` |
|        - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|        - |  302 | `		return SXERR_CORRUPT;` |
|        - |  303 | `	}` |
|        - |  304 | `#endif` |
|    86010 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    86010 |  306 | `	return SXRET_OK;` |
|        2 |  307 |  |
|   570636 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|        2 |  309 |  |
|        - |  310 | `	SyHashEntry_Pr *pEntry;` |
|        - |  311 | `#if defined(UNTRUST)` |
|        - |  312 | `	if( INVALID_HASH(pHash) ){` |
|        - |  313 | `		return 0;` |
|        - |  314 | `	}` |
|        - |  315 | `#endif` |
|   570638 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    85576 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    85576 |  318 | `		return 0;` |
|        - |  319 | `	}` |
|   485064 |  320 | `	pEntry = pHash->pCurrent;` |
|        - |  321 | `	/* Advance the cursor */` |
|   485064 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|        - |  323 | `	/* Return the current entry */` |
|   485064 |  324 | `	return (SyHashEntry *)pEntry;` |
|   285320 |  325 |  |
|       10 |  326 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|        1 |  327 |  |
|        - |  328 | `	SyHashEntry_Pr *pEntry;` |
|        - |  329 | `	sxi32 rc;` |
|        - |  330 | `	sxu32 n;` |
|        - |  331 | `#if defined(UNTRUST)` |
|        - |  332 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|        - |  333 | `		return 0;` |
|        - |  334 | `	}` |
|        - |  335 | `#endif` |
|       11 |  336 | `	pEntry = pHash->pList;` |
|     1573 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|        - |  338 | `		/* Invoke the callback */` |
|     1563 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|     1563 |  340 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  341 | `			return rc;` |
|        - |  342 | `		}` |
|        - |  343 | `		/* Point to the next entry */` |
|     1563 |  344 | `		pEntry = pEntry->pNext;` |
|      782 |  345 | `	}` |
|       11 |  346 | `	return SXRET_OK;` |
|        6 |  347 |  |
|     7948 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|        2 |  349 |  |
|     7950 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|        - |  351 | `	SyHashEntry_Pr *pEntry;` |
|        - |  352 | `	SyHashEntry_Pr **apNew;` |
|        - |  353 | `	sxu32 n,iBucket;` |
|        - |  354 |  |
|        - |  355 | `	/* Allocate a new larger table */` |
|     7950 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     7950 |  357 | `	if( apNew == 0 ){` |
|        - |  358 | `		/* Not so fatal,simply a performance hit */` |
|      ! 0 |  359 | `		return SXRET_OK;` |
|        - |  360 | `	}` |
|        - |  361 | `	/* Zero the new table */` |
|     7950 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|        - |  363 | `	/* Rehash all entries */` |
|  1081998 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|  1074050 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - |  366 | `		/* Install in the new bucket */` |
|  1074050 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|  1074050 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|  1074050 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   515831 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|   257917 |  371 | `		}` |
|  1074050 |  372 | `		apNew[iBucket] = pEntry;` |
|        - |  373 | `		/* Point to the next entry */` |
|  1074050 |  374 | `		pEntry = pEntry->pNext;` |
|   537026 |  375 | `	}` |
|        - |  376 | `	/* Release the old table and reflect the change */` |
|     7950 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     7950 |  378 | `	pHash->apBucket = apNew;` |
|     7950 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     7950 |  380 | `	return SXRET_OK;` |
|     3976 |  381 |  |
|   980386 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|        2 |  383 |  |
|   980388 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|        - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   980388 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   980388 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   656956 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   328436 |  389 | `	}` |
|   980388 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|        - |  391 | `	/* Link to the entry list */` |
|   980388 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   980388 |  393 | `	if( pHash->nEntry == 0 ){` |
|    40674 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    20336 |  395 | `	}` |
|   980388 |  396 | `	pHash->nEntry++;` |
|   980388 |  397 | `	return SXRET_OK;` |
|        2 |  398 |  |
|   980386 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|        2 |  400 |  |
|        - |  401 | `	SyHashEntry_Pr *pEntry;` |
|        - |  402 | `	sxi32 rc;` |
|        - |  403 | `#if defined(UNTRUST)` |
|        - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|        - |  405 | `		return SXERR_CORRUPT;` |
|        - |  406 | `	}` |
|        - |  407 | `#endif` |
|   980388 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     7950 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     7950 |  410 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  411 | `			return rc;` |
|        - |  412 | `		}` |
|     3974 |  413 | `	}` |
|        - |  414 | `	/* Allocate a new hash entry */` |
|   980388 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   980388 |  416 | `	if( pEntry == 0 ){` |
|      ! 0 |  417 | `		return SXERR_MEM;` |
|        - |  418 | `	}` |
|        - |  419 | `	/* Zero the entry */` |
|   980388 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   980388 |  421 | `	pEntry->pHash = pHash;` |
|   980388 |  422 | `	pEntry->pKey = pKey;` |
|   980388 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   980388 |  424 | `	pEntry->pUserData = pUserData;` |
|   980388 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|        - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   980388 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   980388 |  428 | `	return rc;` |
|   490195 |  429 |  |
|    64124 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|        2 |  431 |  |
|        - |  432 | `#if defined(UNTRUST)` |
|        - |  433 | `	if( INVALID_HASH(pHash) ){` |
|        - |  434 | `		return 0;` |
|        - |  435 | `	}` |
|        - |  436 | `#endif` |
|        - |  437 | `	/* Last inserted entry */` |
|    64126 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|        2 |  439 |  |
|        - |  440 |  |
