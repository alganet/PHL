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
|  13093516 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  13093518 |   16 | `	pSet->nSize = 0 ;` |
|  13093518 |   17 | `	pSet->nUsed = 0;` |
|  13093518 |   18 | `	pSet->nCursor = 0;` |
|  13093518 |   19 | `	pSet->eSize = ElemSize;` |
|  13093518 |   20 | `	pSet->pAllocator = pAllocator;` |
|  13093518 |   21 | `	pSet->pBase =  0;` |
|  13093518 |   22 | `	pSet->pUserData = 0;` |
|  13093518 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  21624536 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  21624538 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3735050 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3735050 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3628562 |   34 | `			pSet->nSize = 4;` |
|   1814280 |   35 | `		}` |
|   3735050 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3735050 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3735050 |   40 | `		pSet->pBase = pNew;` |
|   3735050 |   41 | `		pSet->nSize <<= 1;` |
|   1867524 |   42 | `	}` |
|  21624538 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 160615266 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  21624538 |   45 | `	pSet->nUsed++;` |
|  21624538 |   46 | `	return SXRET_OK;` |
|  10812292 |   47 |  |
|    732616 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    732618 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    732618 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    732618 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    732618 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    732618 |   60 | `	pSet->nSize = nItem;` |
|    732618 |   61 | `	return SXRET_OK;` |
|    366310 |   62 |  |
|   1208270 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1208272 |   65 | `	pSet->nUsed   = 0;` |
|   1208272 |   66 | `	pSet->nCursor = 0;` |
|   1208272 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     42428 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     42430 |   71 | `	pSet->nCursor = 0;` |
|     42430 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     46356 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     46358 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     17430 |   79 | `		pSet->nCursor = 0;` |
|     17430 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     28930 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     28930 |   83 | `	if( ppEntry ){` |
|     28930 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     14464 |   85 | `	}` |
|     28930 |   86 | `	pSet->nCursor++;` |
|     28930 |   87 | `	return SXRET_OK;` |
|     23180 |   88 |  |
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
|     90248 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     90250 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     90250 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   7910314 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   7910316 |  109 | `	sxi32 rc = SXRET_OK;` |
|   7910316 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4093412 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2046705 |  112 | `	}` |
|   7910316 |  113 | `	pSet->pBase = 0;` |
|   7910316 |  114 | `	pSet->nUsed = 0;` |
|   7910316 |  115 | `	pSet->nCursor = 0;` |
|   7910316 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4303474 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4303476 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4303386 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4303386 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2151739 |  126 |  |
|   3213700 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3213702 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2142662 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1071042 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1071042 |  135 | `	pSet->nUsed--;` |
|   1071042 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1071042 |  137 | `	return pData;` |
|   1606852 |  138 |  |
|  10135560 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  10135562 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  10135562 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  10135562 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5067997 |  148 |  |
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
|    154190 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    154192 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    154192 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    154192 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    154192 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    154192 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    154192 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    154192 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    154192 |  180 | `	pHash->nEntry = 0;` |
|    154192 |  181 | `	pHash->apBucket = apNew;` |
|    154192 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    154192 |  183 | `	return SXRET_OK;` |
|     77097 |  184 |  |
|     28936 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     28938 |  193 | `	pEntry = pHash->pList;` |
|     16171 |  194 | `	for(;;){` |
|     32344 |  195 | `		if( pHash->nEntry == 0 ){` |
|     28938 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3408 |  198 | `		pNext = pEntry->pNext;` |
|      3408 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3408 |  200 | `		pEntry = pNext;` |
|      3408 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     28938 |  203 | `	if( pHash->apBucket ){` |
|     28938 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     14468 |  205 | `	}` |
|     28938 |  206 | `	pHash->apBucket = 0;` |
|     28938 |  207 | `	pHash->nBucketSize = 0;` |
|     28938 |  208 | `	pHash->pAllocator = 0;` |
|     28938 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  10744966 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  10744968 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  10744968 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   9603654 |  218 | `	for(;;){` |
|  19193322 |  219 | `		if( pEntry == 0 ){` |
|   5925498 |  220 | `			break;` |
|         - |  221 | `		}` |
|  15677431 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   4819474 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   4819472 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   8448356 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   5925498 |  229 | `	return 0;` |
|   5372749 |  230 |  |
|  10836018 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  10836020 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     91064 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  10744958 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  10744958 |  244 | `	if( pEntry == 0 ){` |
|   5925498 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   4819462 |  247 | `	return (SyHashEntry *)pEntry;` |
|   5418275 |  248 |  |
|     83160 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     83162 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     63302 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     31652 |  254 | `	}else{` |
|     19862 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     83162 |  257 | `	if( pEntry->pNextCollide ){` |
|      4307 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2153 |  259 | `	}` |
|     83162 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     83162 |  261 | `	pHash->nEntry--;` |
|     83162 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     83162 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     83162 |  268 | `	return rc;` |
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
|     83150 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     83152 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     83152 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     83152 |  296 | `	return rc;` |
|         2 |  297 |  |
|    208940 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    208942 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    208942 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   1544778 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   1544780 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    208508 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    208508 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   1336274 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   1336274 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   1336274 |  324 | `	return (SyHashEntry *)pEntry;` |
|    772391 |  325 |  |
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
|      1633 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1623 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1623 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1623 |  344 | `		pEntry = pEntry->pNext;` |
|       812 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     19244 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     19246 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     19246 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     19246 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     19246 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2651182 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2631938 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2631938 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2631938 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2631938 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1263735 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    631863 |  371 | `		}` |
|   2631938 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2631938 |  374 | `		pEntry = pEntry->pNext;` |
|   1315970 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     19246 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     19246 |  378 | `	pHash->apBucket = apNew;` |
|     19246 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     19246 |  380 | `	return SXRET_OK;` |
|      9624 |  381 |  |
|   2413396 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2413398 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2413398 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2413398 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1612294 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    806151 |  389 | `	}` |
|   2413398 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2413398 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2413398 |  393 | `	if( pHash->nEntry == 0 ){` |
|     95914 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     47956 |  395 | `	}` |
|   2413398 |  396 | `	pHash->nEntry++;` |
|   2413398 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2413396 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2413398 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     19246 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     19246 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      9622 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2413398 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2413398 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2413398 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2413398 |  421 | `	pEntry->pHash = pHash;` |
|   2413398 |  422 | `	pEntry->pKey = pKey;` |
|   2413398 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2413398 |  424 | `	pEntry->pUserData = pUserData;` |
|   2413398 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2413398 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2413398 |  428 | `	return rc;` |
|   1206700 |  429 |  |
|    107872 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    107874 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
