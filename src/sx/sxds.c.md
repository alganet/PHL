# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 290/304 lines (95.39%)

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
|   84818230 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|   84818235 |   16 | `	pSet->nSize = 0 ;` |
|   84818235 |   17 | `	pSet->nUsed = 0;` |
|   84818235 |   18 | `	pSet->nCursor = 0;` |
|   84818235 |   19 | `	pSet->eSize = ElemSize;` |
|   84818235 |   20 | `	pSet->pAllocator = pAllocator;` |
|   84818235 |   21 | `	pSet->pBase =  0;` |
|   84818235 |   22 | `	pSet->pUserData = 0;` |
|   84818235 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  183312159 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  183312164 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   12306527 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   12306527 |   33 | `		if( pSet->nSize <= 0 ){` |
|   10824687 |   34 | `			pSet->nSize = 4;` |
|    5412341 |   35 | `		}` |
|   12306527 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   12306527 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   12306527 |   40 | `		pSet->pBase = pNew;` |
|   12306527 |   41 | `		pSet->nSize <<= 1;` |
|    6153261 |   42 | `	}` |
|  183312164 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 1343512156 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  183312164 |   45 | `	pSet->nUsed++;` |
|  183312164 |   46 | `	return SXRET_OK;` |
|   91656127 |   47 | `}` |
|    8938426 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|    8938431 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|    8938431 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|    8938431 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    8938431 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|    8938431 |   60 | `	pSet->nSize = nItem;` |
|    8938431 |   61 | `	return SXRET_OK;` |
|    4469218 |   62 | `}` |
|   13907143 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   13907148 |   65 | `	pSet->nUsed   = 0;` |
|   13907148 |   66 | `	pSet->nCursor = 0;` |
|   13907148 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      68938 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      68943 |   71 | `	pSet->nCursor = 0;` |
|      68943 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73122 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73127 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29663 |   79 | `		pSet->nCursor = 0;` |
|      29663 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43469 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43469 |   83 | `	if( ppEntry ){` |
|      43469 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21732 |   85 | `	}` |
|      43469 |   86 | `	pSet->nCursor++;` |
|      43469 |   87 | `	return SXRET_OK;` |
|      36566 |   88 | `}` |
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
|    1418396 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    1418401 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1179 |  103 | `		pSet->nUsed = nNewSize;` |
|        587 |  104 | `	}` |
|    1418401 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   31437694 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   31437699 |  109 | `	sxi32 rc = SXRET_OK;` |
|   31437699 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   16879559 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|    8439777 |  112 | `	}` |
|   31437699 |  113 | `	pSet->pBase = 0;` |
|   31437699 |  114 | `	pSet->nUsed = 0;` |
|   31437699 |  115 | `	pSet->nCursor = 0;` |
|   31437699 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   31945458 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   31945463 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        133 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   31945335 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   31945335 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   15972734 |  126 | `}` |
|    6254324 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    6254329 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2195421 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    4058913 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    4058913 |  135 | `	pSet->nUsed--;` |
|    4058913 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    4058913 |  137 | `	return pData;` |
|    3127167 |  138 | `}` |
|   21560827 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   21560832 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   21560810 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   21560810 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   10780751 |  148 | `}` |
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
|    1174030 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1174035 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1174035 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1174035 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1174035 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1174035 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1174035 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1174035 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1174035 |  180 | `	pHash->nEntry = 0;` |
|    1174035 |  181 | `	pHash->apBucket = apNew;` |
|    1174035 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1174035 |  183 | `	return SXRET_OK;` |
|     587020 |  184 | `}` |
|     311354 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     311359 |  193 | `	pEntry = pHash->pList;` |
|     164702 |  194 | `	for(;;){` |
|     329409 |  195 | `		if( pHash->nEntry == 0 ){` |
|     311359 |  196 | `			break;` |
|          - |  197 | `		}` |
|      18055 |  198 | `		pNext = pEntry->pNext;` |
|      18055 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      18055 |  200 | `		pEntry = pNext;` |
|      18055 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     311359 |  203 | `	if( pHash->apBucket ){` |
|     311359 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     155677 |  205 | `	}` |
|     311359 |  206 | `	pHash->apBucket = 0;` |
|     311359 |  207 | `	pHash->nBucketSize = 0;` |
|     311359 |  208 | `	pHash->pAllocator = 0;` |
|     311359 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   41049827 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   41049832 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   41049832 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   38388363 |  218 | `	for(;;){` |
|   76795038 |  219 | `		if( pEntry == 0 ){` |
|   16262434 |  220 | `			break;` |
|          - |  221 | `		}` |
|   72926074 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   24787440 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   24787403 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   35745211 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   16262434 |  229 | `	return 0;` |
|   20525430 |  230 | `}` |
|   44819333 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   44819338 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    3769825 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   41049518 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   41049518 |  244 | `	if( pEntry == 0 ){` |
|   16262434 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   24787089 |  247 | `	return (SyHashEntry *)pEntry;` |
|   22410183 |  248 | `}` |
|     212044 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     212049 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     169605 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      84805 |  254 | `	}else{` |
|      42449 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     212049 |  257 | `	if( pEntry->pNextCollide ){` |
|       4154 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2076 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     212049 |  261 | `	if( pHash->pLast == pEntry ){` |
|     205299 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     102647 |  263 | `	}` |
|     212049 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     212049 |  265 | `	pHash->nEntry--;` |
|     212049 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|        ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        ! 0 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     212049 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     212049 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        314 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        319 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        319 |  284 | `	if( pEntry == 0 ){` |
|        ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        319 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        319 |  288 | `	return rc;` |
|        162 |  289 | `}` |
|     211730 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     211735 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     211735 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     211735 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    1796536 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    1796541 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    1796541 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   13488584 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   13488589 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    1796275 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    1796275 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   11692319 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   11692319 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   11692319 |  328 | `	return (SyHashEntry *)pEntry;` |
|    6744297 |  329 | `}` |
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
|       3123 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3113 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3113 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3113 |  348 | `		pEntry = pEntry->pNext;` |
|       1557 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      77794 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      77799 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      77799 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      77799 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      77799 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|    9262407 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|    9184613 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|    9184613 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|    9184613 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|    9184613 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    4415290 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    2207686 |  375 | `		}` |
|    9184613 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|    9184613 |  378 | `		pEntry = pEntry->pNext;` |
|    4592309 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      77799 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      77799 |  382 | `	pHash->apBucket = apNew;` |
|      77799 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      77799 |  384 | `	return SXRET_OK;` |
|      38902 |  385 | `}` |
|   11573186 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   11573191 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   11573191 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   11573191 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    7292720 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    3646322 |  393 | `	}` |
|   11573191 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   11573191 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   11573139 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   11573191 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     613137 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     613137 |  408 | `		pHash->pLast = pEntry;` |
|     306566 |  409 | `	}` |
|   11573191 |  410 | `	pHash->nEntry++;` |
|   11573191 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   11573186 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   11573191 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      77799 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      77799 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      38897 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   11573191 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   11573191 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   11573191 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   11573191 |  435 | `	pEntry->pHash = pHash;` |
|   11573191 |  436 | `	pEntry->pKey = pKey;` |
|   11573191 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   11573191 |  438 | `	pEntry->pUserData = pUserData;` |
|   11573191 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   11573191 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   11573191 |  442 | `	return rc;` |
|    5786598 |  443 | `}` |
|   11573058 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   11573063 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     252862 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     252867 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
