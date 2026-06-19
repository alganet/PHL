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
|  17644254 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  17644256 |   16 | `	pSet->nSize = 0 ;` |
|  17644256 |   17 | `	pSet->nUsed = 0;` |
|  17644256 |   18 | `	pSet->nCursor = 0;` |
|  17644256 |   19 | `	pSet->eSize = ElemSize;` |
|  17644256 |   20 | `	pSet->pAllocator = pAllocator;` |
|  17644256 |   21 | `	pSet->pBase =  0;` |
|  17644256 |   22 | `	pSet->pUserData = 0;` |
|  17644256 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  28955278 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  28955280 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4322104 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4322104 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4181080 |   34 | `			pSet->nSize = 4;` |
|   2090539 |   35 | `		}` |
|   4322104 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4322104 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4322104 |   40 | `		pSet->pBase = pNew;` |
|   4322104 |   41 | `		pSet->nSize <<= 1;` |
|   2161051 |   42 | `	}` |
|  28955280 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 216788198 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  28955280 |   45 | `	pSet->nUsed++;` |
|  28955280 |   46 | `	return SXRET_OK;` |
|  14477663 |   47 |  |
|   1162680 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|   1162682 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1162682 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1162682 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1162682 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1162682 |   60 | `	pSet->nSize = nItem;` |
|   1162682 |   61 | `	return SXRET_OK;` |
|    581342 |   62 |  |
|   1643676 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1643678 |   65 | `	pSet->nUsed   = 0;` |
|   1643678 |   66 | `	pSet->nCursor = 0;` |
|   1643678 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     53964 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     53966 |   71 | `	pSet->nCursor = 0;` |
|     53966 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     58142 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     58144 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     22240 |   79 | `		pSet->nCursor = 0;` |
|     22240 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     35906 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     35906 |   83 | `	if( ppEntry ){` |
|     35906 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     17952 |   85 | `	}` |
|     35906 |   86 | `	pSet->nCursor++;` |
|     35906 |   87 | `	return SXRET_OK;` |
|     29073 |   88 |  |
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
|    196520 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    196522 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       116 |  103 | `		pSet->nUsed = nNewSize;` |
|        57 |  104 | `	}` |
|    196522 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   9452326 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   9452328 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9452328 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4734408 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2367203 |  112 | `	}` |
|   9452328 |  113 | `	pSet->pBase = 0;` |
|   9452328 |  114 | `	pSet->nUsed = 0;` |
|   9452328 |  115 | `	pSet->nCursor = 0;` |
|   9452328 |  116 | `	return rc;` |
|         2 |  117 |  |
|   5335126 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5335128 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       112 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5335018 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5335018 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2667565 |  126 |  |
|   3451732 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3451734 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2151960 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1299776 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1299776 |  135 | `	pSet->nUsed--;` |
|   1299776 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1299776 |  137 | `	return pData;` |
|   1725868 |  138 |  |
|  12591947 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  12591949 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  12591949 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  12591949 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6296115 |  148 |  |
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
|    504782 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    504784 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    504784 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    504784 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    504784 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    504784 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    504784 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    504784 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    504784 |  180 | `	pHash->nEntry = 0;` |
|    504784 |  181 | `	pHash->apBucket = apNew;` |
|    504784 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    504784 |  183 | `	return SXRET_OK;` |
|    252393 |  184 |  |
|     92916 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     92918 |  193 | `	pEntry = pHash->pList;` |
|     49587 |  194 | `	for(;;){` |
|     99176 |  195 | `		if( pHash->nEntry == 0 ){` |
|     92918 |  196 | `			break;` |
|         - |  197 | `		}` |
|      6260 |  198 | `		pNext = pEntry->pNext;` |
|      6260 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      6260 |  200 | `		pEntry = pNext;` |
|      6260 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     92918 |  203 | `	if( pHash->apBucket ){` |
|     92918 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     46458 |  205 | `	}` |
|     92918 |  206 | `	pHash->apBucket = 0;` |
|     92918 |  207 | `	pHash->nBucketSize = 0;` |
|     92918 |  208 | `	pHash->pAllocator = 0;` |
|     92918 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  15849350 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  15849352 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  15849352 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  14097383 |  218 | `	for(;;){` |
|  28346627 |  219 | `		if( pEntry == 0 ){` |
|   8343638 |  220 | `			break;` |
|         - |  221 | `		}` |
|  23755718 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   7505718 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   7505716 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  12497277 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   8343638 |  229 | `	return 0;` |
|   7924941 |  230 |  |
|  16599610 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  16599612 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    750416 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  15849198 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  15849198 |  244 | `	if( pEntry == 0 ){` |
|   8343638 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   7505562 |  247 | `	return (SyHashEntry *)pEntry;` |
|   8300071 |  248 |  |
|    111348 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    111350 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     85224 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     42613 |  254 | `	}else{` |
|     26128 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    111350 |  257 | `	if( pEntry->pNextCollide ){` |
|      4947 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2473 |  259 | `	}` |
|    111350 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    111350 |  261 | `	pHash->nEntry--;` |
|    111350 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    111350 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    111350 |  268 | `	return rc;` |
|         2 |  269 |  |
|       154 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       156 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       156 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       156 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       156 |  284 | `	return rc;` |
|        79 |  285 |  |
|    111194 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|    111196 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    111196 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    111196 |  296 | `	return rc;` |
|         2 |  297 |  |
|   1027284 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|   1027286 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   1027286 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   6399004 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   6399006 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1026850 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   1026850 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   5372158 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   5372158 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   5372158 |  324 | `	return (SyHashEntry *)pEntry;` |
|   3199504 |  325 |  |
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
|      1899 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1889 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1889 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1889 |  344 | `		pEntry = pEntry->pNext;` |
|       945 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     26200 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     26202 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     26202 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     26202 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     26202 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3329466 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3303266 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3303266 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3303266 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3303266 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1573467 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    786838 |  371 | `		}` |
|   3303266 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3303266 |  374 | `		pEntry = pEntry->pNext;` |
|   1651634 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     26202 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     26202 |  378 | `	pHash->apBucket = apNew;` |
|     26202 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     26202 |  380 | `	return SXRET_OK;` |
|     13102 |  381 |  |
|   4273280 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   4273282 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   4273282 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4273282 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2386394 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1193203 |  389 | `	}` |
|   4273282 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   4273282 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   4273282 |  393 | `	if( pHash->nEntry == 0 ){` |
|    272838 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    136418 |  395 | `	}` |
|   4273282 |  396 | `	pHash->nEntry++;` |
|   4273282 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   4273280 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   4273282 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     26202 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     26202 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     13100 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   4273282 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4273282 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   4273282 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4273282 |  421 | `	pEntry->pHash = pHash;` |
|   4273282 |  422 | `	pEntry->pKey = pKey;` |
|   4273282 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   4273282 |  424 | `	pEntry->pUserData = pUserData;` |
|   4273282 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4273282 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   4273282 |  428 | `	return rc;` |
|   2136642 |  429 |  |
|    140758 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    140760 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
