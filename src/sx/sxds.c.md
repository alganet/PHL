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
|  4605218 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|        2 |   15 |  |
|  4605220 |   16 | `	pSet->nSize = 0 ;` |
|  4605220 |   17 | `	pSet->nUsed = 0;` |
|  4605220 |   18 | `	pSet->nCursor = 0;` |
|  4605220 |   19 | `	pSet->eSize = ElemSize;` |
|  4605220 |   20 | `	pSet->pAllocator = pAllocator;` |
|  4605220 |   21 | `	pSet->pBase =  0;` |
|  4605220 |   22 | `	pSet->pUserData = 0;` |
|  4605220 |   23 | `	return SXRET_OK;` |
|        2 |   24 |  |
|  7535862 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|        2 |   26 |  |
|        - |   27 | `	unsigned char *zbase;` |
|  7535864 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|        - |   29 | `		void *pNew;` |
|   944094 |   30 | `		if( pSet->pAllocator == 0 ){` |
|      ! 0 |   31 | `			return  SXERR_LOCKED;` |
|        - |   32 | `		}` |
|   944094 |   33 | `		if( pSet->nSize <= 0 ){` |
|   897480 |   34 | `			pSet->nSize = 4;` |
|   448739 |   35 | `		}` |
|   944094 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   944094 |   37 | `		if( pNew == 0 ){` |
|      ! 0 |   38 | `			return SXERR_MEM;` |
|        - |   39 | `		}` |
|   944094 |   40 | `		pSet->pBase = pNew;` |
|   944094 |   41 | `		pSet->nSize <<= 1;` |
|   472046 |   42 | `	}` |
|  7535864 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 49723952 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  7535864 |   45 | `	pSet->nUsed++;` |
|  7535864 |   46 | `	return SXRET_OK;` |
|  3767955 |   47 |  |
|   335968 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|        2 |   49 |  |
|   335970 |   50 | `	if( pSet->nSize > 0 ){` |
|      ! 0 |   51 | `		return SXERR_LOCKED;` |
|        - |   52 | `	}` |
|   335970 |   53 | `	if( nItem < 8 ){` |
|      ! 0 |   54 | `		nItem = 8;` |
|      ! 0 |   55 | `	}` |
|   335970 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   335970 |   57 | `	if( pSet->pBase == 0 ){` |
|      ! 0 |   58 | `		return SXERR_MEM;` |
|        - |   59 | `	}` |
|   335970 |   60 | `	pSet->nSize = nItem;` |
|   335970 |   61 | `	return SXRET_OK;` |
|   167986 |   62 |  |
|   704008 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|        2 |   64 |  |
|   704010 |   65 | `	pSet->nUsed   = 0;` |
|   704010 |   66 | `	pSet->nCursor = 0;` |
|   704010 |   67 | `	return SXRET_OK;` |
|        2 |   68 |  |
|    30688 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|        2 |   70 |  |
|    30690 |   71 | `	pSet->nCursor = 0;` |
|    30690 |   72 | `	return SXRET_OK;` |
|        2 |   73 |  |
|    33570 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|        2 |   75 |  |
|        - |   76 | `	register unsigned char *zSrc;` |
|    33572 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|        - |   78 | `		/* Reset cursor */` |
|    12126 |   79 | `		pSet->nCursor = 0;` |
|    12126 |   80 | `		return SXERR_EOF;` |
|        - |   81 | `	}` |
|    21448 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|    21448 |   83 | `	if( ppEntry ){` |
|    21448 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|    10723 |   85 | `	}` |
|    21448 |   86 | `	pSet->nCursor++;` |
|    21448 |   87 | `	return SXRET_OK;` |
|    16787 |   88 |  |
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
|    41530 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|        2 |  101 |  |
|    41532 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       20 |  103 | `		pSet->nUsed = nNewSize;` |
|        9 |  104 | `	}` |
|    41532 |  105 | `	return SXRET_OK;` |
|        2 |  106 |  |
|  2067568 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|        2 |  108 |  |
|  2067570 |  109 | `	sxi32 rc = SXRET_OK;` |
|  2067570 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|  1112222 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   556110 |  112 | `	}` |
|  2067570 |  113 | `	pSet->pBase = 0;` |
|  2067570 |  114 | `	pSet->nUsed = 0;` |
|  2067570 |  115 | `	pSet->nCursor = 0;` |
|  2067570 |  116 | `	return rc;` |
|        2 |  117 |  |
|   976886 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|        2 |  119 |  |
|        - |  120 | `	const char *zBase;` |
|   976888 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       92 |  122 | `		return 0;` |
|        - |  123 | `	}` |
|   976798 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   976798 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   488445 |  126 |  |
|   701362 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|        2 |  128 |  |
|        - |  129 | `	const char *zBase;` |
|        - |  130 | `	void *pData;` |
|   701364 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    75910 |  132 | `		return 0;` |
|        - |  133 | `	}` |
|   625456 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   625456 |  135 | `	pSet->nUsed--;` |
|   625456 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   625456 |  137 | `	return pData;` |
|   350683 |  138 |  |
|  5207905 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|        2 |  140 |  |
|        - |  141 | `	const char *zBase;` |
|  5207907 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|        - |  143 | `		/* Out of range */` |
|      ! 0 |  144 | `		return 0;` |
|        - |  145 | `	}` |
|  5207907 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  5207907 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|  2604140 |  148 |  |
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
|    56080 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|        2 |  163 |  |
|        - |  164 | `	SyHashEntry_Pr **apNew;` |
|        - |  165 | `#if defined(UNTRUST)` |
|        - |  166 | `	if( pHash == 0 ){` |
|        - |  167 | `		return SXERR_EMPTY;` |
|        - |  168 | `	}` |
|        - |  169 | `#endif` |
|        - |  170 | `	/* Allocate a new table */` |
|    56082 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    56082 |  172 | `	if( apNew == 0 ){` |
|      ! 0 |  173 | `		return SXERR_MEM;` |
|        - |  174 | `	}` |
|    56082 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    56082 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    56082 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    56082 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    56082 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    56082 |  180 | `	pHash->nEntry = 0;` |
|    56082 |  181 | `	pHash->apBucket = apNew;` |
|    56082 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    56082 |  183 | `	return SXRET_OK;` |
|    28042 |  184 |  |
|     8088 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|        2 |  186 |  |
|        - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|        - |  188 | `#if defined(UNTRUST)` |
|        - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|        - |  190 | `		return SXERR_EMPTY;` |
|        - |  191 | `	}` |
|        - |  192 | `#endif` |
|     8090 |  193 | `	pEntry = pHash->pList;` |
|     4285 |  194 | `	for(;;){` |
|     8572 |  195 | `		if( pHash->nEntry == 0 ){` |
|     8090 |  196 | `			break;` |
|        - |  197 | `		}` |
|      484 |  198 | `		pNext = pEntry->pNext;` |
|      484 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      484 |  200 | `		pEntry = pNext;` |
|      484 |  201 | `		pHash->nEntry--;` |
|        2 |  202 | `	}` |
|     8090 |  203 | `	if( pHash->apBucket ){` |
|     8090 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     4044 |  205 | `	}` |
|     8090 |  206 | `	pHash->apBucket = 0;` |
|     8090 |  207 | `	pHash->nBucketSize = 0;` |
|     8090 |  208 | `	pHash->pAllocator = 0;` |
|     8090 |  209 | `	return SXRET_OK;` |
|        2 |  210 |  |
|  6174542 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|        2 |  212 |  |
|        - |  213 | `	SyHashEntry_Pr *pEntry;` |
|        - |  214 | `	sxu32 nHash;` |
|        - |  215 |  |
|  6174544 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  6174544 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  5441790 |  218 | `	for(;;){` |
| 10948908 |  219 | `		if( pEntry == 0 ){` |
|  3321792 |  220 | `			break;` |
|        - |  221 | `		}` |
|  9053364 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|  2852756 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|  2852754 |  224 | `				return pEntry;` |
|        - |  225 | `		}` |
|  4774366 |  226 | `		pEntry = pEntry->pNextCollide;` |
|        2 |  227 | `	}` |
|        - |  228 | `	/* Entry not found */` |
|  3321792 |  229 | `	return 0;` |
|  3087537 |  230 |  |
|  6206276 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|        2 |  232 |  |
|        - |  233 | `	SyHashEntry_Pr *pEntry;` |
|        - |  234 | `#if defined(UNTRUST)` |
|        - |  235 | `	if( INVALID_HASH(pHash) ){` |
|        - |  236 | `		return 0;` |
|        - |  237 | `	}` |
|        - |  238 | `#endif` |
|  6206278 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|        - |  240 | `		/* Don't bother hashing,return immediately */` |
|    31742 |  241 | `		return 0;` |
|        - |  242 | `	}` |
|  6174538 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  6174538 |  244 | `	if( pEntry == 0 ){` |
|  3321792 |  245 | `		return 0;` |
|        - |  246 | `	}` |
|  2852748 |  247 | `	return (SyHashEntry *)pEntry;` |
|  3103404 |  248 |  |
|    53764 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|        2 |  250 |  |
|        - |  251 | `	sxi32 rc;` |
|    53766 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    39912 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|    19957 |  254 | `	}else{` |
|    13856 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|        - |  256 | `	}` |
|    53766 |  257 | `	if( pEntry->pNextCollide ){` |
|     3423 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     1711 |  259 | `	}` |
|    53766 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    53766 |  261 | `	pHash->nEntry--;` |
|    53766 |  262 | `	if( ppUserData ){` |
|        - |  263 | `		/* Write a pointer to the user data */` |
|      ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|      ! 0 |  265 | `	}` |
|        - |  266 | `	/* Release the entry */` |
|    53766 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    53766 |  268 | `	return rc;` |
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
|    53758 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|        2 |  287 |  |
|    53760 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|        - |  289 | `	sxi32 rc;` |
|        - |  290 | `#if defined(UNTRUST)` |
|        - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|        - |  292 | `		return SXERR_CORRUPT;` |
|        - |  293 | `	}` |
|        - |  294 | `#endif` |
|    53760 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    53760 |  296 | `	return rc;` |
|        2 |  297 |  |
|    85372 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|        2 |  299 |  |
|        - |  300 | `#if defined(UNTRUST)` |
|        - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|        - |  302 | `		return SXERR_CORRUPT;` |
|        - |  303 | `	}` |
|        - |  304 | `#endif` |
|    85374 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    85374 |  306 | `	return SXRET_OK;` |
|        2 |  307 |  |
|   566470 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|        2 |  309 |  |
|        - |  310 | `	SyHashEntry_Pr *pEntry;` |
|        - |  311 | `#if defined(UNTRUST)` |
|        - |  312 | `	if( INVALID_HASH(pHash) ){` |
|        - |  313 | `		return 0;` |
|        - |  314 | `	}` |
|        - |  315 | `#endif` |
|   566472 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    84940 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    84940 |  318 | `		return 0;` |
|        - |  319 | `	}` |
|   481534 |  320 | `	pEntry = pHash->pCurrent;` |
|        - |  321 | `	/* Advance the cursor */` |
|   481534 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|        - |  323 | `	/* Return the current entry */` |
|   481534 |  324 | `	return (SyHashEntry *)pEntry;` |
|   283237 |  325 |  |
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
|     1569 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|        - |  338 | `		/* Invoke the callback */` |
|     1559 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|     1559 |  340 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  341 | `			return rc;` |
|        - |  342 | `		}` |
|        - |  343 | `		/* Point to the next entry */` |
|     1559 |  344 | `		pEntry = pEntry->pNext;` |
|      780 |  345 | `	}` |
|       11 |  346 | `	return SXRET_OK;` |
|        6 |  347 |  |
|     7852 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|        2 |  349 |  |
|     7854 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|        - |  351 | `	SyHashEntry_Pr *pEntry;` |
|        - |  352 | `	SyHashEntry_Pr **apNew;` |
|        - |  353 | `	sxu32 n,iBucket;` |
|        - |  354 |  |
|        - |  355 | `	/* Allocate a new larger table */` |
|     7854 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     7854 |  357 | `	if( apNew == 0 ){` |
|        - |  358 | `		/* Not so fatal,simply a performance hit */` |
|      ! 0 |  359 | `		return SXRET_OK;` |
|        - |  360 | `	}` |
|        - |  361 | `	/* Zero the new table */` |
|     7854 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|        - |  363 | `	/* Rehash all entries */` |
|  1068654 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|  1060802 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - |  366 | `		/* Install in the new bucket */` |
|  1060802 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|  1060802 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|  1060802 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   509464 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|   254732 |  371 | `		}` |
|  1060802 |  372 | `		apNew[iBucket] = pEntry;` |
|        - |  373 | `		/* Point to the next entry */` |
|  1060802 |  374 | `		pEntry = pEntry->pNext;` |
|   530402 |  375 | `	}` |
|        - |  376 | `	/* Release the old table and reflect the change */` |
|     7854 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     7854 |  378 | `	pHash->apBucket = apNew;` |
|     7854 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     7854 |  380 | `	return SXRET_OK;` |
|     3928 |  381 |  |
|   967396 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|        2 |  383 |  |
|   967398 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|        - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   967398 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   967398 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   648329 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   324176 |  389 | `	}` |
|   967398 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|        - |  391 | `	/* Link to the entry list */` |
|   967398 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   967398 |  393 | `	if( pHash->nEntry == 0 ){` |
|    40290 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    20144 |  395 | `	}` |
|   967398 |  396 | `	pHash->nEntry++;` |
|   967398 |  397 | `	return SXRET_OK;` |
|        2 |  398 |  |
|   967396 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|        2 |  400 |  |
|        - |  401 | `	SyHashEntry_Pr *pEntry;` |
|        - |  402 | `	sxi32 rc;` |
|        - |  403 | `#if defined(UNTRUST)` |
|        - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|        - |  405 | `		return SXERR_CORRUPT;` |
|        - |  406 | `	}` |
|        - |  407 | `#endif` |
|   967398 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     7854 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     7854 |  410 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  411 | `			return rc;` |
|        - |  412 | `		}` |
|     3926 |  413 | `	}` |
|        - |  414 | `	/* Allocate a new hash entry */` |
|   967398 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   967398 |  416 | `	if( pEntry == 0 ){` |
|      ! 0 |  417 | `		return SXERR_MEM;` |
|        - |  418 | `	}` |
|        - |  419 | `	/* Zero the entry */` |
|   967398 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   967398 |  421 | `	pEntry->pHash = pHash;` |
|   967398 |  422 | `	pEntry->pKey = pKey;` |
|   967398 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   967398 |  424 | `	pEntry->pUserData = pUserData;` |
|   967398 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|        - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   967398 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   967398 |  428 | `	return rc;` |
|   483700 |  429 |  |
|    63664 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|        2 |  431 |  |
|        - |  432 | `#if defined(UNTRUST)` |
|        - |  433 | `	if( INVALID_HASH(pHash) ){` |
|        - |  434 | `		return 0;` |
|        - |  435 | `	}` |
|        - |  436 | `#endif` |
|        - |  437 | `	/* Last inserted entry */` |
|    63666 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|        2 |  439 |  |
|        - |  440 |  |
