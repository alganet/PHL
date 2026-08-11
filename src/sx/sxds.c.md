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
|   81138238 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|   81138243 |   16 | `	pSet->nSize = 0 ;` |
|   81138243 |   17 | `	pSet->nUsed = 0;` |
|   81138243 |   18 | `	pSet->nCursor = 0;` |
|   81138243 |   19 | `	pSet->eSize = ElemSize;` |
|   81138243 |   20 | `	pSet->pAllocator = pAllocator;` |
|   81138243 |   21 | `	pSet->pBase =  0;` |
|   81138243 |   22 | `	pSet->pUserData = 0;` |
|   81138243 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  175179163 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  175179168 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   11783909 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   11783909 |   33 | `		if( pSet->nSize <= 0 ){` |
|   10410081 |   34 | `			pSet->nSize = 4;` |
|    5205038 |   35 | `		}` |
|   11783909 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   11783909 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   11783909 |   40 | `		pSet->pBase = pNew;` |
|   11783909 |   41 | `		pSet->nSize <<= 1;` |
|    5891952 |   42 | `	}` |
|  175179168 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 1290595388 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  175179168 |   45 | `	pSet->nUsed++;` |
|  175179168 |   46 | `	return SXRET_OK;` |
|   87589629 |   47 | `}` |
|    8678336 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|    8678341 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|    8678341 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|    8678341 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    8678341 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|    8678341 |   60 | `	pSet->nSize = nItem;` |
|    8678341 |   61 | `	return SXRET_OK;` |
|    4339173 |   62 | `}` |
|   13596167 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   13596172 |   65 | `	pSet->nUsed   = 0;` |
|   13596172 |   66 | `	pSet->nCursor = 0;` |
|   13596172 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      68544 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      68549 |   71 | `	pSet->nCursor = 0;` |
|      68549 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      72720 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      72725 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29487 |   79 | `		pSet->nCursor = 0;` |
|      29487 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43243 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43243 |   83 | `	if( ppEntry ){` |
|      43243 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21619 |   85 | `	}` |
|      43243 |   86 | `	pSet->nCursor++;` |
|      43243 |   87 | `	return SXRET_OK;` |
|      36365 |   88 | `}` |
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
|    1400546 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    1400551 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1091 |  103 | `		pSet->nUsed = nNewSize;` |
|        543 |  104 | `	}` |
|    1400551 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   30729208 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   30729213 |  109 | `	sxi32 rc = SXRET_OK;` |
|   30729213 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   16253985 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|    8126990 |  112 | `	}` |
|   30729213 |  113 | `	pSet->pBase = 0;` |
|   30729213 |  114 | `	pSet->nUsed = 0;` |
|   30729213 |  115 | `	pSet->nCursor = 0;` |
|   30729213 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   30605480 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   30605485 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        133 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   30605357 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   30605357 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   15302745 |  126 | `}` |
|    6273272 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    6273277 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2394181 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    3879101 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    3879101 |  135 | `	pSet->nUsed--;` |
|    3879101 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    3879101 |  137 | `	return pData;` |
|    3136641 |  138 | `}` |
|   21241015 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   21241020 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|        ! 0 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   21241020 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   21241020 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   10620787 |  148 | `}` |
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
|    1153290 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1153295 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1153295 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1153295 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1153295 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1153295 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1153295 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1153295 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1153295 |  180 | `	pHash->nEntry = 0;` |
|    1153295 |  181 | `	pHash->apBucket = apNew;` |
|    1153295 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1153295 |  183 | `	return SXRET_OK;` |
|     576650 |  184 | `}` |
|     306666 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     306671 |  193 | `	pEntry = pHash->pList;` |
|     161853 |  194 | `	for(;;){` |
|     323711 |  195 | `		if( pHash->nEntry == 0 ){` |
|     306671 |  196 | `			break;` |
|          - |  197 | `		}` |
|      17045 |  198 | `		pNext = pEntry->pNext;` |
|      17045 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      17045 |  200 | `		pEntry = pNext;` |
|      17045 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     306671 |  203 | `	if( pHash->apBucket ){` |
|     306671 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     153333 |  205 | `	}` |
|     306671 |  206 | `	pHash->apBucket = 0;` |
|     306671 |  207 | `	pHash->nBucketSize = 0;` |
|     306671 |  208 | `	pHash->pAllocator = 0;` |
|     306671 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   39761826 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   39761831 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   39761831 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   37062697 |  218 | `	for(;;){` |
|   74459628 |  219 | `		if( pEntry == 0 ){` |
|   15930197 |  220 | `			break;` |
|          - |  221 | `		}` |
|   70445017 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   23831672 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   23831639 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   34697802 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   15930197 |  229 | `	return 0;` |
|   19881428 |  230 | `}` |
|   43318516 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   43318521 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    3556977 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   39761549 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   39761549 |  244 | `	if( pEntry == 0 ){` |
|   15930197 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   23831357 |  247 | `	return (SyHashEntry *)pEntry;` |
|   21659773 |  248 | `}` |
|     206236 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     206241 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     164273 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      82139 |  254 | `	}else{` |
|      41973 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     206241 |  257 | `	if( pEntry->pNextCollide ){` |
|       3754 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       1877 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     206241 |  261 | `	if( pHash->pLast == pEntry ){` |
|     199559 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|      99777 |  263 | `	}` |
|     206241 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     206241 |  265 | `	pHash->nEntry--;` |
|     206241 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|        ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        ! 0 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     206241 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     206241 |  272 | `	return rc;` |
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
|     205954 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     205959 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     205959 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     205959 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    1744942 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    1744947 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    1744947 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   13068480 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   13068485 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    1744685 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    1744685 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   11323805 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   11323805 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   11323805 |  328 | `	return (SyHashEntry *)pEntry;` |
|    6534245 |  329 | `}` |
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
|    4404851 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    2202257 |  375 | `		}` |
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
|   11356616 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   11356621 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   11356621 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   11356621 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    7134600 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    3567288 |  393 | `	}` |
|   11356621 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   11356621 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   11356569 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   11356621 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     599061 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     599061 |  408 | `		pHash->pLast = pEntry;` |
|     299528 |  409 | `	}` |
|   11356621 |  410 | `	pHash->nEntry++;` |
|   11356621 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   11356616 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   11356621 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      77537 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      77537 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      38766 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   11356621 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   11356621 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   11356621 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   11356621 |  435 | `	pEntry->pHash = pHash;` |
|   11356621 |  436 | `	pEntry->pKey = pKey;` |
|   11356621 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   11356621 |  438 | `	pEntry->pUserData = pUserData;` |
|   11356621 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   11356621 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   11356621 |  442 | `	return rc;` |
|    5678313 |  443 | `}` |
|   11356488 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   11356493 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     246700 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     246705 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
