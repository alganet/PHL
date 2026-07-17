# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 289/304 lines (95.07%)

[Root index](../../index.md) | [Directory index](index.md)

|       Hits | Line | Source |
| ---------: | ---: | :--- |
|          - |    1 | `/**` |
|          - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|          - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|          - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|          - |    5 | ` */` |
|          - |    6 | `#include "sxtypes.h"` |
|          - |    7 | `#include "sxmacros.h"` |
|          - |    8 | `#include "sxset.h"` |
|          - |    9 | `#include "sxmem.h"` |
|          - |   10 | `#include "sxhashtable.h"` |
|          - |   11 | `#include "sxhash.h"` |
|          - |   12 | `#include "sxstr.h"` |
|          - |   13 |  |
|   80944428 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|   80944433 |   16 | `	pSet->nSize = 0 ;` |
|   80944433 |   17 | `	pSet->nUsed = 0;` |
|   80944433 |   18 | `	pSet->nCursor = 0;` |
|   80944433 |   19 | `	pSet->eSize = ElemSize;` |
|   80944433 |   20 | `	pSet->pAllocator = pAllocator;` |
|   80944433 |   21 | `	pSet->pBase =  0;` |
|   80944433 |   22 | `	pSet->pUserData = 0;` |
|   80944433 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  174796505 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  174796510 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   11757227 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   11757227 |   33 | `		if( pSet->nSize <= 0 ){` |
|   10386373 |   34 | `			pSet->nSize = 4;` |
|    5193184 |   35 | `		}` |
|   11757227 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   11757227 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   11757227 |   40 | `		pSet->pBase = pNew;` |
|   11757227 |   41 | `		pSet->nSize <<= 1;` |
|    5878611 |   42 | `	}` |
|  174796510 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 1287857190 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  174796510 |   45 | `	pSet->nUsed++;` |
|  174796510 |   46 | `	return SXRET_OK;` |
|   87398300 |   47 | `}` |
|    8659150 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|    8659155 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|    8659155 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|    8659155 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    8659155 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|    8659155 |   60 | `	pSet->nSize = nItem;` |
|    8659155 |   61 | `	return SXRET_OK;` |
|    4329580 |   62 | `}` |
|   13563971 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   13563976 |   65 | `	pSet->nUsed   = 0;` |
|   13563976 |   66 | `	pSet->nCursor = 0;` |
|   13563976 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      68384 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      68389 |   71 | `	pSet->nCursor = 0;` |
|      68389 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      72560 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      72565 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29419 |   79 | `		pSet->nCursor = 0;` |
|      29419 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43151 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43151 |   83 | `	if( ppEntry ){` |
|      43151 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21573 |   85 | `	}` |
|      43151 |   86 | `	pSet->nCursor++;` |
|      43151 |   87 | `	return SXRET_OK;` |
|      36285 |   88 | `}` |
|          - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|          8 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|          1 |   91 | `{` |
|          - |   92 | `	register unsigned char *zSrc;` |
|          9 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          3 |   94 | `		return 0;` |
|          - |   95 | `	}` |
|          7 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|          7 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|          5 |   98 | `}` |
|          - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    1397150 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    1397155 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        683 |  103 | `		pSet->nUsed = nNewSize;` |
|        339 |  104 | `	}` |
|    1397155 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   30660786 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   30660791 |  109 | `	sxi32 rc = SXRET_OK;` |
|   30660791 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   16217503 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|    8108749 |  112 | `	}` |
|   30660791 |  113 | `	pSet->pBase = 0;` |
|   30660791 |  114 | `	pSet->nUsed = 0;` |
|   30660791 |  115 | `	pSet->nCursor = 0;` |
|   30660791 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   30543082 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   30543087 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        133 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   30542959 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   30542959 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   15271546 |  126 | `}` |
|    6259228 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    6259233 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2393513 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    3865725 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    3865725 |  135 | `	pSet->nUsed--;` |
|    3865725 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    3865725 |  137 | `	return pData;` |
|    3129619 |  138 | `}` |
|   21135282 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   21135287 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|        ! 0 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   21135287 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   21135287 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   10567974 |  148 | `}` |
|          - |  149 | `/* Private hash entry */` |
|          - |  150 | `struct SyHashEntry_Pr` |
|          - |  151 | `{` |
|          - |  152 | `	const void *pKey; /* Hash key */` |
|          - |  153 | `	sxu32 nKeyLen;    /* Key length */` |
|          - |  154 | `	void *pUserData;  /* User private data */` |
|          - |  155 | `	/* Private fields */` |
|          - |  156 | `	sxu32 nHash;` |
|          - |  157 | `	SyHash *pHash;` |
|          - |  158 | `	SyHashEntry_Pr *pNext,*pPrev; /* Next and previous entry in the list */` |
|          - |  159 | `	SyHashEntry_Pr *pNextCollide,*pPrevCollide; /* Collision list */` |
|          - |  160 | `};` |
|          - |  161 | `#define INVALID_HASH(H) ((H)->apBucket == 0)` |
|    1150300 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1150305 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1150305 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1150305 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1150305 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1150305 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1150305 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1150305 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1150305 |  180 | `	pHash->nEntry = 0;` |
|    1150305 |  181 | `	pHash->apBucket = apNew;` |
|    1150305 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1150305 |  183 | `	return SXRET_OK;` |
|     575155 |  184 | `}` |
|     305496 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     305501 |  193 | `	pEntry = pHash->pList;` |
|     161137 |  194 | `	for(;;){` |
|     322279 |  195 | `		if( pHash->nEntry == 0 ){` |
|     305501 |  196 | `			break;` |
|          - |  197 | `		}` |
|      16783 |  198 | `		pNext = pEntry->pNext;` |
|      16783 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      16783 |  200 | `		pEntry = pNext;` |
|      16783 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     305501 |  203 | `	if( pHash->apBucket ){` |
|     305501 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     152748 |  205 | `	}` |
|     305501 |  206 | `	pHash->apBucket = 0;` |
|     305501 |  207 | `	pHash->nBucketSize = 0;` |
|     305501 |  208 | `	pHash->pAllocator = 0;` |
|     305501 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   39597278 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   39597283 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   39597283 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   37036504 |  218 | `	for(;;){` |
|   74068816 |  219 | `		if( pEntry == 0 ){` |
|   15856913 |  220 | `			break;` |
|          - |  221 | `		}` |
|   70081857 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   23740408 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   23740375 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   34471538 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   15856913 |  229 | `	return 0;` |
|   19799154 |  230 | `}` |
|   43145712 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   43145717 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    3548721 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   39597001 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   39597001 |  244 | `	if( pEntry == 0 ){` |
|   15856913 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   23740093 |  247 | `	return (SyHashEntry *)pEntry;` |
|   21573371 |  248 | `}` |
|     205280 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     205285 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     163395 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      81700 |  254 | `	}else{` |
|      41895 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     205285 |  257 | `	if( pEntry->pNextCollide ){` |
|       3712 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       1856 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     205285 |  261 | `	if( pHash->pLast == pEntry ){` |
|     198617 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|      99306 |  263 | `	}` |
|     205285 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     205285 |  265 | `	pHash->nEntry--;` |
|     205285 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|        ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        ! 0 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     205285 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     205285 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        282 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        287 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        287 |  284 | `	if( pEntry == 0 ){` |
|        ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        287 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        287 |  288 | `	return rc;` |
|        146 |  289 | `}` |
|     204998 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     205003 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     205003 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     205003 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    1740936 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    1740941 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    1740941 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   13021304 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   13021309 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    1740679 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    1740679 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   11280635 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   11280635 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   11280635 |  328 | `	return (SyHashEntry *)pEntry;` |
|    6510657 |  329 | `}` |
|         10 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|          1 |  331 | `{` |
|          - |  332 | `	SyHashEntry_Pr *pEntry;` |
|          - |  333 | `	sxi32 rc;` |
|          - |  334 | `	sxu32 n;` |
|          - |  335 | `#if defined(UNTRUST)` |
|          - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|          - |  337 | `		return 0;` |
|          - |  338 | `	}` |
|          - |  339 | `#endif` |
|         11 |  340 | `	pEntry = pHash->pList;` |
|       2845 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       2835 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       2835 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       2835 |  348 | `		pEntry = pEntry->pNext;` |
|       1418 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      77364 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      77369 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      77369 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      77369 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      77369 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|    9211577 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|    9134213 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|    9134213 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|    9134213 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|    9134213 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    4395384 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    2197673 |  375 | `		}` |
|    9134213 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|    9134213 |  378 | `		pEntry = pEntry->pNext;` |
|    4567109 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      77369 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      77369 |  382 | `	pHash->apBucket = apNew;` |
|      77369 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      77369 |  384 | `	return SXRET_OK;` |
|      38687 |  385 | `}` |
|   11331194 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   11331199 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   11331199 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   11331199 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    7119066 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    3559310 |  393 | `	}` |
|   11331199 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   11331199 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   11331147 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   11331199 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     597337 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     597337 |  408 | `		pHash->pLast = pEntry;` |
|     298666 |  409 | `	}` |
|   11331199 |  410 | `	pHash->nEntry++;` |
|   11331199 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   11331194 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   11331199 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      77369 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      77369 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      38682 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   11331199 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   11331199 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   11331199 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   11331199 |  435 | `	pEntry->pHash = pHash;` |
|   11331199 |  436 | `	pEntry->pKey = pKey;` |
|   11331199 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   11331199 |  438 | `	pEntry->pUserData = pUserData;` |
|   11331199 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   11331199 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   11331199 |  442 | `	return rc;` |
|    5665602 |  443 | `}` |
|   11331066 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   11331071 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        128 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          2 |  455 | `{` |
|        130 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          2 |  457 | `}` |
|     245598 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     245603 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
