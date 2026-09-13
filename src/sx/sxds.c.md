# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 293/304 lines (96.38%)

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
|  172883546 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  172883551 |   16 | `	pSet->nSize = 0 ;` |
|  172883551 |   17 | `	pSet->nUsed = 0;` |
|  172883551 |   18 | `	pSet->nCursor = 0;` |
|  172883551 |   19 | `	pSet->eSize = ElemSize;` |
|  172883551 |   20 | `	pSet->pAllocator = pAllocator;` |
|  172883551 |   21 | `	pSet->pBase =  0;` |
|  172883551 |   22 | `	pSet->pUserData = 0;` |
|  172883551 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  391718085 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  391718090 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   22570798 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   22570798 |   33 | `		if( pSet->nSize <= 0 ){` |
|   19226900 |   34 | `			pSet->nSize = 4;` |
|    9614122 |   35 | `		}` |
|   22570798 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   22570798 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   22570798 |   40 | `		pSet->pBase = pNew;` |
|   22570798 |   41 | `		pSet->nSize <<= 1;` |
|   11286071 |   42 | `	}` |
|  391718090 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3089603172 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  391718090 |   45 | `	pSet->nUsed++;` |
|  391718090 |   46 | `	return SXRET_OK;` |
|  195861506 |   47 | `}` |
|   19465462 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   19465467 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   19465467 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   19465467 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   19465467 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   19465467 |   60 | `	pSet->nSize = nItem;` |
|   19465467 |   61 | `	return SXRET_OK;` |
|    9732736 |   62 | `}` |
|   28978765 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   28978770 |   65 | `	pSet->nUsed   = 0;` |
|   28978770 |   66 | `	pSet->nCursor = 0;` |
|   28978770 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      71260 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      71265 |   71 | `	pSet->nCursor = 0;` |
|      71265 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      71678 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      71683 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30847 |   79 | `		pSet->nCursor = 0;` |
|      30847 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      40841 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      40841 |   83 | `	if( ppEntry ){` |
|      40841 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      20418 |   85 | `	}` |
|      40841 |   86 | `	pSet->nCursor++;` |
|      40841 |   87 | `	return SXRET_OK;` |
|      35844 |   88 | `}` |
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
|    3010638 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3010643 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1203 |  103 | `		pSet->nUsed = nNewSize;` |
|        599 |  104 | `	}` |
|    3010643 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   59502644 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   59502649 |  109 | `	sxi32 rc = SXRET_OK;` |
|   59502649 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   32196964 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   16099154 |  112 | `	}` |
|   59502649 |  113 | `	pSet->pBase = 0;` |
|   59502649 |  114 | `	pSet->nUsed = 0;` |
|   59502649 |  115 | `	pSet->nCursor = 0;` |
|   59502649 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   69683062 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   69683067 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19559 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   69663513 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   69663513 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   34841536 |  126 | `}` |
|    9708945 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9708950 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2236319 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7472636 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7472636 |  135 | `	pSet->nUsed--;` |
|    7472636 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7472636 |  137 | `	return pData;` |
|    4854927 |  138 | `}` |
|   35932839 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   35932844 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   35932792 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   35932792 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   17970565 |  148 | `}` |
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
|    1906328 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1906333 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1906333 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1906333 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1906333 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1906333 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1906333 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1906333 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1906333 |  180 | `	pHash->nEntry = 0;` |
|    1906333 |  181 | `	pHash->apBucket = apNew;` |
|    1906333 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1906333 |  183 | `	return SXRET_OK;` |
|     953244 |  184 | `}` |
|     442838 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     442843 |  193 | `	pEntry = pHash->pList;` |
|     236186 |  194 | `	for(;;){` |
|     472227 |  195 | `		if( pHash->nEntry == 0 ){` |
|     442843 |  196 | `			break;` |
|          - |  197 | `		}` |
|      29389 |  198 | `		pNext = pEntry->pNext;` |
|      29389 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      29389 |  200 | `		pEntry = pNext;` |
|      29389 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     442843 |  203 | `	if( pHash->apBucket ){` |
|     442843 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     221494 |  205 | `	}` |
|     442843 |  206 | `	pHash->apBucket = 0;` |
|     442843 |  207 | `	pHash->nBucketSize = 0;` |
|     442843 |  208 | `	pHash->pAllocator = 0;` |
|     442843 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   72772634 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   72772639 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   72772639 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   65578625 |  218 | `	for(;;){` |
|  131289464 |  219 | `		if( pEntry == 0 ){` |
|   26763662 |  220 | `			break;` |
|          - |  221 | `		}` |
|  127529412 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   46013031 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   46008982 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   58516830 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   26763662 |  229 | `	return 0;` |
|   36392956 |  230 | `}` |
|   80214998 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   80215003 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    7442809 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   72772199 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   72772199 |  244 | `	if( pEntry == 0 ){` |
|   26763644 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   46008560 |  247 | `	return (SyHashEntry *)pEntry;` |
|   40114213 |  248 | `}` |
|     488638 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     488643 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     399581 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     200168 |  254 | `	}else{` |
|      89067 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     488643 |  257 | `	if( pEntry->pNextCollide ){` |
|       4663 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2329 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     488643 |  261 | `	if( pHash->pLast == pEntry ){` |
|     478561 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     239728 |  263 | `	}` |
|     488643 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     488643 |  265 | `	pHash->nEntry--;` |
|     488643 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     488643 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     488643 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        440 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        445 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        445 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        427 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        427 |  288 | `	return rc;` |
|        225 |  289 | `}` |
|     488216 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     488221 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     488221 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     488221 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3114862 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3114867 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3114867 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   23962366 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   23962371 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3114601 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3114601 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   20847775 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   20847775 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   20847775 |  328 | `	return (SyHashEntry *)pEntry;` |
|   11981188 |  329 | `}` |
|         14 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|          1 |  331 | `{` |
|          - |  332 | `	SyHashEntry_Pr *pEntry;` |
|          - |  333 | `	sxi32 rc;` |
|          - |  334 | `	sxu32 n;` |
|          - |  335 | `#if defined(UNTRUST)` |
|          - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|          - |  337 | `		return 0;` |
|          - |  338 | `	}` |
|          - |  339 | `#endif` |
|         15 |  340 | `	pEntry = pHash->pList;` |
|       4563 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4549 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4549 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4549 |  348 | `		pEntry = pEntry->pNext;` |
|       2275 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|     100440 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100445 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100445 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100445 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100445 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18652061 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18551621 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18551621 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18551621 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18551621 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8891999 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4446032 |  375 | `		}` |
|   18551621 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18551621 |  378 | `		pEntry = pEntry->pNext;` |
|    9275813 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100445 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100445 |  382 | `	pHash->apBucket = apNew;` |
|     100445 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100445 |  384 | `	return SXRET_OK;` |
|      50225 |  385 | `}` |
|   20584064 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   20584069 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   20584069 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   20584069 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   13130083 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    6565092 |  393 | `	}` |
|   20584069 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   20584069 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         63 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         63 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         63 |  401 | `		pHash->pLast = pEntry;` |
|         33 |  402 | `	}else{` |
|   20584009 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   20584069 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1067319 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1067319 |  408 | `		pHash->pLast = pEntry;` |
|     533732 |  409 | `	}` |
|   20584069 |  410 | `	pHash->nEntry++;` |
|   20584069 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   20584064 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   20584069 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100445 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100445 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50220 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   20584069 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   20584069 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   20584069 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   20584069 |  435 | `	pEntry->pHash = pHash;` |
|   20584069 |  436 | `	pEntry->pKey = pKey;` |
|   20584069 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   20584069 |  438 | `	pEntry->pUserData = pUserData;` |
|   20584069 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   20584069 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   20584069 |  442 | `	return rc;` |
|   10292487 |  443 | `}` |
|   20583912 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   20583917 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        152 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          4 |  455 | `{` |
|        156 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          4 |  457 | `}` |
|     528726 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     528731 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
