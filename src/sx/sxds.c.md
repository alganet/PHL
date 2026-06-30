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
|  19490798 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 |  |
|  19490803 |   16 | `	pSet->nSize = 0 ;` |
|  19490803 |   17 | `	pSet->nUsed = 0;` |
|  19490803 |   18 | `	pSet->nCursor = 0;` |
|  19490803 |   19 | `	pSet->eSize = ElemSize;` |
|  19490803 |   20 | `	pSet->pAllocator = pAllocator;` |
|  19490803 |   21 | `	pSet->pBase =  0;` |
|  19490803 |   22 | `	pSet->pUserData = 0;` |
|  19490803 |   23 | `	return SXRET_OK;` |
|         5 |   24 |  |
|  32185575 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  32185580 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4620361 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4620361 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4461819 |   34 | `			pSet->nSize = 4;` |
|   2230907 |   35 | `		}` |
|   4620361 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4620361 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4620361 |   40 | `		pSet->pBase = pNew;` |
|   4620361 |   41 | `		pSet->nSize <<= 1;` |
|   2310178 |   42 | `	}` |
|  32185580 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 241318988 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  32185580 |   45 | `	pSet->nUsed++;` |
|  32185580 |   46 | `	return SXRET_OK;` |
|  16092835 |   47 |  |
|   1321166 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 |  |
|   1321171 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1321171 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1321171 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1321171 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1321171 |   60 | `	pSet->nSize = nItem;` |
|   1321171 |   61 | `	return SXRET_OK;` |
|    660588 |   62 |  |
|   1824247 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 |  |
|   1824252 |   65 | `	pSet->nUsed   = 0;` |
|   1824252 |   66 | `	pSet->nCursor = 0;` |
|   1824252 |   67 | `	return SXRET_OK;` |
|         5 |   68 |  |
|     58046 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 |  |
|     58051 |   71 | `	pSet->nCursor = 0;` |
|     58051 |   72 | `	return SXRET_OK;` |
|         5 |   73 |  |
|     62252 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     62257 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     24037 |   79 | `		pSet->nCursor = 0;` |
|     24037 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     38225 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     38225 |   83 | `	if( ppEntry ){` |
|     38225 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     19110 |   85 | `	}` |
|     38225 |   86 | `	pSet->nCursor++;` |
|     38225 |   87 | `	return SXRET_OK;` |
|     31131 |   88 |  |
|         - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         8 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|         1 |   91 |  |
|         - |   92 | `	register unsigned char *zSrc;` |
|         9 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         3 |   94 | `		return 0;` |
|         - |   95 | `	}` |
|         7 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|         7 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|         5 |   98 |  |
|         - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    222238 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 |  |
|    222243 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       141 |  103 | `		pSet->nUsed = nNewSize;` |
|        68 |  104 | `	}` |
|    222243 |  105 | `	return SXRET_OK;` |
|         5 |  106 |  |
|  10073808 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 |  |
|  10073813 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10073813 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5053039 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2526517 |  112 | `	}` |
|  10073813 |  113 | `	pSet->pBase = 0;` |
|  10073813 |  114 | `	pSet->nUsed = 0;` |
|  10073813 |  115 | `	pSet->nCursor = 0;` |
|  10073813 |  116 | `	return rc;` |
|         5 |  117 |  |
|   5842878 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5842883 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       133 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5842755 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5842755 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2921444 |  126 |  |
|   3603044 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3603049 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2182303 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1420751 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1420751 |  135 | `	pSet->nUsed--;` |
|   1420751 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1420751 |  137 | `	return pData;` |
|   1801527 |  138 |  |
|  13461717 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  13461722 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13461722 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13461722 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6731233 |  148 |  |
|         - |  149 | `/* Private hash entry */` |
|         - |  150 | `struct SyHashEntry_Pr` |
|         - |  151 |  |
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
|    581038 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    581043 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    581043 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    581043 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    581043 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    581043 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    581043 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    581043 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    581043 |  180 | `	pHash->nEntry = 0;` |
|    581043 |  181 | `	pHash->apBucket = apNew;` |
|    581043 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    581043 |  183 | `	return SXRET_OK;` |
|    290524 |  184 |  |
|    104014 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    104019 |  193 | `	pEntry = pHash->pList;` |
|     55722 |  194 | `	for(;;){` |
|    111449 |  195 | `		if( pHash->nEntry == 0 ){` |
|    104019 |  196 | `			break;` |
|         - |  197 | `		}` |
|      7435 |  198 | `		pNext = pEntry->pNext;` |
|      7435 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      7435 |  200 | `		pEntry = pNext;` |
|      7435 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    104019 |  203 | `	if( pHash->apBucket ){` |
|    104019 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     52007 |  205 | `	}` |
|    104019 |  206 | `	pHash->apBucket = 0;` |
|    104019 |  207 | `	pHash->nBucketSize = 0;` |
|    104019 |  208 | `	pHash->pAllocator = 0;` |
|    104019 |  209 | `	return SXRET_OK;` |
|         5 |  210 |  |
|  17594028 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  17594033 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  17594033 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  15839302 |  218 | `	for(;;){` |
|  31583490 |  219 | `		if( pEntry == 0 ){` |
|   9362117 |  220 | `			break;` |
|         - |  221 | `		}` |
|  26337083 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   8231920 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   8231921 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  13989462 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   9362117 |  229 | `	return 0;` |
|   8797529 |  230 |  |
|  18470192 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  18470197 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    876377 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  17593825 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  17593825 |  244 | `	if( pEntry == 0 ){` |
|   9362117 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   8231713 |  247 | `	return (SyHashEntry *)pEntry;` |
|   9235611 |  248 |  |
|    125048 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    125053 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     96405 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     48205 |  254 | `	}else{` |
|     28653 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    125053 |  257 | `	if( pEntry->pNextCollide ){` |
|      5085 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2542 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    125053 |  261 | `	if( pHash->pLast == pEntry ){` |
|    118763 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     59379 |  263 | `	}` |
|    125053 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    125053 |  265 | `	pHash->nEntry--;` |
|    125053 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    125053 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    125053 |  272 | `	return rc;` |
|         5 |  273 |  |
|       208 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 |  |
|         - |  276 | `	SyHashEntry_Pr *pEntry;` |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( INVALID_HASH(pHash) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|       213 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       213 |  284 | `	if( pEntry == 0 ){` |
|       ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|         - |  286 | `	}` |
|       213 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       213 |  288 | `	return rc;` |
|       109 |  289 |  |
|    124840 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 |  |
|    124845 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    124845 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    124845 |  300 | `	return rc;` |
|         5 |  301 |  |
|   1164730 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 |  |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1164735 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1164735 |  310 | `	return SXRET_OK;` |
|         5 |  311 |  |
|   7393436 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 |  |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   7393441 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1164473 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1164473 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   6228973 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   6228973 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   6228973 |  328 | `	return (SyHashEntry *)pEntry;` |
|   3696723 |  329 |  |
|        10 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         1 |  331 |  |
|         - |  332 | `	SyHashEntry_Pr *pEntry;` |
|         - |  333 | `	sxi32 rc;` |
|         - |  334 | `	sxu32 n;` |
|         - |  335 | `#if defined(UNTRUST)` |
|         - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  337 | `		return 0;` |
|         - |  338 | `	}` |
|         - |  339 | `#endif` |
|        11 |  340 | `	pEntry = pHash->pList;` |
|      1999 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|      1989 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1989 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|      1989 |  348 | `		pEntry = pEntry->pNext;` |
|       995 |  349 | `	}` |
|        11 |  350 | `	return SXRET_OK;` |
|         6 |  351 |  |
|     30526 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  353 |  |
|     30531 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  355 | `	SyHashEntry_Pr *pEntry;` |
|         - |  356 | `	SyHashEntry_Pr **apNew;` |
|         - |  357 | `	sxu32 n,iBucket;` |
|         - |  358 |  |
|         - |  359 | `	/* Allocate a new larger table */` |
|     30531 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     30531 |  361 | `	if( apNew == 0 ){` |
|         - |  362 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  363 | `		return SXRET_OK;` |
|         - |  364 | `	}` |
|         - |  365 | `	/* Zero the new table */` |
|     30531 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  367 | `	/* Rehash all entries */` |
|   3850659 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3820133 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  370 | `		/* Install in the new bucket */` |
|   3820133 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3820133 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3820133 |  373 | `		if( apNew[iBucket] != 0 ){` |
|   1833606 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    916789 |  375 | `		}` |
|   3820133 |  376 | `		apNew[iBucket] = pEntry;` |
|         - |  377 | `		/* Point to the next entry */` |
|   3820133 |  378 | `		pEntry = pEntry->pNext;` |
|   1910069 |  379 | `	}` |
|         - |  380 | `	/* Release the old table and reflect the change */` |
|     30531 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     30531 |  382 | `	pHash->apBucket = apNew;` |
|     30531 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     30531 |  384 | `	return SXRET_OK;` |
|     15268 |  385 |  |
|   5022990 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 |  |
|   5022995 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   5022995 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   5022995 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2845648 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1422829 |  393 | `	}` |
|   5022995 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   5022995 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        33 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        33 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        33 |  401 | `		pHash->pLast = pEntry;` |
|        17 |  402 | `	}else{` |
|   5022963 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   5022995 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    312547 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    312547 |  408 | `		pHash->pLast = pEntry;` |
|    156271 |  409 | `	}` |
|   5022995 |  410 | `	pHash->nEntry++;` |
|   5022995 |  411 | `	return SXRET_OK;` |
|         5 |  412 |  |
|   5022990 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 |  |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   5022995 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     30531 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     30531 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     15263 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   5022995 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   5022995 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   5022995 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   5022995 |  435 | `	pEntry->pHash = pHash;` |
|   5022995 |  436 | `	pEntry->pKey = pKey;` |
|   5022995 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   5022995 |  438 | `	pEntry->pUserData = pUserData;` |
|   5022995 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   5022995 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   5022995 |  442 | `	return rc;` |
|   2511500 |  443 |  |
|   5022920 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 |  |
|   5022925 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|         5 |  447 |  |
|         - |  448 | `/*` |
|         - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|         - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|         - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|         - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|         - |  453 | ` */` |
|        70 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  455 |  |
|        72 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|         2 |  457 |  |
|    161678 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 |  |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    161683 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 |  |
|         - |  468 |  |
