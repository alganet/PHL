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
|  4705592 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|        2 |   15 |  |
|  4705594 |   16 | `	pSet->nSize = 0 ;` |
|  4705594 |   17 | `	pSet->nUsed = 0;` |
|  4705594 |   18 | `	pSet->nCursor = 0;` |
|  4705594 |   19 | `	pSet->eSize = ElemSize;` |
|  4705594 |   20 | `	pSet->pAllocator = pAllocator;` |
|  4705594 |   21 | `	pSet->pBase =  0;` |
|  4705594 |   22 | `	pSet->pUserData = 0;` |
|  4705594 |   23 | `	return SXRET_OK;` |
|        2 |   24 |  |
|  7707646 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|        2 |   26 |  |
|        - |   27 | `	unsigned char *zbase;` |
|  7707648 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|        - |   29 | `		void *pNew;` |
|   958784 |   30 | `		if( pSet->pAllocator == 0 ){` |
|      ! 0 |   31 | `			return  SXERR_LOCKED;` |
|        - |   32 | `		}` |
|   958784 |   33 | `		if( pSet->nSize <= 0 ){` |
|   910930 |   34 | `			pSet->nSize = 4;` |
|   455464 |   35 | `		}` |
|   958784 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   958784 |   37 | `		if( pNew == 0 ){` |
|      ! 0 |   38 | `			return SXERR_MEM;` |
|        - |   39 | `		}` |
|   958784 |   40 | `		pSet->pBase = pNew;` |
|   958784 |   41 | `		pSet->nSize <<= 1;` |
|   479391 |   42 | `	}` |
|  7707648 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 50948992 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  7707648 |   45 | `	pSet->nUsed++;` |
|  7707648 |   46 | `	return SXRET_OK;` |
|  3853847 |   47 |  |
|   344680 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|        2 |   49 |  |
|   344682 |   50 | `	if( pSet->nSize > 0 ){` |
|      ! 0 |   51 | `		return SXERR_LOCKED;` |
|        - |   52 | `	}` |
|   344682 |   53 | `	if( nItem < 8 ){` |
|      ! 0 |   54 | `		nItem = 8;` |
|      ! 0 |   55 | `	}` |
|   344682 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   344682 |   57 | `	if( pSet->pBase == 0 ){` |
|      ! 0 |   58 | `		return SXERR_MEM;` |
|        - |   59 | `	}` |
|   344682 |   60 | `	pSet->nSize = nItem;` |
|   344682 |   61 | `	return SXRET_OK;` |
|   172342 |   62 |  |
|   717962 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|        2 |   64 |  |
|   717964 |   65 | `	pSet->nUsed   = 0;` |
|   717964 |   66 | `	pSet->nCursor = 0;` |
|   717964 |   67 | `	return SXRET_OK;` |
|        2 |   68 |  |
|    30968 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|        2 |   70 |  |
|    30970 |   71 | `	pSet->nCursor = 0;` |
|    30970 |   72 | `	return SXRET_OK;` |
|        2 |   73 |  |
|    33870 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|        2 |   75 |  |
|        - |   76 | `	register unsigned char *zSrc;` |
|    33872 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|        - |   78 | `		/* Reset cursor */` |
|    12262 |   79 | `		pSet->nCursor = 0;` |
|    12262 |   80 | `		return SXERR_EOF;` |
|        - |   81 | `	}` |
|    21612 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|    21612 |   83 | `	if( ppEntry ){` |
|    21612 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|    10805 |   85 | `	}` |
|    21612 |   86 | `	pSet->nCursor++;` |
|    21612 |   87 | `	return SXRET_OK;` |
|    16937 |   88 |  |
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
|    42754 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|        2 |  101 |  |
|    42756 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       20 |  103 | `		pSet->nUsed = nNewSize;` |
|        9 |  104 | `	}` |
|    42756 |  105 | `	return SXRET_OK;` |
|        2 |  106 |  |
|  2101744 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|        2 |  108 |  |
|  2101746 |  109 | `	sxi32 rc = SXRET_OK;` |
|  2101746 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|  1130628 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   565313 |  112 | `	}` |
|  2101746 |  113 | `	pSet->pBase = 0;` |
|  2101746 |  114 | `	pSet->nUsed = 0;` |
|  2101746 |  115 | `	pSet->nCursor = 0;` |
|  2101746 |  116 | `	return rc;` |
|        2 |  117 |  |
|  1003044 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|        2 |  119 |  |
|        - |  120 | `	const char *zBase;` |
|  1003046 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       92 |  122 | `		return 0;` |
|        - |  123 | `	}` |
|  1002956 |  124 | `	zBase = (const char *)pSet->pBase;` |
|  1002956 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   501524 |  126 |  |
|   709840 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|        2 |  128 |  |
|        - |  129 | `	const char *zBase;` |
|        - |  130 | `	void *pData;` |
|   709842 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    76692 |  132 | `		return 0;` |
|        - |  133 | `	}` |
|   633152 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   633152 |  135 | `	pSet->nUsed--;` |
|   633152 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   633152 |  137 | `	return pData;` |
|   354922 |  138 |  |
|  5267945 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|        2 |  140 |  |
|        - |  141 | `	const char *zBase;` |
|  5267947 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|        - |  143 | `		/* Out of range */` |
|      ! 0 |  144 | `		return 0;` |
|        - |  145 | `	}` |
|  5267947 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  5267947 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|  2634161 |  148 |  |
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
|    57618 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|        2 |  163 |  |
|        - |  164 | `	SyHashEntry_Pr **apNew;` |
|        - |  165 | `#if defined(UNTRUST)` |
|        - |  166 | `	if( pHash == 0 ){` |
|        - |  167 | `		return SXERR_EMPTY;` |
|        - |  168 | `	}` |
|        - |  169 | `#endif` |
|        - |  170 | `	/* Allocate a new table */` |
|    57620 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    57620 |  172 | `	if( apNew == 0 ){` |
|      ! 0 |  173 | `		return SXERR_MEM;` |
|        - |  174 | `	}` |
|    57620 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    57620 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    57620 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    57620 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    57620 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    57620 |  180 | `	pHash->nEntry = 0;` |
|    57620 |  181 | `	pHash->apBucket = apNew;` |
|    57620 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    57620 |  183 | `	return SXRET_OK;` |
|    28811 |  184 |  |
|     8222 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|        2 |  186 |  |
|        - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|        - |  188 | `#if defined(UNTRUST)` |
|        - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|        - |  190 | `		return SXERR_EMPTY;` |
|        - |  191 | `	}` |
|        - |  192 | `#endif` |
|     8224 |  193 | `	pEntry = pHash->pList;` |
|     4394 |  194 | `	for(;;){` |
|     8790 |  195 | `		if( pHash->nEntry == 0 ){` |
|     8224 |  196 | `			break;` |
|        - |  197 | `		}` |
|      568 |  198 | `		pNext = pEntry->pNext;` |
|      568 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      568 |  200 | `		pEntry = pNext;` |
|      568 |  201 | `		pHash->nEntry--;` |
|        2 |  202 | `	}` |
|     8224 |  203 | `	if( pHash->apBucket ){` |
|     8224 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     4111 |  205 | `	}` |
|     8224 |  206 | `	pHash->apBucket = 0;` |
|     8224 |  207 | `	pHash->nBucketSize = 0;` |
|     8224 |  208 | `	pHash->pAllocator = 0;` |
|     8224 |  209 | `	return SXRET_OK;` |
|        2 |  210 |  |
|  6291602 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|        2 |  212 |  |
|        - |  213 | `	SyHashEntry_Pr *pEntry;` |
|        - |  214 | `	sxu32 nHash;` |
|        - |  215 |  |
|  6291604 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  6291604 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  5548769 |  218 | `	for(;;){` |
| 11171780 |  219 | `		if( pEntry == 0 ){` |
|  3390222 |  220 | `			break;` |
|        - |  221 | `		}` |
|  9232121 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|  2901386 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|  2901384 |  224 | `				return pEntry;` |
|        - |  225 | `		}` |
|  4880178 |  226 | `		pEntry = pEntry->pNextCollide;` |
|        2 |  227 | `	}` |
|        - |  228 | `	/* Entry not found */` |
|  3390222 |  229 | `	return 0;` |
|  3146067 |  230 |  |
|  6324152 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|        2 |  232 |  |
|        - |  233 | `	SyHashEntry_Pr *pEntry;` |
|        - |  234 | `#if defined(UNTRUST)` |
|        - |  235 | `	if( INVALID_HASH(pHash) ){` |
|        - |  236 | `		return 0;` |
|        - |  237 | `	}` |
|        - |  238 | `#endif` |
|  6324154 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|        - |  240 | `		/* Don't bother hashing,return immediately */` |
|    32558 |  241 | `		return 0;` |
|        - |  242 | `	}` |
|  6291598 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  6291598 |  244 | `	if( pEntry == 0 ){` |
|  3390222 |  245 | `		return 0;` |
|        - |  246 | `	}` |
|  2901378 |  247 | `	return (SyHashEntry *)pEntry;` |
|  3162342 |  248 |  |
|    54450 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|        2 |  250 |  |
|        - |  251 | `	sxi32 rc;` |
|    54452 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    40452 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|    20227 |  254 | `	}else{` |
|    14002 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|        - |  256 | `	}` |
|    54452 |  257 | `	if( pEntry->pNextCollide ){` |
|     3419 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     1709 |  259 | `	}` |
|    54452 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    54452 |  261 | `	pHash->nEntry--;` |
|    54452 |  262 | `	if( ppUserData ){` |
|        - |  263 | `		/* Write a pointer to the user data */` |
|      ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|      ! 0 |  265 | `	}` |
|        - |  266 | `	/* Release the entry */` |
|    54452 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    54452 |  268 | `	return rc;` |
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
|    54444 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|        2 |  287 |  |
|    54446 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|        - |  289 | `	sxi32 rc;` |
|        - |  290 | `#if defined(UNTRUST)` |
|        - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|        - |  292 | `		return SXERR_CORRUPT;` |
|        - |  293 | `	}` |
|        - |  294 | `#endif` |
|    54446 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    54446 |  296 | `	return rc;` |
|        2 |  297 |  |
|    87032 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|        2 |  299 |  |
|        - |  300 | `#if defined(UNTRUST)` |
|        - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|        - |  302 | `		return SXERR_CORRUPT;` |
|        - |  303 | `	}` |
|        - |  304 | `#endif` |
|    87034 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    87034 |  306 | `	return SXRET_OK;` |
|        2 |  307 |  |
|   588960 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|        2 |  309 |  |
|        - |  310 | `	SyHashEntry_Pr *pEntry;` |
|        - |  311 | `#if defined(UNTRUST)` |
|        - |  312 | `	if( INVALID_HASH(pHash) ){` |
|        - |  313 | `		return 0;` |
|        - |  314 | `	}` |
|        - |  315 | `#endif` |
|   588962 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    86600 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    86600 |  318 | `		return 0;` |
|        - |  319 | `	}` |
|   502364 |  320 | `	pEntry = pHash->pCurrent;` |
|        - |  321 | `	/* Advance the cursor */` |
|   502364 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|        - |  323 | `	/* Return the current entry */` |
|   502364 |  324 | `	return (SyHashEntry *)pEntry;` |
|   294482 |  325 |  |
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
|     8156 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|        2 |  349 |  |
|     8158 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|        - |  351 | `	SyHashEntry_Pr *pEntry;` |
|        - |  352 | `	SyHashEntry_Pr **apNew;` |
|        - |  353 | `	sxu32 n,iBucket;` |
|        - |  354 |  |
|        - |  355 | `	/* Allocate a new larger table */` |
|     8158 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     8158 |  357 | `	if( apNew == 0 ){` |
|        - |  358 | `		/* Not so fatal,simply a performance hit */` |
|      ! 0 |  359 | `		return SXRET_OK;` |
|        - |  360 | `	}` |
|        - |  361 | `	/* Zero the new table */` |
|     8158 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|        - |  363 | `	/* Rehash all entries */` |
|  1111006 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|  1102850 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - |  366 | `		/* Install in the new bucket */` |
|  1102850 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|  1102850 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|  1102850 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   529636 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|   264820 |  371 | `		}` |
|  1102850 |  372 | `		apNew[iBucket] = pEntry;` |
|        - |  373 | `		/* Point to the next entry */` |
|  1102850 |  374 | `		pEntry = pEntry->pNext;` |
|   551426 |  375 | `	}` |
|        - |  376 | `	/* Release the old table and reflect the change */` |
|     8158 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     8158 |  378 | `	pHash->apBucket = apNew;` |
|     8158 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     8158 |  380 | `	return SXRET_OK;` |
|     4080 |  381 |  |
|  1003624 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|        2 |  383 |  |
|  1003626 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|        - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|  1003626 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|  1003626 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   673461 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   336744 |  389 | `	}` |
|  1003626 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|        - |  391 | `	/* Link to the entry list */` |
|  1003626 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|  1003626 |  393 | `	if( pHash->nEntry == 0 ){` |
|    41402 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    20700 |  395 | `	}` |
|  1003626 |  396 | `	pHash->nEntry++;` |
|  1003626 |  397 | `	return SXRET_OK;` |
|        2 |  398 |  |
|  1003624 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|        2 |  400 |  |
|        - |  401 | `	SyHashEntry_Pr *pEntry;` |
|        - |  402 | `	sxi32 rc;` |
|        - |  403 | `#if defined(UNTRUST)` |
|        - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|        - |  405 | `		return SXERR_CORRUPT;` |
|        - |  406 | `	}` |
|        - |  407 | `#endif` |
|  1003626 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     8158 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     8158 |  410 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  411 | `			return rc;` |
|        - |  412 | `		}` |
|     4078 |  413 | `	}` |
|        - |  414 | `	/* Allocate a new hash entry */` |
|  1003626 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|  1003626 |  416 | `	if( pEntry == 0 ){` |
|      ! 0 |  417 | `		return SXERR_MEM;` |
|        - |  418 | `	}` |
|        - |  419 | `	/* Zero the entry */` |
|  1003626 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|  1003626 |  421 | `	pEntry->pHash = pHash;` |
|  1003626 |  422 | `	pEntry->pKey = pKey;` |
|  1003626 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|  1003626 |  424 | `	pEntry->pUserData = pUserData;` |
|  1003626 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|        - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|  1003626 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|  1003626 |  428 | `	return rc;` |
|   501814 |  429 |  |
|    64734 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|        2 |  431 |  |
|        - |  432 | `#if defined(UNTRUST)` |
|        - |  433 | `	if( INVALID_HASH(pHash) ){` |
|        - |  434 | `		return 0;` |
|        - |  435 | `	}` |
|        - |  436 | `#endif` |
|        - |  437 | `	/* Last inserted entry */` |
|    64736 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|        2 |  439 |  |
|        - |  440 |  |
