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
|  15047010 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  15047012 |   16 | `	pSet->nSize = 0 ;` |
|  15047012 |   17 | `	pSet->nUsed = 0;` |
|  15047012 |   18 | `	pSet->nCursor = 0;` |
|  15047012 |   19 | `	pSet->eSize = ElemSize;` |
|  15047012 |   20 | `	pSet->pAllocator = pAllocator;` |
|  15047012 |   21 | `	pSet->pBase =  0;` |
|  15047012 |   22 | `	pSet->pUserData = 0;` |
|  15047012 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  24433802 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  24433804 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3959230 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3959230 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3844962 |   34 | `			pSet->nSize = 4;` |
|   1922480 |   35 | `		}` |
|   3959230 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3959230 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3959230 |   40 | `		pSet->pBase = pNew;` |
|   3959230 |   41 | `		pSet->nSize <<= 1;` |
|   1979614 |   42 | `	}` |
|  24433804 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 182357624 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  24433804 |   45 | `	pSet->nUsed++;` |
|  24433804 |   46 | `	return SXRET_OK;` |
|  12216925 |   47 |  |
|    900228 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    900230 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    900230 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    900230 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    900230 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    900230 |   60 | `	pSet->nSize = nItem;` |
|    900230 |   61 | `	return SXRET_OK;` |
|    450116 |   62 |  |
|   1375606 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1375608 |   65 | `	pSet->nUsed   = 0;` |
|   1375608 |   66 | `	pSet->nCursor = 0;` |
|   1375608 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     47588 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     47590 |   71 | `	pSet->nCursor = 0;` |
|     47590 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     51670 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     51672 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     19554 |   79 | `		pSet->nCursor = 0;` |
|     19554 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     32120 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     32120 |   83 | `	if( ppEntry ){` |
|     32120 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     16059 |   85 | `	}` |
|     32120 |   86 | `	pSet->nCursor++;` |
|     32120 |   87 | `	return SXRET_OK;` |
|     25837 |   88 |  |
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
|    149716 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    149718 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|    149718 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8617548 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8617550 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8617550 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4379924 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2189961 |  112 | `	}` |
|   8617550 |  113 | `	pSet->pBase = 0;` |
|   8617550 |  114 | `	pSet->nUsed = 0;` |
|   8617550 |  115 | `	pSet->nCursor = 0;` |
|   8617550 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4703618 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4703620 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       108 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4703514 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4703514 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2351811 |  126 |  |
|   3316740 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3316742 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2146128 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1170616 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1170616 |  135 | `	pSet->nUsed--;` |
|   1170616 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1170616 |  137 | `	return pData;` |
|   1658372 |  138 |  |
|  11205625 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  11205627 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  11205627 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  11205627 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5603000 |  148 |  |
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
|    270822 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    270824 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    270824 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    270824 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    270824 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    270824 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    270824 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    270824 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    270824 |  180 | `	pHash->nEntry = 0;` |
|    270824 |  181 | `	pHash->apBucket = apNew;` |
|    270824 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    270824 |  183 | `	return SXRET_OK;` |
|    135413 |  184 |  |
|     80498 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     80500 |  193 | `	pEntry = pHash->pList;` |
|     42370 |  194 | `	for(;;){` |
|     84742 |  195 | `		if( pHash->nEntry == 0 ){` |
|     80500 |  196 | `			break;` |
|         - |  197 | `		}` |
|      4244 |  198 | `		pNext = pEntry->pNext;` |
|      4244 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      4244 |  200 | `		pEntry = pNext;` |
|      4244 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     80500 |  203 | `	if( pHash->apBucket ){` |
|     80500 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     40249 |  205 | `	}` |
|     80500 |  206 | `	pHash->apBucket = 0;` |
|     80500 |  207 | `	pHash->nBucketSize = 0;` |
|     80500 |  208 | `	pHash->pAllocator = 0;` |
|     80500 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  12345356 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  12345358 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  12345358 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  11203429 |  218 | `	for(;;){` |
|  22390316 |  219 | `		if( pEntry == 0 ){` |
|   6812092 |  220 | `			break;` |
|         - |  221 | `		}` |
|  18344729 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5533270 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5533268 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  10044960 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   6812092 |  229 | `	return 0;` |
|   6172944 |  230 |  |
|  12842416 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  12842418 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    497166 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  12345254 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  12345254 |  244 | `	if( pEntry == 0 ){` |
|   6812092 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5533164 |  247 | `	return (SyHashEntry *)pEntry;` |
|   6421474 |  248 |  |
|     92938 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     92940 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     70668 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     35335 |  254 | `	}else{` |
|     22274 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     92940 |  257 | `	if( pEntry->pNextCollide ){` |
|      4717 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2358 |  259 | `	}` |
|     92940 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     92940 |  261 | `	pHash->nEntry--;` |
|     92940 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     92940 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     92940 |  268 | `	return rc;` |
|         2 |  269 |  |
|       104 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       106 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       106 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       106 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       106 |  284 | `	return rc;` |
|        54 |  285 |  |
|     92834 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     92836 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     92836 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     92836 |  296 | `	return rc;` |
|         2 |  297 |  |
|    323796 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    323798 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    323798 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2565648 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2565650 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    323364 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    323364 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2242288 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2242288 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2242288 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1282826 |  325 |  |
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
|      1773 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1763 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1763 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1763 |  344 | `		pEntry = pEntry->pNext;` |
|       882 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     23434 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     23436 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     23436 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     23436 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     23436 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2976300 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2952866 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2952866 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2952866 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2952866 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1411065 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    705473 |  371 | `		}` |
|   2952866 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2952866 |  374 | `		pEntry = pEntry->pNext;` |
|   1476434 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     23436 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     23436 |  378 | `	pHash->apBucket = apNew;` |
|     23436 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     23436 |  380 | `	return SXRET_OK;` |
|     11719 |  381 |  |
|   3025744 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   3025746 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   3025746 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   3025746 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1959727 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    979867 |  389 | `	}` |
|   3025746 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   3025746 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   3025746 |  393 | `	if( pHash->nEntry == 0 ){` |
|    135136 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     67567 |  395 | `	}` |
|   3025746 |  396 | `	pHash->nEntry++;` |
|   3025746 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   3025744 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   3025746 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     23436 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     23436 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     11717 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   3025746 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   3025746 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   3025746 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   3025746 |  421 | `	pEntry->pHash = pHash;` |
|   3025746 |  422 | `	pEntry->pKey = pKey;` |
|   3025746 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   3025746 |  424 | `	pEntry->pUserData = pUserData;` |
|   3025746 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   3025746 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   3025746 |  428 | `	return rc;` |
|   1512874 |  429 |  |
|    119124 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    119126 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
