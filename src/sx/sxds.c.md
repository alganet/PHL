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
|   83645378 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|   83645383 |   16 | `	pSet->nSize = 0 ;` |
|   83645383 |   17 | `	pSet->nUsed = 0;` |
|   83645383 |   18 | `	pSet->nCursor = 0;` |
|   83645383 |   19 | `	pSet->eSize = ElemSize;` |
|   83645383 |   20 | `	pSet->pAllocator = pAllocator;` |
|   83645383 |   21 | `	pSet->pBase =  0;` |
|   83645383 |   22 | `	pSet->pUserData = 0;` |
|   83645383 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  180099003 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  180099008 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   12071113 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   12071113 |   33 | `		if( pSet->nSize <= 0 ){` |
|   10640013 |   34 | `			pSet->nSize = 4;` |
|    5320004 |   35 | `		}` |
|   12071113 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   12071113 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   12071113 |   40 | `		pSet->pBase = pNew;` |
|   12071113 |   41 | `		pSet->nSize <<= 1;` |
|    6035554 |   42 | `	}` |
|  180099008 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 1321718448 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  180099008 |   45 | `	pSet->nUsed++;` |
|  180099008 |   46 | `	return SXRET_OK;` |
|   90049549 |   47 | `}` |
|    8859962 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|    8859967 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|    8859967 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|    8859967 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    8859967 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|    8859967 |   60 | `	pSet->nSize = nItem;` |
|    8859967 |   61 | `	return SXRET_OK;` |
|    4429986 |   62 | `}` |
|   13811651 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   13811656 |   65 | `	pSet->nUsed   = 0;` |
|   13811656 |   66 | `	pSet->nCursor = 0;` |
|   13811656 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      68884 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      68889 |   71 | `	pSet->nCursor = 0;` |
|      68889 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73068 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73073 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29639 |   79 | `		pSet->nCursor = 0;` |
|      29639 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43439 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43439 |   83 | `	if( ppEntry ){` |
|      43439 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21717 |   85 | `	}` |
|      43439 |   86 | `	pSet->nCursor++;` |
|      43439 |   87 | `	return SXRET_OK;` |
|      36539 |   88 | `}` |
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
|    1414450 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    1414455 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1179 |  103 | `		pSet->nUsed = nNewSize;` |
|        587 |  104 | `	}` |
|    1414455 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   31117942 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   31117947 |  109 | `	sxi32 rc = SXRET_OK;` |
|   31117947 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   16644093 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|    8322044 |  112 | `	}` |
|   31117947 |  113 | `	pSet->pBase = 0;` |
|   31117947 |  114 | `	pSet->nUsed = 0;` |
|   31117947 |  115 | `	pSet->nCursor = 0;` |
|   31117947 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   31398000 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   31398005 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        133 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   31397877 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   31397877 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   15699005 |  126 | `}` |
|    6194316 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    6194321 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2195415 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    3998911 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    3998911 |  135 | `	pSet->nUsed--;` |
|    3998911 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    3998911 |  137 | `	return pData;` |
|    3097163 |  138 | `}` |
|   21356709 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   21356714 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         22 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   21356694 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   21356694 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   10678704 |  148 | `}` |
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
|    1161724 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1161729 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1161729 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1161729 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1161729 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1161729 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1161729 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1161729 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1161729 |  180 | `	pHash->nEntry = 0;` |
|    1161729 |  181 | `	pHash->apBucket = apNew;` |
|    1161729 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1161729 |  183 | `	return SXRET_OK;` |
|     580867 |  184 | `}` |
|     310910 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     310915 |  193 | `	pEntry = pHash->pList;` |
|     164322 |  194 | `	for(;;){` |
|     328649 |  195 | `		if( pHash->nEntry == 0 ){` |
|     310915 |  196 | `			break;` |
|          - |  197 | `		}` |
|      17739 |  198 | `		pNext = pEntry->pNext;` |
|      17739 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      17739 |  200 | `		pEntry = pNext;` |
|      17739 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     310915 |  203 | `	if( pHash->apBucket ){` |
|     310915 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     155455 |  205 | `	}` |
|     310915 |  206 | `	pHash->apBucket = 0;` |
|     310915 |  207 | `	pHash->nBucketSize = 0;` |
|     310915 |  208 | `	pHash->pAllocator = 0;` |
|     310915 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   40586583 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   40586588 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   40586588 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   38102779 |  218 | `	for(;;){` |
|   76260148 |  219 | `		if( pEntry == 0 ){` |
|   16162618 |  220 | `			break;` |
|          - |  221 | `		}` |
|   72309286 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   24424012 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   24423975 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   35673565 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   16162618 |  229 | `	return 0;` |
|   20293808 |  230 | `}` |
|   44231105 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   44231110 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    3644831 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   40586284 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   40586284 |  244 | `	if( pEntry == 0 ){` |
|   16162618 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   24423671 |  247 | `	return (SyHashEntry *)pEntry;` |
|   22116069 |  248 | `}` |
|     211418 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     211423 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     169023 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      84514 |  254 | `	}else{` |
|      42405 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     211423 |  257 | `	if( pEntry->pNextCollide ){` |
|       3996 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       1997 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     211423 |  261 | `	if( pHash->pLast == pEntry ){` |
|     204683 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     102339 |  263 | `	}` |
|     211423 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     211423 |  265 | `	pHash->nEntry--;` |
|     211423 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|        ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        ! 0 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     211423 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     211423 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        304 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        309 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        309 |  284 | `	if( pEntry == 0 ){` |
|        ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        309 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        309 |  288 | `	return rc;` |
|        157 |  289 | `}` |
|     211114 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     211119 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     211119 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     211119 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    1757786 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    1757791 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    1757791 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   13361442 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   13361447 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    1757527 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    1757527 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   11603925 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   11603925 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   11603925 |  328 | `	return (SyHashEntry *)pEntry;` |
|    6680726 |  329 | `}` |
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
|       3117 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3107 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3107 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3107 |  348 | `		pEntry = pEntry->pNext;` |
|       1554 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      77790 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      77795 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      77795 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      77795 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      77795 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|    9260867 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|    9183077 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|    9183077 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|    9183077 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|    9183077 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    4414109 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    2207114 |  375 | `		}` |
|    9183077 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|    9183077 |  378 | `		pEntry = pEntry->pNext;` |
|    4591541 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      77795 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      77795 |  382 | `	pHash->apBucket = apNew;` |
|      77795 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      77795 |  384 | `	return SXRET_OK;` |
|      38900 |  385 | `}` |
|   11483354 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   11483359 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   11483359 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   11483359 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    7236262 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    3618094 |  393 | `	}` |
|   11483359 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   11483359 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   11483307 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   11483359 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     604875 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     604875 |  408 | `		pHash->pLast = pEntry;` |
|     302435 |  409 | `	}` |
|   11483359 |  410 | `	pHash->nEntry++;` |
|   11483359 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   11483354 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   11483359 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      77795 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      77795 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      38895 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   11483359 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   11483359 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   11483359 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   11483359 |  435 | `	pEntry->pHash = pHash;` |
|   11483359 |  436 | `	pEntry->pKey = pKey;` |
|   11483359 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   11483359 |  438 | `	pEntry->pUserData = pUserData;` |
|   11483359 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   11483359 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   11483359 |  442 | `	return rc;` |
|    5741682 |  443 | `}` |
|   11483226 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   11483231 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     252174 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     252179 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
