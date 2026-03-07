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
|  10337762 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  10337764 |   16 | `	pSet->nSize = 0 ;` |
|  10337764 |   17 | `	pSet->nUsed = 0;` |
|  10337764 |   18 | `	pSet->nCursor = 0;` |
|  10337764 |   19 | `	pSet->eSize = ElemSize;` |
|  10337764 |   20 | `	pSet->pAllocator = pAllocator;` |
|  10337764 |   21 | `	pSet->pBase =  0;` |
|  10337764 |   22 | `	pSet->pUserData = 0;` |
|  10337764 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  16362140 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  16362142 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3329948 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3329948 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3264566 |   34 | `			pSet->nSize = 4;` |
|   1632282 |   35 | `		}` |
|   3329948 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3329948 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3329948 |   40 | `		pSet->pBase = pNew;` |
|   3329948 |   41 | `		pSet->nSize <<= 1;` |
|   1664973 |   42 | `	}` |
|  16362142 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 123068370 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  16362142 |   45 | `	pSet->nUsed++;` |
|  16362142 |   46 | `	return SXRET_OK;` |
|   8181094 |   47 |  |
|    459252 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    459254 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    459254 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    459254 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    459254 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    459254 |   60 | `	pSet->nSize = nItem;` |
|    459254 |   61 | `	return SXRET_OK;` |
|    229628 |   62 |  |
|    896264 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|    896266 |   65 | `	pSet->nUsed   = 0;` |
|    896266 |   66 | `	pSet->nCursor = 0;` |
|    896266 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     36118 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     36120 |   71 | `	pSet->nCursor = 0;` |
|     36120 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     39640 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     39642 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     14518 |   79 | `		pSet->nCursor = 0;` |
|     14518 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     25126 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     25126 |   83 | `	if( ppEntry ){` |
|     25126 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     12562 |   85 | `	}` |
|     25126 |   86 | `	pSet->nCursor++;` |
|     25126 |   87 | `	return SXRET_OK;` |
|     19822 |   88 |  |
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
|     57796 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     57798 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     57798 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   6906776 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   6906778 |  109 | `	sxi32 rc = SXRET_OK;` |
|   6906778 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3553016 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1776507 |  112 | `	}` |
|   6906778 |  113 | `	pSet->pBase = 0;` |
|   6906778 |  114 | `	pSet->nUsed = 0;` |
|   6906778 |  115 | `	pSet->nCursor = 0;` |
|   6906778 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3379354 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3379356 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3379266 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3379266 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1689679 |  126 |  |
|   2994420 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   2994422 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2124028 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    870396 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    870396 |  135 | `	pSet->nUsed--;` |
|    870396 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    870396 |  137 | `	return pData;` |
|   1497212 |  138 |  |
|   8795847 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   8795849 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   8795849 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   8795849 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4398180 |  148 |  |
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
|     82032 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     82034 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     82034 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     82034 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     82034 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     82034 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     82034 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     82034 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     82034 |  180 | `	pHash->nEntry = 0;` |
|     82034 |  181 | `	pHash->apBucket = apNew;` |
|     82034 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     82034 |  183 | `	return SXRET_OK;` |
|     41018 |  184 |  |
|     10346 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     10348 |  193 | `	pEntry = pHash->pList;` |
|      6129 |  194 | `	for(;;){` |
|     12260 |  195 | `		if( pHash->nEntry == 0 ){` |
|     10348 |  196 | `			break;` |
|         - |  197 | `		}` |
|      1914 |  198 | `		pNext = pEntry->pNext;` |
|      1914 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      1914 |  200 | `		pEntry = pNext;` |
|      1914 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     10348 |  203 | `	if( pHash->apBucket ){` |
|     10348 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      5173 |  205 | `	}` |
|     10348 |  206 | `	pHash->apBucket = 0;` |
|     10348 |  207 | `	pHash->nBucketSize = 0;` |
|     10348 |  208 | `	pHash->pAllocator = 0;` |
|     10348 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   8300246 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   8300248 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   8300248 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   7165176 |  218 | `	for(;;){` |
|  14422684 |  219 | `		if( pEntry == 0 ){` |
|   4504480 |  220 | `			break;` |
|         - |  221 | `		}` |
|  11815960 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   3795772 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   3795770 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   6122438 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   4504480 |  229 | `	return 0;` |
|   4150389 |  230 |  |
|   8346520 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   8346522 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     46282 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   8300242 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   8300242 |  244 | `	if( pEntry == 0 ){` |
|   4504480 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   3795764 |  247 | `	return (SyHashEntry *)pEntry;` |
|   4173526 |  248 |  |
|     66196 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     66198 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     49590 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     24796 |  254 | `	}else{` |
|     16610 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     66198 |  257 | `	if( pEntry->pNextCollide ){` |
|      3977 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      1988 |  259 | `	}` |
|     66198 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     66198 |  261 | `	pHash->nEntry--;` |
|     66198 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     66198 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     66198 |  268 | `	return rc;` |
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
|     66190 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     66192 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     66192 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     66192 |  296 | `	return rc;` |
|         2 |  297 |  |
|    119096 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    119098 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    119098 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    828504 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    828506 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    118664 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    118664 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    709844 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    709844 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    709844 |  324 | `	return (SyHashEntry *)pEntry;` |
|    414254 |  325 |  |
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
|     11692 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     11694 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     11694 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     11694 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     11694 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1602510 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1590818 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1590818 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1590818 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1590818 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    763928 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    381964 |  371 | `		}` |
|   1590818 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1590818 |  374 | `		pEntry = pEntry->pNext;` |
|    795410 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     11694 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     11694 |  378 | `	pHash->apBucket = apNew;` |
|     11694 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     11694 |  380 | `	return SXRET_OK;` |
|      5848 |  381 |  |
|   1450550 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1450552 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1450552 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1450552 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    971500 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    485728 |  389 | `	}` |
|   1450552 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1450552 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1450552 |  393 | `	if( pHash->nEntry == 0 ){` |
|     58844 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     29421 |  395 | `	}` |
|   1450552 |  396 | `	pHash->nEntry++;` |
|   1450552 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1450550 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1450552 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     11694 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     11694 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      5846 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1450552 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1450552 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1450552 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1450552 |  421 | `	pEntry->pHash = pHash;` |
|   1450552 |  422 | `	pEntry->pKey = pKey;` |
|   1450552 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1450552 |  424 | `	pEntry->pUserData = pUserData;` |
|   1450552 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1450552 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1450552 |  428 | `	return rc;` |
|    725277 |  429 |  |
|     80926 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     80928 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
