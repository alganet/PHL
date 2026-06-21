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
|  17734730 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 |  |
|  17734735 |   16 | `	pSet->nSize = 0 ;` |
|  17734735 |   17 | `	pSet->nUsed = 0;` |
|  17734735 |   18 | `	pSet->nCursor = 0;` |
|  17734735 |   19 | `	pSet->eSize = ElemSize;` |
|  17734735 |   20 | `	pSet->pAllocator = pAllocator;` |
|  17734735 |   21 | `	pSet->pBase =  0;` |
|  17734735 |   22 | `	pSet->pUserData = 0;` |
|  17734735 |   23 | `	return SXRET_OK;` |
|         5 |   24 |  |
|  29066892 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  29066897 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4340873 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4340873 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4199389 |   34 | `			pSet->nSize = 4;` |
|   2099692 |   35 | `		}` |
|   4340873 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4340873 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4340873 |   40 | `		pSet->pBase = pNew;` |
|   4340873 |   41 | `		pSet->nSize <<= 1;` |
|   2170434 |   42 | `	}` |
|  29066897 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 217468563 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  29066897 |   45 | `	pSet->nUsed++;` |
|  29066897 |   46 | `	return SXRET_OK;` |
|  14533473 |   47 |  |
|   1167062 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 |  |
|   1167067 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1167067 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1167067 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1167067 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1167067 |   60 | `	pSet->nSize = nItem;` |
|   1167067 |   61 | `	return SXRET_OK;` |
|    583536 |   62 |  |
|   1656638 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 |  |
|   1656643 |   65 | `	pSet->nUsed   = 0;` |
|   1656643 |   66 | `	pSet->nCursor = 0;` |
|   1656643 |   67 | `	return SXRET_OK;` |
|         5 |   68 |  |
|     54718 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 |  |
|     54723 |   71 | `	pSet->nCursor = 0;` |
|     54723 |   72 | `	return SXRET_OK;` |
|         5 |   73 |  |
|     58896 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     58901 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     22547 |   79 | `		pSet->nCursor = 0;` |
|     22547 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     36359 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     36359 |   83 | `	if( ppEntry ){` |
|     36359 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     18177 |   85 | `	}` |
|     36359 |   86 | `	pSet->nCursor++;` |
|     36359 |   87 | `	return SXRET_OK;` |
|     29453 |   88 |  |
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
|    197148 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 |  |
|    197153 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       118 |  103 | `		pSet->nUsed = nNewSize;` |
|        57 |  104 | `	}` |
|    197153 |  105 | `	return SXRET_OK;` |
|         5 |  106 |  |
|   9493530 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 |  |
|   9493535 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9493535 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4754577 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2377286 |  112 | `	}` |
|   9493535 |  113 | `	pSet->pBase = 0;` |
|   9493535 |  114 | `	pSet->nUsed = 0;` |
|   9493535 |  115 | `	pSet->nCursor = 0;` |
|   9493535 |  116 | `	return rc;` |
|         5 |  117 |  |
|   5347100 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5347105 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       115 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5346995 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5346995 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2673555 |  126 |  |
|   3467078 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3467083 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2152207 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1314881 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1314881 |  135 | `	pSet->nUsed--;` |
|   1314881 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1314881 |  137 | `	return pData;` |
|   1733544 |  138 |  |
|  12693986 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  12693991 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  12693991 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  12693991 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6347112 |  148 |  |
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
|    507736 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    507741 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    507741 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    507741 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    507741 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    507741 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    507741 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    507741 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    507741 |  180 | `	pHash->nEntry = 0;` |
|    507741 |  181 | `	pHash->apBucket = apNew;` |
|    507741 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    507741 |  183 | `	return SXRET_OK;` |
|    253873 |  184 |  |
|     94664 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     94669 |  193 | `	pEntry = pHash->pList;` |
|     50542 |  194 | `	for(;;){` |
|    101089 |  195 | `		if( pHash->nEntry == 0 ){` |
|     94669 |  196 | `			break;` |
|         - |  197 | `		}` |
|      6425 |  198 | `		pNext = pEntry->pNext;` |
|      6425 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      6425 |  200 | `		pEntry = pNext;` |
|      6425 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|     94669 |  203 | `	if( pHash->apBucket ){` |
|     94669 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     47332 |  205 | `	}` |
|     94669 |  206 | `	pHash->apBucket = 0;` |
|     94669 |  207 | `	pHash->nBucketSize = 0;` |
|     94669 |  208 | `	pHash->pAllocator = 0;` |
|     94669 |  209 | `	return SXRET_OK;` |
|         5 |  210 |  |
|  15957084 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  15957089 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  15957089 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  14170439 |  218 | `	for(;;){` |
|  28285984 |  219 | `		if( pEntry == 0 ){` |
|   8400949 |  220 | `			break;` |
|         - |  221 | `		}` |
|  23662980 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   7556144 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   7556145 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  12328900 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   8400949 |  229 | `	return 0;` |
|   7978811 |  230 |  |
|  16702300 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  16702305 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    745387 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  15956923 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  15956923 |  244 | `	if( pEntry == 0 ){` |
|   8400949 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   7555979 |  247 | `	return (SyHashEntry *)pEntry;` |
|   8351419 |  248 |  |
|    112798 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    112803 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     86413 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     43209 |  254 | `	}else{` |
|     26395 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    112803 |  257 | `	if( pEntry->pNextCollide ){` |
|      4945 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2472 |  259 | `	}` |
|    112803 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    112803 |  261 | `	pHash->nEntry--;` |
|    112803 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    112803 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    112803 |  268 | `	return rc;` |
|         5 |  269 |  |
|       166 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       171 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       171 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       171 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       171 |  284 | `	return rc;` |
|        88 |  285 |  |
|    112632 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  287 |  |
|    112637 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    112637 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    112637 |  296 | `	return rc;` |
|         5 |  297 |  |
|   1032500 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|   1032505 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   1032505 |  306 | `	return SXRET_OK;` |
|         5 |  307 |  |
|   6464598 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   6464603 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1032069 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   1032069 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   5432539 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   5432539 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   5432539 |  324 | `	return (SyHashEntry *)pEntry;` |
|   3232304 |  325 |  |
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
|      1909 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1899 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1899 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1899 |  344 | `		pEntry = pEntry->pNext;` |
|       950 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     26274 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  349 |  |
|     26279 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     26279 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     26279 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     26279 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3339143 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3312869 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3312869 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3312869 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3312869 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1577850 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    788885 |  371 | `		}` |
|   3312869 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3312869 |  374 | `		pEntry = pEntry->pNext;` |
|   1656437 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     26279 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     26279 |  378 | `	pHash->apBucket = apNew;` |
|     26279 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     26279 |  380 | `	return SXRET_OK;` |
|     13142 |  381 |  |
|   4295302 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         5 |  383 |  |
|   4295307 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   4295307 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4295307 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2399310 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1199697 |  389 | `	}` |
|   4295307 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   4295307 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   4295307 |  393 | `	if( pHash->nEntry == 0 ){` |
|    273959 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    136977 |  395 | `	}` |
|   4295307 |  396 | `	pHash->nEntry++;` |
|   4295307 |  397 | `	return SXRET_OK;` |
|         5 |  398 |  |
|   4295302 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   4295307 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     26279 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     26279 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     13137 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   4295307 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4295307 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   4295307 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4295307 |  421 | `	pEntry->pHash = pHash;` |
|   4295307 |  422 | `	pEntry->pKey = pKey;` |
|   4295307 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   4295307 |  424 | `	pEntry->pUserData = pUserData;` |
|   4295307 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4295307 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   4295307 |  428 | `	return rc;` |
|   2147656 |  429 |  |
|    142338 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    142343 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  439 |  |
|         - |  440 |  |
