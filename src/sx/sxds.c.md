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
|  4843328 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|        2 |   15 |  |
|  4843330 |   16 | `	pSet->nSize = 0 ;` |
|  4843330 |   17 | `	pSet->nUsed = 0;` |
|  4843330 |   18 | `	pSet->nCursor = 0;` |
|  4843330 |   19 | `	pSet->eSize = ElemSize;` |
|  4843330 |   20 | `	pSet->pAllocator = pAllocator;` |
|  4843330 |   21 | `	pSet->pBase =  0;` |
|  4843330 |   22 | `	pSet->pUserData = 0;` |
|  4843330 |   23 | `	return SXRET_OK;` |
|        2 |   24 |  |
|  7951008 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|        2 |   26 |  |
|        - |   27 | `	unsigned char *zbase;` |
|  7951010 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|        - |   29 | `		void *pNew;` |
|   979722 |   30 | `		if( pSet->pAllocator == 0 ){` |
|      ! 0 |   31 | `			return  SXERR_LOCKED;` |
|        - |   32 | `		}` |
|   979722 |   33 | `		if( pSet->nSize <= 0 ){` |
|   930060 |   34 | `			pSet->nSize = 4;` |
|   465029 |   35 | `		}` |
|   979722 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   979722 |   37 | `		if( pNew == 0 ){` |
|      ! 0 |   38 | `			return SXERR_MEM;` |
|        - |   39 | `		}` |
|   979722 |   40 | `		pSet->pBase = pNew;` |
|   979722 |   41 | `		pSet->nSize <<= 1;` |
|   489860 |   42 | `	}` |
|  7951010 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 52679642 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  7951010 |   45 | `	pSet->nUsed++;` |
|  7951010 |   46 | `	return SXRET_OK;` |
|  3975528 |   47 |  |
|   357296 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|        2 |   49 |  |
|   357298 |   50 | `	if( pSet->nSize > 0 ){` |
|      ! 0 |   51 | `		return SXERR_LOCKED;` |
|        - |   52 | `	}` |
|   357298 |   53 | `	if( nItem < 8 ){` |
|      ! 0 |   54 | `		nItem = 8;` |
|      ! 0 |   55 | `	}` |
|   357298 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   357298 |   57 | `	if( pSet->pBase == 0 ){` |
|      ! 0 |   58 | `		return SXERR_MEM;` |
|        - |   59 | `	}` |
|   357298 |   60 | `	pSet->nSize = nItem;` |
|   357298 |   61 | `	return SXRET_OK;` |
|   178650 |   62 |  |
|   734732 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|        2 |   64 |  |
|   734734 |   65 | `	pSet->nUsed   = 0;` |
|   734734 |   66 | `	pSet->nCursor = 0;` |
|   734734 |   67 | `	return SXRET_OK;` |
|        2 |   68 |  |
|    31308 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|        2 |   70 |  |
|    31310 |   71 | `	pSet->nCursor = 0;` |
|    31310 |   72 | `	return SXRET_OK;` |
|        2 |   73 |  |
|    34230 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|        2 |   75 |  |
|        - |   76 | `	register unsigned char *zSrc;` |
|    34232 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|        - |   78 | `		/* Reset cursor */` |
|    12422 |   79 | `		pSet->nCursor = 0;` |
|    12422 |   80 | `		return SXERR_EOF;` |
|        - |   81 | `	}` |
|    21812 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|    21812 |   83 | `	if( ppEntry ){` |
|    21812 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|    10905 |   85 | `	}` |
|    21812 |   86 | `	pSet->nCursor++;` |
|    21812 |   87 | `	return SXRET_OK;` |
|    17117 |   88 |  |
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
|    44526 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|        2 |  101 |  |
|    44528 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       20 |  103 | `		pSet->nUsed = nNewSize;` |
|        9 |  104 | `	}` |
|    44528 |  105 | `	return SXRET_OK;` |
|        2 |  106 |  |
|  2151726 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|        2 |  108 |  |
|  2151728 |  109 | `	sxi32 rc = SXRET_OK;` |
|  2151728 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|  1157022 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   578510 |  112 | `	}` |
|  2151728 |  113 | `	pSet->pBase = 0;` |
|  2151728 |  114 | `	pSet->nUsed = 0;` |
|  2151728 |  115 | `	pSet->nCursor = 0;` |
|  2151728 |  116 | `	return rc;` |
|        2 |  117 |  |
|  1039298 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|        2 |  119 |  |
|        - |  120 | `	const char *zBase;` |
|  1039300 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       92 |  122 | `		return 0;` |
|        - |  123 | `	}` |
|  1039210 |  124 | `	zBase = (const char *)pSet->pBase;` |
|  1039210 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   519651 |  126 |  |
|   721824 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|        2 |  128 |  |
|        - |  129 | `	const char *zBase;` |
|        - |  130 | `	void *pData;` |
|   721826 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    77788 |  132 | `		return 0;` |
|        - |  133 | `	}` |
|   644040 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   644040 |  135 | `	pSet->nUsed--;` |
|   644040 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   644040 |  137 | `	return pData;` |
|   360914 |  138 |  |
|  5369813 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|        2 |  140 |  |
|        - |  141 | `	const char *zBase;` |
|  5369815 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|        - |  143 | `		/* Out of range */` |
|      ! 0 |  144 | `		return 0;` |
|        - |  145 | `	}` |
|  5369815 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  5369815 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|  2685079 |  148 |  |
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
|    59846 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|        2 |  163 |  |
|        - |  164 | `	SyHashEntry_Pr **apNew;` |
|        - |  165 | `#if defined(UNTRUST)` |
|        - |  166 | `	if( pHash == 0 ){` |
|        - |  167 | `		return SXERR_EMPTY;` |
|        - |  168 | `	}` |
|        - |  169 | `#endif` |
|        - |  170 | `	/* Allocate a new table */` |
|    59848 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    59848 |  172 | `	if( apNew == 0 ){` |
|      ! 0 |  173 | `		return SXERR_MEM;` |
|        - |  174 | `	}` |
|    59848 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    59848 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    59848 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    59848 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    59848 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    59848 |  180 | `	pHash->nEntry = 0;` |
|    59848 |  181 | `	pHash->apBucket = apNew;` |
|    59848 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    59848 |  183 | `	return SXRET_OK;` |
|    29925 |  184 |  |
|     8422 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|        2 |  186 |  |
|        - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|        - |  188 | `#if defined(UNTRUST)` |
|        - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|        - |  190 | `		return SXERR_EMPTY;` |
|        - |  191 | `	}` |
|        - |  192 | `#endif` |
|     8424 |  193 | `	pEntry = pHash->pList;` |
|     4566 |  194 | `	for(;;){` |
|     9134 |  195 | `		if( pHash->nEntry == 0 ){` |
|     8424 |  196 | `			break;` |
|        - |  197 | `		}` |
|      712 |  198 | `		pNext = pEntry->pNext;` |
|      712 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      712 |  200 | `		pEntry = pNext;` |
|      712 |  201 | `		pHash->nEntry--;` |
|        2 |  202 | `	}` |
|     8424 |  203 | `	if( pHash->apBucket ){` |
|     8424 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     4211 |  205 | `	}` |
|     8424 |  206 | `	pHash->apBucket = 0;` |
|     8424 |  207 | `	pHash->nBucketSize = 0;` |
|     8424 |  208 | `	pHash->pAllocator = 0;` |
|     8424 |  209 | `	return SXRET_OK;` |
|        2 |  210 |  |
|  6473434 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|        2 |  212 |  |
|        - |  213 | `	SyHashEntry_Pr *pEntry;` |
|        - |  214 | `	sxu32 nHash;` |
|        - |  215 |  |
|  6473436 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  6473436 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  5700004 |  218 | `	for(;;){` |
| 11418266 |  219 | `		if( pEntry == 0 ){` |
|  3494024 |  220 | `			break;` |
|        - |  221 | `		}` |
|  9413820 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|  2979416 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|  2979414 |  224 | `				return pEntry;` |
|        - |  225 | `		}` |
|  4944832 |  226 | `		pEntry = pEntry->pNextCollide;` |
|        2 |  227 | `	}` |
|        - |  228 | `	/* Entry not found */` |
|  3494024 |  229 | `	return 0;` |
|  3236983 |  230 |  |
|  6507148 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|        2 |  232 |  |
|        - |  233 | `	SyHashEntry_Pr *pEntry;` |
|        - |  234 | `#if defined(UNTRUST)` |
|        - |  235 | `	if( INVALID_HASH(pHash) ){` |
|        - |  236 | `		return 0;` |
|        - |  237 | `	}` |
|        - |  238 | `#endif` |
|  6507150 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|        - |  240 | `		/* Don't bother hashing,return immediately */` |
|    33722 |  241 | `		return 0;` |
|        - |  242 | `	}` |
|  6473430 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  6473430 |  244 | `	if( pEntry == 0 ){` |
|  3494024 |  245 | `		return 0;` |
|        - |  246 | `	}` |
|  2979408 |  247 | `	return (SyHashEntry *)pEntry;` |
|  3253840 |  248 |  |
|    55456 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|        2 |  250 |  |
|        - |  251 | `	sxi32 rc;` |
|    55458 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    41268 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|    20635 |  254 | `	}else{` |
|    14192 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|        - |  256 | `	}` |
|    55458 |  257 | `	if( pEntry->pNextCollide ){` |
|     3427 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     1713 |  259 | `	}` |
|    55458 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    55458 |  261 | `	pHash->nEntry--;` |
|    55458 |  262 | `	if( ppUserData ){` |
|        - |  263 | `		/* Write a pointer to the user data */` |
|      ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|      ! 0 |  265 | `	}` |
|        - |  266 | `	/* Release the entry */` |
|    55458 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    55458 |  268 | `	return rc;` |
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
|    55450 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|        2 |  287 |  |
|    55452 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|        - |  289 | `	sxi32 rc;` |
|        - |  290 | `#if defined(UNTRUST)` |
|        - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|        - |  292 | `		return SXERR_CORRUPT;` |
|        - |  293 | `	}` |
|        - |  294 | `#endif` |
|    55452 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    55452 |  296 | `	return rc;` |
|        2 |  297 |  |
|    89448 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|        2 |  299 |  |
|        - |  300 | `#if defined(UNTRUST)` |
|        - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|        - |  302 | `		return SXERR_CORRUPT;` |
|        - |  303 | `	}` |
|        - |  304 | `#endif` |
|    89450 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    89450 |  306 | `	return SXRET_OK;` |
|        2 |  307 |  |
|   605440 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|        2 |  309 |  |
|        - |  310 | `	SyHashEntry_Pr *pEntry;` |
|        - |  311 | `#if defined(UNTRUST)` |
|        - |  312 | `	if( INVALID_HASH(pHash) ){` |
|        - |  313 | `		return 0;` |
|        - |  314 | `	}` |
|        - |  315 | `#endif` |
|   605442 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    89016 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    89016 |  318 | `		return 0;` |
|        - |  319 | `	}` |
|   516428 |  320 | `	pEntry = pHash->pCurrent;` |
|        - |  321 | `	/* Advance the cursor */` |
|   516428 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|        - |  323 | `	/* Return the current entry */` |
|   516428 |  324 | `	return (SyHashEntry *)pEntry;` |
|   302722 |  325 |  |
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
|     8572 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|        2 |  349 |  |
|     8574 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|        - |  351 | `	SyHashEntry_Pr *pEntry;` |
|        - |  352 | `	SyHashEntry_Pr **apNew;` |
|        - |  353 | `	sxu32 n,iBucket;` |
|        - |  354 |  |
|        - |  355 | `	/* Allocate a new larger table */` |
|     8574 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     8574 |  357 | `	if( apNew == 0 ){` |
|        - |  358 | `		/* Not so fatal,simply a performance hit */` |
|      ! 0 |  359 | `		return SXRET_OK;` |
|        - |  360 | `	}` |
|        - |  361 | `	/* Zero the new table */` |
|     8574 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|        - |  363 | `	/* Rehash all entries */` |
|  1168830 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|  1160258 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - |  366 | `		/* Install in the new bucket */` |
|  1160258 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|  1160258 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|  1160258 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   557217 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|   278605 |  371 | `		}` |
|  1160258 |  372 | `		apNew[iBucket] = pEntry;` |
|        - |  373 | `		/* Point to the next entry */` |
|  1160258 |  374 | `		pEntry = pEntry->pNext;` |
|   580130 |  375 | `	}` |
|        - |  376 | `	/* Release the old table and reflect the change */` |
|     8574 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     8574 |  378 | `	pHash->apBucket = apNew;` |
|     8574 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     8574 |  380 | `	return SXRET_OK;` |
|     4288 |  381 |  |
|  1051278 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|        2 |  383 |  |
|  1051280 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|        - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|  1051280 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|  1051280 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   706748 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   353337 |  389 | `	}` |
|  1051280 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|        - |  391 | `	/* Link to the entry list */` |
|  1051280 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|  1051280 |  393 | `	if( pHash->nEntry == 0 ){` |
|    43006 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    21502 |  395 | `	}` |
|  1051280 |  396 | `	pHash->nEntry++;` |
|  1051280 |  397 | `	return SXRET_OK;` |
|        2 |  398 |  |
|  1051278 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|        2 |  400 |  |
|        - |  401 | `	SyHashEntry_Pr *pEntry;` |
|        - |  402 | `	sxi32 rc;` |
|        - |  403 | `#if defined(UNTRUST)` |
|        - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|        - |  405 | `		return SXERR_CORRUPT;` |
|        - |  406 | `	}` |
|        - |  407 | `#endif` |
|  1051280 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     8574 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     8574 |  410 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  411 | `			return rc;` |
|        - |  412 | `		}` |
|     4286 |  413 | `	}` |
|        - |  414 | `	/* Allocate a new hash entry */` |
|  1051280 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|  1051280 |  416 | `	if( pEntry == 0 ){` |
|      ! 0 |  417 | `		return SXERR_MEM;` |
|        - |  418 | `	}` |
|        - |  419 | `	/* Zero the entry */` |
|  1051280 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|  1051280 |  421 | `	pEntry->pHash = pHash;` |
|  1051280 |  422 | `	pEntry->pKey = pKey;` |
|  1051280 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|  1051280 |  424 | `	pEntry->pUserData = pUserData;` |
|  1051280 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|        - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|  1051280 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|  1051280 |  428 | `	return rc;` |
|   525641 |  429 |  |
|    66260 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|        2 |  431 |  |
|        - |  432 | `#if defined(UNTRUST)` |
|        - |  433 | `	if( INVALID_HASH(pHash) ){` |
|        - |  434 | `		return 0;` |
|        - |  435 | `	}` |
|        - |  436 | `#endif` |
|        - |  437 | `	/* Last inserted entry */` |
|    66262 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|        2 |  439 |  |
|        - |  440 |  |
