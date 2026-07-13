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
|   80672574 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|   80672579 |   16 | `	pSet->nSize = 0 ;` |
|   80672579 |   17 | `	pSet->nUsed = 0;` |
|   80672579 |   18 | `	pSet->nCursor = 0;` |
|   80672579 |   19 | `	pSet->eSize = ElemSize;` |
|   80672579 |   20 | `	pSet->pAllocator = pAllocator;` |
|   80672579 |   21 | `	pSet->pBase =  0;` |
|   80672579 |   22 | `	pSet->pUserData = 0;` |
|   80672579 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  174191507 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  174191512 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   11725119 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   11725119 |   33 | `		if( pSet->nSize <= 0 ){` |
|   10359201 |   34 | `			pSet->nSize = 4;` |
|    5179598 |   35 | `		}` |
|   11725119 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   11725119 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   11725119 |   40 | `		pSet->pBase = pNew;` |
|   11725119 |   41 | `		pSet->nSize <<= 1;` |
|    5862557 |   42 | `	}` |
|  174191512 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 1283406132 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  174191512 |   45 | `	pSet->nUsed++;` |
|  174191512 |   46 | `	return SXRET_OK;` |
|   87095801 |   47 | `}` |
|    8627508 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|    8627513 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|    8627513 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|    8627513 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    8627513 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|    8627513 |   60 | `	pSet->nSize = nItem;` |
|    8627513 |   61 | `	return SXRET_OK;` |
|    4313759 |   62 | `}` |
|   13515955 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   13515960 |   65 | `	pSet->nUsed   = 0;` |
|   13515960 |   66 | `	pSet->nCursor = 0;` |
|   13515960 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      68258 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      68263 |   71 | `	pSet->nCursor = 0;` |
|      68263 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      72424 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      72429 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29361 |   79 | `		pSet->nCursor = 0;` |
|      29361 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43073 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43073 |   83 | `	if( ppEntry ){` |
|      43073 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21534 |   85 | `	}` |
|      43073 |   86 | `	pSet->nCursor++;` |
|      43073 |   87 | `	return SXRET_OK;` |
|      36217 |   88 | `}` |
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
|    1392094 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    1392099 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        683 |  103 | `		pSet->nUsed = nNewSize;` |
|        339 |  104 | `	}` |
|    1392099 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   30570020 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   30570025 |  109 | `	sxi32 rc = SXRET_OK;` |
|   30570025 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   16169037 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|    8084516 |  112 | `	}` |
|   30570025 |  113 | `	pSet->pBase = 0;` |
|   30570025 |  114 | `	pSet->nUsed = 0;` |
|   30570025 |  115 | `	pSet->nCursor = 0;` |
|   30570025 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   30439306 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   30439311 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        133 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   30439183 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   30439183 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   15219658 |  126 | `}` |
|    6246712 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    6246717 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2392335 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    3854387 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    3854387 |  135 | `	pSet->nUsed--;` |
|    3854387 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    3854387 |  137 | `	return pData;` |
|    3123361 |  138 | `}` |
|   21088556 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   21088561 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|        ! 0 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   21088561 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   21088561 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   10544637 |  148 | `}` |
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
|    1146268 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1146273 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1146273 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1146273 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1146273 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1146273 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1146273 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1146273 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1146273 |  180 | `	pHash->nEntry = 0;` |
|    1146273 |  181 | `	pHash->apBucket = apNew;` |
|    1146273 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1146273 |  183 | `	return SXRET_OK;` |
|     573139 |  184 | `}` |
|     304524 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     304529 |  193 | `	pEntry = pHash->pList;` |
|     160609 |  194 | `	for(;;){` |
|     321223 |  195 | `		if( pHash->nEntry == 0 ){` |
|     304529 |  196 | `			break;` |
|          - |  197 | `		}` |
|      16699 |  198 | `		pNext = pEntry->pNext;` |
|      16699 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      16699 |  200 | `		pEntry = pNext;` |
|      16699 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     304529 |  203 | `	if( pHash->apBucket ){` |
|     304529 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     152262 |  205 | `	}` |
|     304529 |  206 | `	pHash->apBucket = 0;` |
|     304529 |  207 | `	pHash->nBucketSize = 0;` |
|     304529 |  208 | `	pHash->pAllocator = 0;` |
|     304529 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   39472196 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   39472201 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   39472201 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   36887649 |  218 | `	for(;;){` |
|   73754411 |  219 | `		if( pEntry == 0 ){` |
|   15809021 |  220 | `			break;` |
|          - |  221 | `		}` |
|   69776745 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   23663210 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   23663185 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   34282215 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   15809021 |  229 | `	return 0;` |
|   19736613 |  230 | `}` |
|   43007862 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   43007867 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    3535935 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   39471937 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   39471937 |  244 | `	if( pEntry == 0 ){` |
|   15809021 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   23662921 |  247 | `	return (SyHashEntry *)pEntry;` |
|   21504446 |  248 | `}` |
|     204814 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     204819 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     163001 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      81503 |  254 | `	}else{` |
|      41823 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     204819 |  257 | `	if( pEntry->pNextCollide ){` |
|       3684 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       1841 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     204819 |  261 | `	if( pHash->pLast == pEntry ){` |
|     198221 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|      99108 |  263 | `	}` |
|     204819 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     204819 |  265 | `	pHash->nEntry--;` |
|     204819 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|        ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        ! 0 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     204819 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     204819 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        264 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        269 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        269 |  284 | `	if( pEntry == 0 ){` |
|        ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        269 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        269 |  288 | `	return rc;` |
|        137 |  289 | `}` |
|     204550 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     204555 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     204555 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     204555 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    1734974 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    1734979 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    1734979 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   12977800 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   12977805 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    1734717 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    1734717 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   11243093 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   11243093 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   11243093 |  328 | `	return (SyHashEntry *)pEntry;` |
|    6488905 |  329 | `}` |
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
|       2821 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       2811 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       2811 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       2811 |  348 | `		pEntry = pEntry->pNext;` |
|       1406 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      77070 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      77075 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      77075 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      77075 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      77075 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|    9176339 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|    9099269 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|    9099269 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|    9099269 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|    9099269 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    4379136 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    2189269 |  375 | `		}` |
|    9099269 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|    9099269 |  378 | `		pEntry = pEntry->pNext;` |
|    4549637 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      77075 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      77075 |  382 | `	pHash->apBucket = apNew;` |
|      77075 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      77075 |  384 | `	return SXRET_OK;` |
|      38540 |  385 | `}` |
|   11288832 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   11288837 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   11288837 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   11288837 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    7091892 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    3546127 |  393 | `	}` |
|   11288837 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   11288837 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   11288785 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   11288837 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     595237 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     595237 |  408 | `		pHash->pLast = pEntry;` |
|     297616 |  409 | `	}` |
|   11288837 |  410 | `	pHash->nEntry++;` |
|   11288837 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   11288832 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   11288837 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      77075 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      77075 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      38535 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   11288837 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   11288837 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   11288837 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   11288837 |  435 | `	pEntry->pHash = pHash;` |
|   11288837 |  436 | `	pEntry->pKey = pKey;` |
|   11288837 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   11288837 |  438 | `	pEntry->pUserData = pUserData;` |
|   11288837 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   11288837 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   11288837 |  442 | `	return rc;` |
|    5644421 |  443 | `}` |
|   11288704 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   11288709 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     244984 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     244989 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
