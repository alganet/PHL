# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 289/304 lines (95.07%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits | Line | Source |
| --------: | ---: | :--- |
|         - |    1 | `/**` |
|         - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|         - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |    5 | ` */` |
|         - |    6 | `#include "sxtypes.h"` |
|         - |    7 | `#include "sxmacros.h"` |
|         - |    8 | `#include "sxset.h"` |
|         - |    9 | `#include "sxmem.h"` |
|         - |   10 | `#include "sxhashtable.h"` |
|         - |   11 | `#include "sxhash.h"` |
|         - |   12 | `#include "sxstr.h"` |
|         - |   13 |  |
|  20514752 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 | `{` |
|  20514757 |   16 | `	pSet->nSize = 0 ;` |
|  20514757 |   17 | `	pSet->nUsed = 0;` |
|  20514757 |   18 | `	pSet->nCursor = 0;` |
|  20514757 |   19 | `	pSet->eSize = ElemSize;` |
|  20514757 |   20 | `	pSet->pAllocator = pAllocator;` |
|  20514757 |   21 | `	pSet->pBase =  0;` |
|  20514757 |   22 | `	pSet->pUserData = 0;` |
|  20514757 |   23 | `	return SXRET_OK;` |
|         5 |   24 | `}` |
|  34014507 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 | `{` |
|         - |   27 | `	unsigned char *zbase;` |
|  34014512 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4781759 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4781759 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4612197 |   34 | `			pSet->nSize = 4;` |
|   2306096 |   35 | `		}` |
|   4781759 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4781759 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4781759 |   40 | `		pSet->pBase = pNew;` |
|   4781759 |   41 | `		pSet->nSize <<= 1;` |
|   2390877 |   42 | `	}` |
|  34014512 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 254782720 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  34014512 |   45 | `	pSet->nUsed++;` |
|  34014512 |   46 | `	return SXRET_OK;` |
|  17007302 |   47 | `}` |
|   1421186 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 | `{` |
|   1421191 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1421191 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1421191 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1421191 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1421191 |   60 | `	pSet->nSize = nItem;` |
|   1421191 |   61 | `	return SXRET_OK;` |
|    710598 |   62 | `}` |
|   2286757 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 | `{` |
|   2286762 |   65 | `	pSet->nUsed   = 0;` |
|   2286762 |   66 | `	pSet->nCursor = 0;` |
|   2286762 |   67 | `	return SXRET_OK;` |
|         5 |   68 | `}` |
|     59124 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 | `{` |
|     59129 |   71 | `	pSet->nCursor = 0;` |
|     59129 |   72 | `	return SXRET_OK;` |
|         5 |   73 | `}` |
|     63322 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 | `{` |
|         - |   76 | `	register unsigned char *zSrc;` |
|     63327 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     24617 |   79 | `		pSet->nCursor = 0;` |
|     24617 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     38715 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     38715 |   83 | `	if( ppEntry ){` |
|     38715 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     19355 |   85 | `	}` |
|     38715 |   86 | `	pSet->nCursor++;` |
|     38715 |   87 | `	return SXRET_OK;` |
|     31666 |   88 | `}` |
|         - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         8 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|         1 |   91 | `{` |
|         - |   92 | `	register unsigned char *zSrc;` |
|         9 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         3 |   94 | `		return 0;` |
|         - |   95 | `	}` |
|         7 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|         7 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|         5 |   98 | `}` |
|         - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    238834 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 | `{` |
|    238839 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       529 |  103 | `		pSet->nUsed = nNewSize;` |
|       262 |  104 | `	}` |
|    238839 |  105 | `	return SXRET_OK;` |
|         5 |  106 | `}` |
|  10455448 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 | `{` |
|  10455453 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10455453 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5245089 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2622542 |  112 | `	}` |
|  10455453 |  113 | `	pSet->pBase = 0;` |
|  10455453 |  114 | `	pSet->nUsed = 0;` |
|  10455453 |  115 | `	pSet->nCursor = 0;` |
|  10455453 |  116 | `	return rc;` |
|         5 |  117 | `}` |
|   6113524 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 | `{` |
|         - |  120 | `	const char *zBase;` |
|   6113529 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       133 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   6113401 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   6113401 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   3056767 |  126 | `}` |
|   3673238 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 | `{` |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3673243 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2189897 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1483351 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1483351 |  135 | `	pSet->nUsed--;` |
|   1483351 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1483351 |  137 | `	return pData;` |
|   1836624 |  138 | `}` |
|  13937099 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	const char *zBase;` |
|  13937104 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13937104 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13937104 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6968991 |  148 | `}` |
|         - |  149 | `/* Private hash entry */` |
|         - |  150 | `struct SyHashEntry_Pr` |
|         - |  151 | `{` |
|         - |  152 | `	const void *pKey; /* Hash key */` |
|         - |  153 | `	sxu32 nKeyLen;    /* Key length */` |
|         - |  154 | `	void *pUserData;  /* User private data */` |
|         - |  155 | `	/* Private fields */` |
|         - |  156 | `	sxu32 nHash;` |
|         - |  157 | `	SyHash *pHash;` |
|         - |  158 | `	SyHashEntry_Pr *pNext,*pPrev; /* Next and previous entry in the list */` |
|         - |  159 | `	SyHashEntry_Pr *pNextCollide,*pPrevCollide; /* Collision list */` |
|         - |  160 | `};` |
|         - |  161 | `#define INVALID_HASH(H) ((H)->apBucket == 0)` |
|    672700 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 | `{` |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    672705 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    672705 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    672705 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    672705 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    672705 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    672705 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    672705 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    672705 |  180 | `	pHash->nEntry = 0;` |
|    672705 |  181 | `	pHash->apBucket = apNew;` |
|    672705 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    672705 |  183 | `	return SXRET_OK;` |
|    336355 |  184 | `}` |
|    149232 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 | `{` |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    149237 |  193 | `	pEntry = pHash->pList;` |
|     78878 |  194 | `	for(;;){` |
|    157761 |  195 | `		if( pHash->nEntry == 0 ){` |
|    149237 |  196 | `			break;` |
|         - |  197 | `		}` |
|      8529 |  198 | `		pNext = pEntry->pNext;` |
|      8529 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      8529 |  200 | `		pEntry = pNext;` |
|      8529 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    149237 |  203 | `	if( pHash->apBucket ){` |
|    149237 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     74616 |  205 | `	}` |
|    149237 |  206 | `	pHash->apBucket = 0;` |
|    149237 |  207 | `	pHash->nBucketSize = 0;` |
|    149237 |  208 | `	pHash->pAllocator = 0;` |
|    149237 |  209 | `	return SXRET_OK;` |
|         5 |  210 | `}` |
|  18805504 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 | `{` |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  18805509 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  18805509 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  17024354 |  218 | `	for(;;){` |
|  33900808 |  219 | `		if( pEntry == 0 ){` |
|  10013613 |  220 | `			break;` |
|         - |  221 | `		}` |
|  28282892 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   8791906 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   8791901 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  15095304 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|  10013613 |  229 | `	return 0;` |
|   9403279 |  230 | `}` |
|  19762984 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 | `{` |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  19762989 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    957709 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  18805285 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  18805285 |  244 | `	if( pEntry == 0 ){` |
|  10013613 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   8791677 |  247 | `	return (SyHashEntry *)pEntry;` |
|   9882019 |  248 | `}` |
|    148044 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 | `{` |
|         - |  251 | `	sxi32 rc;` |
|    148049 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    117199 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     58602 |  254 | `	}else{` |
|     30855 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    148049 |  257 | `	if( pEntry->pNextCollide ){` |
|      5382 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2690 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    148049 |  261 | `	if( pHash->pLast == pEntry ){` |
|    141547 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     70771 |  263 | `	}` |
|    148049 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    148049 |  265 | `	pHash->nEntry--;` |
|    148049 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    148049 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    148049 |  272 | `	return rc;` |
|         5 |  273 | `}` |
|       224 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 | `{` |
|         - |  276 | `	SyHashEntry_Pr *pEntry;` |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( INVALID_HASH(pHash) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|       229 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       229 |  284 | `	if( pEntry == 0 ){` |
|       ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|         - |  286 | `	}` |
|       229 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       229 |  288 | `	return rc;` |
|       117 |  289 | `}` |
|    147820 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 | `{` |
|    147825 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    147825 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    147825 |  300 | `	return rc;` |
|         5 |  301 | `}` |
|   1328016 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 | `{` |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1328021 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1328021 |  310 | `	return SXRET_OK;` |
|         5 |  311 | `}` |
|   8334558 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 | `{` |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   8334563 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1327759 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1327759 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   7006809 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   7006809 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   7006809 |  328 | `	return (SyHashEntry *)pEntry;` |
|   4167284 |  329 | `}` |
|        10 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         1 |  331 | `{` |
|         - |  332 | `	SyHashEntry_Pr *pEntry;` |
|         - |  333 | `	sxi32 rc;` |
|         - |  334 | `	sxu32 n;` |
|         - |  335 | `#if defined(UNTRUST)` |
|         - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  337 | `		return 0;` |
|         - |  338 | `	}` |
|         - |  339 | `#endif` |
|        11 |  340 | `	pEntry = pHash->pList;` |
|      2043 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|      2033 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      2033 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|      2033 |  348 | `		pEntry = pEntry->pNext;` |
|      1017 |  349 | `	}` |
|        11 |  350 | `	return SXRET_OK;` |
|         6 |  351 | `}` |
|     32782 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  353 | `{` |
|     32787 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  355 | `	SyHashEntry_Pr *pEntry;` |
|         - |  356 | `	SyHashEntry_Pr **apNew;` |
|         - |  357 | `	sxu32 n,iBucket;` |
|         - |  358 |  |
|         - |  359 | `	/* Allocate a new larger table */` |
|     32787 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     32787 |  361 | `	if( apNew == 0 ){` |
|         - |  362 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  363 | `		return SXRET_OK;` |
|         - |  364 | `	}` |
|         - |  365 | `	/* Zero the new table */` |
|     32787 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  367 | `	/* Rehash all entries */` |
|   4132563 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   4099781 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  370 | `		/* Install in the new bucket */` |
|   4099781 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   4099781 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   4099781 |  373 | `		if( apNew[iBucket] != 0 ){` |
|   1967895 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    983952 |  375 | `		}` |
|   4099781 |  376 | `		apNew[iBucket] = pEntry;` |
|         - |  377 | `		/* Point to the next entry */` |
|   4099781 |  378 | `		pEntry = pEntry->pNext;` |
|   2049893 |  379 | `	}` |
|         - |  380 | `	/* Release the old table and reflect the change */` |
|     32787 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     32787 |  382 | `	pHash->apBucket = apNew;` |
|     32787 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     32787 |  384 | `	return SXRET_OK;` |
|     16396 |  385 | `}` |
|   5512402 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 | `{` |
|   5512407 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   5512407 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   5512407 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   3091277 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1545672 |  393 | `	}` |
|   5512407 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   5512407 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        51 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        51 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        51 |  401 | `		pHash->pLast = pEntry;` |
|        26 |  402 | `	}else{` |
|   5512357 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   5512407 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    353081 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    353081 |  408 | `		pHash->pLast = pEntry;` |
|    176538 |  409 | `	}` |
|   5512407 |  410 | `	pHash->nEntry++;` |
|   5512407 |  411 | `	return SXRET_OK;` |
|         5 |  412 | `}` |
|   5512402 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 | `{` |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   5512407 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     32787 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     32787 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     16391 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   5512407 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   5512407 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   5512407 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   5512407 |  435 | `	pEntry->pHash = pHash;` |
|   5512407 |  436 | `	pEntry->pKey = pKey;` |
|   5512407 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   5512407 |  438 | `	pEntry->pUserData = pUserData;` |
|   5512407 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   5512407 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   5512407 |  442 | `	return rc;` |
|   2756206 |  443 | `}` |
|   5512286 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 | `{` |
|   5512291 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|         5 |  447 | `}` |
|         - |  448 | `/*` |
|         - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|         - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|         - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|         - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|         - |  453 | ` */` |
|       116 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  455 | `{` |
|       118 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|         2 |  457 | `}` |
|    187570 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 | `{` |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    187575 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 | `}` |
|         - |  468 |  |
