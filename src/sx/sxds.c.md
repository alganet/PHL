# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 272/287 lines (94.77%)

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
|   9735130 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|   9735132 |   16 | `	pSet->nSize = 0 ;` |
|   9735132 |   17 | `	pSet->nUsed = 0;` |
|   9735132 |   18 | `	pSet->nCursor = 0;` |
|   9735132 |   19 | `	pSet->eSize = ElemSize;` |
|   9735132 |   20 | `	pSet->pAllocator = pAllocator;` |
|   9735132 |   21 | `	pSet->pBase =  0;` |
|   9735132 |   22 | `	pSet->pUserData = 0;` |
|   9735132 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  15346368 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  15346370 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3202870 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3202870 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3145162 |   34 | `			pSet->nSize = 4;` |
|   1572580 |   35 | `		}` |
|   3202870 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3202870 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3202870 |   40 | `		pSet->pBase = pNew;` |
|   3202870 |   41 | `		pSet->nSize <<= 1;` |
|   1601434 |   42 | `	}` |
|  15346370 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 116172910 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  15346370 |   45 | `	pSet->nUsed++;` |
|  15346370 |   46 | `	return SXRET_OK;` |
|   7673208 |   47 |  |
|    413486 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    413488 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    413488 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    413488 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    413488 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    413488 |   60 | `	pSet->nSize = nItem;` |
|    413488 |   61 | `	return SXRET_OK;` |
|    206745 |   62 |  |
|    822306 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|    822308 |   65 | `	pSet->nUsed   = 0;` |
|    822308 |   66 | `	pSet->nCursor = 0;` |
|    822308 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     33684 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     33686 |   71 | `	pSet->nCursor = 0;` |
|     33686 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     36886 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     36888 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     13470 |   79 | `		pSet->nCursor = 0;` |
|     13470 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     23420 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     23420 |   83 | `	if( ppEntry ){` |
|     23420 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     11709 |   85 | `	}` |
|     23420 |   86 | `	pSet->nCursor++;` |
|     23420 |   87 | `	return SXRET_OK;` |
|     18445 |   88 |  |
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
|     52220 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     52222 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     52222 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   6633446 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   6633448 |  109 | `	sxi32 rc = SXRET_OK;` |
|   6633448 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3404894 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1702446 |  112 | `	}` |
|   6633448 |  113 | `	pSet->pBase = 0;` |
|   6633448 |  114 | `	pSet->nUsed = 0;` |
|   6633448 |  115 | `	pSet->nCursor = 0;` |
|   6633448 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3236040 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3236042 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3235952 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3235952 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1618022 |  126 |  |
|   2904426 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   2904428 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2120176 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    784254 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    784254 |  135 | `	pSet->nUsed--;` |
|    784254 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    784254 |  137 | `	return pData;` |
|   1452215 |  138 |  |
|   8221812 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   8221814 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   8221814 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   8221814 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4111111 |  148 |  |
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
|     74310 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     74312 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     74312 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     74312 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     74312 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     74312 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     74312 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     74312 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     74312 |  180 | `	pHash->nEntry = 0;` |
|     74312 |  181 | `	pHash->apBucket = apNew;` |
|     74312 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     74312 |  183 | `	return SXRET_OK;` |
|     37157 |  184 |  |
|      9512 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|      9514 |  193 | `	pEntry = pHash->pList;` |
|      5458 |  194 | `	for(;;){` |
|     10918 |  195 | `		if( pHash->nEntry == 0 ){` |
|      9514 |  196 | `			break;` |
|         - |  197 | `		}` |
|      1406 |  198 | `		pNext = pEntry->pNext;` |
|      1406 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      1406 |  200 | `		pEntry = pNext;` |
|      1406 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|      9514 |  203 | `	if( pHash->apBucket ){` |
|      9514 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      4756 |  205 | `	}` |
|      9514 |  206 | `	pHash->apBucket = 0;` |
|      9514 |  207 | `	pHash->nBucketSize = 0;` |
|      9514 |  208 | `	pHash->pAllocator = 0;` |
|      9514 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   7554480 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   7554482 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   7554482 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   6440420 |  218 | `	for(;;){` |
|  13038943 |  219 | `		if( pEntry == 0 ){` |
|   4087012 |  220 | `			break;` |
|         - |  221 | `		}` |
|  10685538 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   3467474 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   3467472 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   5484463 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   4087012 |  229 | `	return 0;` |
|   3777506 |  230 |  |
|   7596550 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   7596552 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     42078 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   7554476 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   7554476 |  244 | `	if( pEntry == 0 ){` |
|   4087012 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   3467466 |  247 | `	return (SyHashEntry *)pEntry;` |
|   3798541 |  248 |  |
|     61224 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     61226 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     45804 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     22903 |  254 | `	}else{` |
|     15424 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     61226 |  257 | `	if( pEntry->pNextCollide ){` |
|      3631 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      1815 |  259 | `	}` |
|     61226 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     61226 |  261 | `	pHash->nEntry--;` |
|     61226 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     61226 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     61226 |  268 | `	return rc;` |
|         2 |  269 |  |
|         6 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         1 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|         7 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|         7 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|         7 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|         7 |  284 | `	return rc;` |
|         4 |  285 |  |
|     61218 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     61220 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     61220 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     61220 |  296 | `	return rc;` |
|         2 |  297 |  |
|    109344 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    109346 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    109346 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    762690 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    762692 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    108912 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    108912 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    653782 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    653782 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    653782 |  324 | `	return (SyHashEntry *)pEntry;` |
|    381347 |  325 |  |
|        10 |  326 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         1 |  327 |  |
|         - |  328 | `	SyHashEntry_Pr *pEntry;` |
|         - |  329 | `	sxi32 rc;` |
|         - |  330 | `	sxu32 n;` |
|         - |  331 | `#if defined(UNTRUST)` |
|         - |  332 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  333 | `		return 0;` |
|         - |  334 | `	}` |
|         - |  335 | `#endif` |
|        11 |  336 | `	pEntry = pHash->pList;` |
|      1579 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1569 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1569 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1569 |  344 | `		pEntry = pEntry->pNext;` |
|       785 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     10380 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     10382 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     10382 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     10382 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     10382 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1420142 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1409762 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1409762 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1409762 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1409762 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    677008 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    338504 |  371 | `		}` |
|   1409762 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1409762 |  374 | `		pEntry = pEntry->pNext;` |
|    704882 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     10382 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     10382 |  378 | `	pHash->apBucket = apNew;` |
|     10382 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     10382 |  380 | `	return SXRET_OK;` |
|      5192 |  381 |  |
|   1285598 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1285600 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1285600 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1285600 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    855128 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    427554 |  389 | `	}` |
|   1285600 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1285600 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1285600 |  393 | `	if( pHash->nEntry == 0 ){` |
|     53250 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     26624 |  395 | `	}` |
|   1285600 |  396 | `	pHash->nEntry++;` |
|   1285600 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1285598 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1285600 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     10382 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     10382 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      5190 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1285600 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1285600 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1285600 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1285600 |  421 | `	pEntry->pHash = pHash;` |
|   1285600 |  422 | `	pEntry->pKey = pKey;` |
|   1285600 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1285600 |  424 | `	pEntry->pUserData = pUserData;` |
|   1285600 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1285600 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1285600 |  428 | `	return rc;` |
|    642801 |  429 |  |
|     74310 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     74312 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
