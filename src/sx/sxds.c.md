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
|  18367030 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 |  |
|  18367035 |   16 | `	pSet->nSize = 0 ;` |
|  18367035 |   17 | `	pSet->nUsed = 0;` |
|  18367035 |   18 | `	pSet->nCursor = 0;` |
|  18367035 |   19 | `	pSet->eSize = ElemSize;` |
|  18367035 |   20 | `	pSet->pAllocator = pAllocator;` |
|  18367035 |   21 | `	pSet->pBase =  0;` |
|  18367035 |   22 | `	pSet->pUserData = 0;` |
|  18367035 |   23 | `	return SXRET_OK;` |
|         5 |   24 |  |
|  30121547 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  30121552 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4446683 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4446683 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4298629 |   34 | `			pSet->nSize = 4;` |
|   2149312 |   35 | `		}` |
|   4446683 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4446683 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4446683 |   40 | `		pSet->pBase = pNew;` |
|   4446683 |   41 | `		pSet->nSize <<= 1;` |
|   2223339 |   42 | `	}` |
|  30121552 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 225352484 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  30121552 |   45 | `	pSet->nUsed++;` |
|  30121552 |   46 | `	return SXRET_OK;` |
|  15060821 |   47 |  |
|   1221926 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 |  |
|   1221931 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1221931 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1221931 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1221931 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1221931 |   60 | `	pSet->nSize = nItem;` |
|   1221931 |   61 | `	return SXRET_OK;` |
|    610968 |   62 |  |
|   1712839 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 |  |
|   1712844 |   65 | `	pSet->nUsed   = 0;` |
|   1712844 |   66 | `	pSet->nCursor = 0;` |
|   1712844 |   67 | `	return SXRET_OK;` |
|         5 |   68 |  |
|     55664 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 |  |
|     55669 |   71 | `	pSet->nCursor = 0;` |
|     55669 |   72 | `	return SXRET_OK;` |
|         5 |   73 |  |
|     59848 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     59853 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     22981 |   79 | `		pSet->nCursor = 0;` |
|     22981 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     36877 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     36877 |   83 | `	if( ppEntry ){` |
|     36877 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     18436 |   85 | `	}` |
|     36877 |   86 | `	pSet->nCursor++;` |
|     36877 |   87 | `	return SXRET_OK;` |
|     29929 |   88 |  |
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
|    206862 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 |  |
|    206867 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       118 |  103 | `		pSet->nUsed = nNewSize;` |
|        57 |  104 | `	}` |
|    206867 |  105 | `	return SXRET_OK;` |
|         5 |  106 |  |
|   9701484 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 |  |
|   9701489 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9701489 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4855367 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2427681 |  112 | `	}` |
|   9701489 |  113 | `	pSet->pBase = 0;` |
|   9701489 |  114 | `	pSet->nUsed = 0;` |
|   9701489 |  115 | `	pSet->nCursor = 0;` |
|   9701489 |  116 | `	return rc;` |
|         5 |  117 |  |
|   5523450 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5523455 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       121 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5523339 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5523339 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2761730 |  126 |  |
|   3530916 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3530921 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2175271 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1355655 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1355655 |  135 | `	pSet->nUsed--;` |
|   1355655 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1355655 |  137 | `	return pData;` |
|   1765463 |  138 |  |
|  13007789 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  13007794 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13007794 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13007794 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6504161 |  148 |  |
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
|    531108 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    531113 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    531113 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    531113 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    531113 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    531113 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    531113 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    531113 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    531113 |  180 | `	pHash->nEntry = 0;` |
|    531113 |  181 | `	pHash->apBucket = apNew;` |
|    531113 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    531113 |  183 | `	return SXRET_OK;` |
|    265559 |  184 |  |
|     97466 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     97471 |  193 | `	pEntry = pHash->pList;` |
|     52059 |  194 | `	for(;;){` |
|    104123 |  195 | `		if( pHash->nEntry == 0 ){` |
|     97471 |  196 | `			break;` |
|         - |  197 | `		}` |
|      6657 |  198 | `		pNext = pEntry->pNext;` |
|      6657 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      6657 |  200 | `		pEntry = pNext;` |
|      6657 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|     97471 |  203 | `	if( pHash->apBucket ){` |
|     97471 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     48733 |  205 | `	}` |
|     97471 |  206 | `	pHash->apBucket = 0;` |
|     97471 |  207 | `	pHash->nBucketSize = 0;` |
|     97471 |  208 | `	pHash->pAllocator = 0;` |
|     97471 |  209 | `	return SXRET_OK;` |
|         5 |  210 |  |
|  16611628 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  16611633 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  16611633 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  14725001 |  218 | `	for(;;){` |
|  29440782 |  219 | `		if( pEntry == 0 ){` |
|   8763789 |  220 | `			break;` |
|         - |  221 | `		}` |
|  24600667 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   7847848 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   7847849 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  12829154 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   8763789 |  229 | `	return 0;` |
|   8306329 |  230 |  |
|  17394160 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  17394165 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    782731 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  16611439 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  16611439 |  244 | `	if( pEntry == 0 ){` |
|   8763789 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   7847655 |  247 | `	return (SyHashEntry *)pEntry;` |
|   8697595 |  248 |  |
|    117550 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    117555 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     90313 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     45159 |  254 | `	}else{` |
|     27247 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    117555 |  257 | `	if( pEntry->pNextCollide ){` |
|      4981 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2490 |  259 | `	}` |
|    117555 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    117555 |  261 | `	pHash->nEntry--;` |
|    117555 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    117555 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    117555 |  268 | `	return rc;` |
|         5 |  269 |  |
|       194 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       199 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       199 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       199 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       199 |  284 | `	return rc;` |
|       102 |  285 |  |
|    117356 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  287 |  |
|    117361 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    117361 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    117361 |  296 | `	return rc;` |
|         5 |  297 |  |
|   1079290 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|   1079295 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   1079295 |  306 | `	return SXRET_OK;` |
|         5 |  307 |  |
|   6757020 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   6757025 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1078845 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   1078845 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   5678185 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   5678185 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   5678185 |  324 | `	return (SyHashEntry *)pEntry;` |
|   3378515 |  325 |  |
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
|      1933 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1923 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1923 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1923 |  344 | `		pEntry = pEntry->pNext;` |
|       962 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     27504 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  349 |  |
|     27509 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     27509 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     27509 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     27509 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3493397 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3465893 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3465893 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3465893 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3465893 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1665723 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    832871 |  371 | `		}` |
|   3465893 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3465893 |  374 | `		pEntry = pEntry->pNext;` |
|   1732949 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     27509 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     27509 |  378 | `	pHash->apBucket = apNew;` |
|     27509 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     27509 |  380 | `	return SXRET_OK;` |
|     13757 |  381 |  |
|   4535300 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         5 |  383 |  |
|   4535305 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   4535305 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4535305 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2529040 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1264487 |  389 | `	}` |
|   4535305 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   4535305 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   4535305 |  393 | `	if( pHash->nEntry == 0 ){` |
|    289853 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    144924 |  395 | `	}` |
|   4535305 |  396 | `	pHash->nEntry++;` |
|   4535305 |  397 | `	return SXRET_OK;` |
|         5 |  398 |  |
|   4535300 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   4535305 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     27509 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     27509 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     13752 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   4535305 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4535305 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   4535305 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4535305 |  421 | `	pEntry->pHash = pHash;` |
|   4535305 |  422 | `	pEntry->pKey = pKey;` |
|   4535305 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   4535305 |  424 | `	pEntry->pUserData = pUserData;` |
|   4535305 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4535305 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   4535305 |  428 | `	return rc;` |
|   2267655 |  429 |  |
|    151390 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    151395 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  439 |  |
|         - |  440 |  |
