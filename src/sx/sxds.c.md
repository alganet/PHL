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
|  162084460 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  162084465 |   16 | `	pSet->nSize = 0 ;` |
|  162084465 |   17 | `	pSet->nUsed = 0;` |
|  162084465 |   18 | `	pSet->nCursor = 0;` |
|  162084465 |   19 | `	pSet->eSize = ElemSize;` |
|  162084465 |   20 | `	pSet->pAllocator = pAllocator;` |
|  162084465 |   21 | `	pSet->pBase =  0;` |
|  162084465 |   22 | `	pSet->pUserData = 0;` |
|  162084465 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  369042827 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  369042832 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   21530773 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   21530773 |   33 | `		if( pSet->nSize <= 0 ){` |
|   18369873 |   34 | `			pSet->nSize = 4;` |
|    9184934 |   35 | `		}` |
|   21530773 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   21530773 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   21530773 |   40 | `		pSet->pBase = pNew;` |
|   21530773 |   41 | `		pSet->nSize <<= 1;` |
|   10765384 |   42 | `	}` |
|  369042832 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2911085708 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  369042832 |   45 | `	pSet->nUsed++;` |
|  369042832 |   46 | `	return SXRET_OK;` |
|  184521442 |   47 | `}` |
|   18308542 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   18308547 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   18308547 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   18308547 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   18308547 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   18308547 |   60 | `	pSet->nSize = nItem;` |
|   18308547 |   61 | `	return SXRET_OK;` |
|    9154276 |   62 | `}` |
|   27232567 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   27232572 |   65 | `	pSet->nUsed   = 0;` |
|   27232572 |   66 | `	pSet->nCursor = 0;` |
|   27232572 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      70272 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      70277 |   71 | `	pSet->nCursor = 0;` |
|      70277 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      74454 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      74459 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30401 |   79 | `		pSet->nCursor = 0;` |
|      30401 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      44063 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      44063 |   83 | `	if( ppEntry ){` |
|      44063 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      22029 |   85 | `	}` |
|      44063 |   86 | `	pSet->nCursor++;` |
|      44063 |   87 | `	return SXRET_OK;` |
|      37232 |   88 | `}` |
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
|    2753020 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2753025 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1199 |  103 | `		pSet->nUsed = nNewSize;` |
|        597 |  104 | `	}` |
|    2753025 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   56291418 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   56291423 |  109 | `	sxi32 rc = SXRET_OK;` |
|   56291423 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   30657547 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   15328771 |  112 | `	}` |
|   56291423 |  113 | `	pSet->pBase = 0;` |
|   56291423 |  114 | `	pSet->nUsed = 0;` |
|   56291423 |  115 | `	pSet->nCursor = 0;` |
|   56291423 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   65517052 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   65517057 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19227 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   65497835 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   65497835 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   32758531 |  126 | `}` |
|    9405728 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9405733 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2232887 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7172851 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7172851 |  135 | `	pSet->nUsed--;` |
|    7172851 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7172851 |  137 | `	return pData;` |
|    4702869 |  138 | `}` |
|   34399197 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   34399202 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   34399180 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   34399180 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   17199772 |  148 | `}` |
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
|    1770904 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1770909 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1770909 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1770909 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1770909 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1770909 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1770909 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1770909 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1770909 |  180 | `	pHash->nEntry = 0;` |
|    1770909 |  181 | `	pHash->apBucket = apNew;` |
|    1770909 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1770909 |  183 | `	return SXRET_OK;` |
|     885457 |  184 | `}` |
|     412662 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     412667 |  193 | `	pEntry = pHash->pList;` |
|     220827 |  194 | `	for(;;){` |
|     441659 |  195 | `		if( pHash->nEntry == 0 ){` |
|     412667 |  196 | `			break;` |
|          - |  197 | `		}` |
|      28997 |  198 | `		pNext = pEntry->pNext;` |
|      28997 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      28997 |  200 | `		pEntry = pNext;` |
|      28997 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     412667 |  203 | `	if( pHash->apBucket ){` |
|     412667 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     206331 |  205 | `	}` |
|     412667 |  206 | `	pHash->apBucket = 0;` |
|     412667 |  207 | `	pHash->nBucketSize = 0;` |
|     412667 |  208 | `	pHash->pAllocator = 0;` |
|     412667 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   67332761 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   67332766 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   67332766 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   61151507 |  218 | `	for(;;){` |
|  122218073 |  219 | `		if( pEntry == 0 ){` |
|   24370272 |  220 | `			break;` |
|          - |  221 | `		}` |
|  119330925 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   42966520 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   42962499 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   54885312 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   24370272 |  229 | `	return 0;` |
|   33666669 |  230 | `}` |
|   74342621 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   74342626 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    7010341 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   67332290 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   67332290 |  244 | `	if( pEntry == 0 ){` |
|   24370254 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   42962041 |  247 | `	return (SyHashEntry *)pEntry;` |
|   37171599 |  248 | `}` |
|     437348 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     437353 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     356933 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     178469 |  254 | `	}else{` |
|      80425 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     437353 |  257 | `	if( pEntry->pNextCollide ){` |
|       4376 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2187 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     437353 |  261 | `	if( pHash->pLast == pEntry ){` |
|     427365 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     213680 |  263 | `	}` |
|     437353 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     437353 |  265 | `	pHash->nEntry--;` |
|     437353 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     437353 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     437353 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        476 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        481 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        481 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        463 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        463 |  288 | `	return rc;` |
|        243 |  289 | `}` |
|     436890 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     436895 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     436895 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     436895 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2837966 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2837971 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2837971 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   21216446 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   21216451 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2837705 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2837705 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18378751 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18378751 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18378751 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10608228 |  329 | `}` |
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
|       4099 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4085 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4085 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4085 |  348 | `		pEntry = pEntry->pNext;` |
|       2043 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|      91928 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      91933 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      91933 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      91933 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      91933 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14432989 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14341061 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14341061 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14341061 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14341061 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    6875311 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3437425 |  375 | `		}` |
|   14341061 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14341061 |  378 | `		pEntry = pEntry->pNext;` |
|    7170533 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      91933 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      91933 |  382 | `	pHash->apBucket = apNew;` |
|      91933 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      91933 |  384 | `	return SXRET_OK;` |
|      45969 |  385 | `}` |
|   18489512 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   18489517 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   18489517 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   18489517 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11722437 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5861319 |  393 | `	}` |
|   18489517 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   18489517 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   18489465 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   18489517 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     976655 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     976655 |  408 | `		pHash->pLast = pEntry;` |
|     488325 |  409 | `	}` |
|   18489517 |  410 | `	pHash->nEntry++;` |
|   18489517 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   18489512 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   18489517 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      91933 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      91933 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45964 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   18489517 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   18489517 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   18489517 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   18489517 |  435 | `	pEntry->pHash = pHash;` |
|   18489517 |  436 | `	pEntry->pKey = pKey;` |
|   18489517 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   18489517 |  438 | `	pEntry->pUserData = pUserData;` |
|   18489517 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   18489517 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   18489517 |  442 | `	return rc;` |
|    9244761 |  443 | `}` |
|   18489378 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   18489383 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        134 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          2 |  455 | `{` |
|        136 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          2 |  457 | `}` |
|     476648 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     476653 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
