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
|  13699218 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  13699220 |   16 | `	pSet->nSize = 0 ;` |
|  13699220 |   17 | `	pSet->nUsed = 0;` |
|  13699220 |   18 | `	pSet->nCursor = 0;` |
|  13699220 |   19 | `	pSet->eSize = ElemSize;` |
|  13699220 |   20 | `	pSet->pAllocator = pAllocator;` |
|  13699220 |   21 | `	pSet->pBase =  0;` |
|  13699220 |   22 | `	pSet->pUserData = 0;` |
|  13699220 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  22776512 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  22776514 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3795474 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3795474 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3686540 |   34 | `			pSet->nSize = 4;` |
|   1843269 |   35 | `		}` |
|   3795474 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3795474 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3795474 |   40 | `		pSet->pBase = pNew;` |
|   3795474 |   41 | `		pSet->nSize <<= 1;` |
|   1897736 |   42 | `	}` |
|  22776514 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 169510618 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  22776514 |   45 | `	pSet->nUsed++;` |
|  22776514 |   46 | `	return SXRET_OK;` |
|  11388280 |   47 |  |
|    809072 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    809074 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    809074 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    809074 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    809074 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    809074 |   60 | `	pSet->nSize = nItem;` |
|    809074 |   61 | `	return SXRET_OK;` |
|    404538 |   62 |  |
|   1258462 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1258464 |   65 | `	pSet->nUsed   = 0;` |
|   1258464 |   66 | `	pSet->nCursor = 0;` |
|   1258464 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     42604 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     42606 |   71 | `	pSet->nCursor = 0;` |
|     42606 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     46532 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     46534 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     17518 |   79 | `		pSet->nCursor = 0;` |
|     17518 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     29018 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     29018 |   83 | `	if( ppEntry ){` |
|     29018 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     14508 |   85 | `	}` |
|     29018 |   86 | `	pSet->nCursor++;` |
|     29018 |   87 | `	return SXRET_OK;` |
|     23268 |   88 |  |
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
|    119456 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    119458 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|    119458 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8139592 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8139594 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8139594 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4180596 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2090297 |  112 | `	}` |
|   8139594 |  113 | `	pSet->pBase = 0;` |
|   8139594 |  114 | `	pSet->nUsed = 0;` |
|   8139594 |  115 | `	pSet->nCursor = 0;` |
|   8139594 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4483678 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4483680 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4483590 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4483590 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2241841 |  126 |  |
|   3221708 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3221710 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2143770 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1077942 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1077942 |  135 | `	pSet->nUsed--;` |
|   1077942 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1077942 |  137 | `	return pData;` |
|   1610856 |  138 |  |
|  10168647 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  10168649 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  10168649 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  10168649 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5084532 |  148 |  |
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
|    173968 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    173970 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    173970 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    173970 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    173970 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    173970 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    173970 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    173970 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    173970 |  180 | `	pHash->nEntry = 0;` |
|    173970 |  181 | `	pHash->apBucket = apNew;` |
|    173970 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    173970 |  183 | `	return SXRET_OK;` |
|     86986 |  184 |  |
|     29400 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     29402 |  193 | `	pEntry = pHash->pList;` |
|     16421 |  194 | `	for(;;){` |
|     32844 |  195 | `		if( pHash->nEntry == 0 ){` |
|     29402 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3444 |  198 | `		pNext = pEntry->pNext;` |
|      3444 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3444 |  200 | `		pEntry = pNext;` |
|      3444 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     29402 |  203 | `	if( pHash->apBucket ){` |
|     29402 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     14700 |  205 | `	}` |
|     29402 |  206 | `	pHash->apBucket = 0;` |
|     29402 |  207 | `	pHash->nBucketSize = 0;` |
|     29402 |  208 | `	pHash->pAllocator = 0;` |
|     29402 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  11129976 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  11129978 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  11129978 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   9832221 |  218 | `	for(;;){` |
|  19581928 |  219 | `		if( pEntry == 0 ){` |
|   6147744 |  220 | `			break;` |
|         - |  221 | `		}` |
|  15925173 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   4982238 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   4982236 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   8451952 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   6147744 |  229 | `	return 0;` |
|   5565254 |  230 |  |
|  11234266 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  11234268 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    104302 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  11129968 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  11129968 |  244 | `	if( pEntry == 0 ){` |
|   6147744 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   4982226 |  247 | `	return (SyHashEntry *)pEntry;` |
|   5617399 |  248 |  |
|     83958 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     83960 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     64012 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     32007 |  254 | `	}else{` |
|     19950 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     83960 |  257 | `	if( pEntry->pNextCollide ){` |
|      4307 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2153 |  259 | `	}` |
|     83960 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     83960 |  261 | `	pHash->nEntry--;` |
|     83960 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     83960 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     83960 |  268 | `	return rc;` |
|         2 |  269 |  |
|        10 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|        12 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        12 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|        12 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        12 |  284 | `	return rc;` |
|         7 |  285 |  |
|     83948 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     83950 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     83950 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     83950 |  296 | `	return rc;` |
|         2 |  297 |  |
|    244546 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    244548 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    244548 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   1864306 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   1864308 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    244114 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    244114 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   1620196 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   1620196 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   1620196 |  324 | `	return (SyHashEntry *)pEntry;` |
|    932155 |  325 |  |
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
|      1673 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1663 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1663 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1663 |  344 | `		pEntry = pEntry->pNext;` |
|       832 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     19812 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     19814 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     19814 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     19814 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     19814 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2727110 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2707298 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2707298 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2707298 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2707298 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1292385 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    646188 |  371 | `		}` |
|   2707298 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2707298 |  374 | `		pEntry = pEntry->pNext;` |
|   1353650 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     19814 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     19814 |  378 | `	pHash->apBucket = apNew;` |
|     19814 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     19814 |  380 | `	return SXRET_OK;` |
|      9908 |  381 |  |
|   2641492 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2641494 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2641494 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2641494 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1737829 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    868894 |  389 | `	}` |
|   2641494 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2641494 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2641494 |  393 | `	if( pHash->nEntry == 0 ){` |
|    109314 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     54656 |  395 | `	}` |
|   2641494 |  396 | `	pHash->nEntry++;` |
|   2641494 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2641492 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2641494 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     19814 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     19814 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      9906 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2641494 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2641494 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2641494 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2641494 |  421 | `	pEntry->pHash = pHash;` |
|   2641494 |  422 | `	pEntry->pKey = pKey;` |
|   2641494 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2641494 |  424 | `	pEntry->pUserData = pUserData;` |
|   2641494 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2641494 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2641494 |  428 | `	return rc;` |
|   1320748 |  429 |  |
|    109392 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    109394 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
