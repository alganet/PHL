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
|   81104256 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|   81104261 |   16 | `	pSet->nSize = 0 ;` |
|   81104261 |   17 | `	pSet->nUsed = 0;` |
|   81104261 |   18 | `	pSet->nCursor = 0;` |
|   81104261 |   19 | `	pSet->eSize = ElemSize;` |
|   81104261 |   20 | `	pSet->pAllocator = pAllocator;` |
|   81104261 |   21 | `	pSet->pBase =  0;` |
|   81104261 |   22 | `	pSet->pUserData = 0;` |
|   81104261 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  175145025 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  175145030 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   11776517 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   11776517 |   33 | `		if( pSet->nSize <= 0 ){` |
|   10402803 |   34 | `			pSet->nSize = 4;` |
|    5201399 |   35 | `		}` |
|   11776517 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   11776517 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   11776517 |   40 | `		pSet->pBase = pNew;` |
|   11776517 |   41 | `		pSet->nSize <<= 1;` |
|    5888256 |   42 | `	}` |
|  175145030 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 1290407810 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  175145030 |   45 | `	pSet->nUsed++;` |
|  175145030 |   46 | `	return SXRET_OK;` |
|   87572560 |   47 | `}` |
|    8677058 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|    8677063 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|    8677063 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|    8677063 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    8677063 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|    8677063 |   60 | `	pSet->nSize = nItem;` |
|    8677063 |   61 | `	return SXRET_OK;` |
|    4338534 |   62 | `}` |
|   13592505 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   13592510 |   65 | `	pSet->nUsed   = 0;` |
|   13592510 |   66 | `	pSet->nCursor = 0;` |
|   13592510 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      68444 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      68449 |   71 | `	pSet->nCursor = 0;` |
|      68449 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      72620 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      72625 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29447 |   79 | `		pSet->nCursor = 0;` |
|      29447 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43183 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43183 |   83 | `	if( ppEntry ){` |
|      43183 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21589 |   85 | `	}` |
|      43183 |   86 | `	pSet->nCursor++;` |
|      43183 |   87 | `	return SXRET_OK;` |
|      36315 |   88 | `}` |
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
|    1400060 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    1400065 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        683 |  103 | `		pSet->nUsed = nNewSize;` |
|        339 |  104 | `	}` |
|    1400065 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   30714760 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   30714765 |  109 | `	sxi32 rc = SXRET_OK;` |
|   30714765 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   16245853 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|    8122924 |  112 | `	}` |
|   30714765 |  113 | `	pSet->pBase = 0;` |
|   30714765 |  114 | `	pSet->nUsed = 0;` |
|   30714765 |  115 | `	pSet->nCursor = 0;` |
|   30714765 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   30602190 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   30602195 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        133 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   30602067 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   30602067 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   15301100 |  126 | `}` |
|    6267216 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    6267221 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2394171 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    3873055 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    3873055 |  135 | `	pSet->nUsed--;` |
|    3873055 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    3873055 |  137 | `	return pData;` |
|    3133613 |  138 | `}` |
|   21205898 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   21205903 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|        ! 0 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   21205903 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   21205903 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   10603257 |  148 | `}` |
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
|    1152728 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1152733 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1152733 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1152733 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1152733 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1152733 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1152733 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1152733 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1152733 |  180 | `	pHash->nEntry = 0;` |
|    1152733 |  181 | `	pHash->apBucket = apNew;` |
|    1152733 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1152733 |  183 | `	return SXRET_OK;` |
|     576369 |  184 | `}` |
|     306176 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     306181 |  193 | `	pEntry = pHash->pList;` |
|     161586 |  194 | `	for(;;){` |
|     323177 |  195 | `		if( pHash->nEntry == 0 ){` |
|     306181 |  196 | `			break;` |
|          - |  197 | `		}` |
|      17001 |  198 | `		pNext = pEntry->pNext;` |
|      17001 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      17001 |  200 | `		pEntry = pNext;` |
|      17001 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     306181 |  203 | `	if( pHash->apBucket ){` |
|     306181 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     153088 |  205 | `	}` |
|     306181 |  206 | `	pHash->apBucket = 0;` |
|     306181 |  207 | `	pHash->nBucketSize = 0;` |
|     306181 |  208 | `	pHash->pAllocator = 0;` |
|     306181 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   39740188 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   39740193 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   39740193 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   37209518 |  218 | `	for(;;){` |
|   74411403 |  219 | `		if( pEntry == 0 ){` |
|   15919747 |  220 | `			break;` |
|          - |  221 | `		}` |
|   70401648 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   23820484 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   23820451 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   34671215 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   15919747 |  229 | `	return 0;` |
|   19870609 |  230 | `}` |
|   43296124 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   43296129 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    3556223 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   39739911 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   39739911 |  244 | `	if( pEntry == 0 ){` |
|   15919747 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   23820169 |  247 | `	return (SyHashEntry *)pEntry;` |
|   21648577 |  248 | `}` |
|     205648 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     205653 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     163719 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      81862 |  254 | `	}else{` |
|      41939 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     205653 |  257 | `	if( pEntry->pNextCollide ){` |
|       3722 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       1860 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     205653 |  261 | `	if( pHash->pLast == pEntry ){` |
|     198981 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|      99488 |  263 | `	}` |
|     205653 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     205653 |  265 | `	pHash->nEntry--;` |
|     205653 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|        ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        ! 0 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     205653 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     205653 |  272 | `	return rc;` |
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
|     205366 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     205371 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     205371 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     205371 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    1744424 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    1744429 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    1744429 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   13050344 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   13050349 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    1744167 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    1744167 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   11306187 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   11306187 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   11306187 |  328 | `	return (SyHashEntry *)pEntry;` |
|    6525177 |  329 | `}` |
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
|      77532 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      77537 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      77537 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      77537 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      77537 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|    9231713 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|    9154181 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|    9154181 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|    9154181 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|    9154181 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    4404833 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    2202641 |  375 | `		}` |
|    9154181 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|    9154181 |  378 | `		pEntry = pEntry->pNext;` |
|    4577093 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      77537 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      77537 |  382 | `	pHash->apBucket = apNew;` |
|      77537 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      77537 |  384 | `	return SXRET_OK;` |
|      38771 |  385 | `}` |
|   11355688 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   11355693 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   11355693 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   11355693 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    7134276 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    3566953 |  393 | `	}` |
|   11355693 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   11355693 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   11355641 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   11355693 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     598617 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     598617 |  408 | `		pHash->pLast = pEntry;` |
|     299306 |  409 | `	}` |
|   11355693 |  410 | `	pHash->nEntry++;` |
|   11355693 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   11355688 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   11355693 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      77537 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      77537 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      38766 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   11355693 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   11355693 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   11355693 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   11355693 |  435 | `	pEntry->pHash = pHash;` |
|   11355693 |  436 | `	pEntry->pKey = pKey;` |
|   11355693 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   11355693 |  438 | `	pEntry->pUserData = pUserData;` |
|   11355693 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   11355693 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   11355693 |  442 | `	return rc;` |
|    5677849 |  443 | `}` |
|   11355560 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   11355565 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     246056 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     246061 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
