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
|  16231860 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  16231862 |   16 | `	pSet->nSize = 0 ;` |
|  16231862 |   17 | `	pSet->nUsed = 0;` |
|  16231862 |   18 | `	pSet->nCursor = 0;` |
|  16231862 |   19 | `	pSet->eSize = ElemSize;` |
|  16231862 |   20 | `	pSet->pAllocator = pAllocator;` |
|  16231862 |   21 | `	pSet->pBase =  0;` |
|  16231862 |   22 | `	pSet->pUserData = 0;` |
|  16231862 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  26646456 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  26646458 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4072644 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4072644 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3941910 |   34 | `			pSet->nSize = 4;` |
|   1970954 |   35 | `		}` |
|   4072644 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4072644 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4072644 |   40 | `		pSet->pBase = pNew;` |
|   4072644 |   41 | `		pSet->nSize <<= 1;` |
|   2036321 |   42 | `	}` |
|  26646458 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 199075796 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  26646458 |   45 | `	pSet->nUsed++;` |
|  26646458 |   46 | `	return SXRET_OK;` |
|  13323252 |   47 |  |
|   1049142 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|   1049144 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1049144 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1049144 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1049144 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1049144 |   60 | `	pSet->nSize = nItem;` |
|   1049144 |   61 | `	return SXRET_OK;` |
|    524573 |   62 |  |
|   1524040 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1524042 |   65 | `	pSet->nUsed   = 0;` |
|   1524042 |   66 | `	pSet->nCursor = 0;` |
|   1524042 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     49832 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     49834 |   71 | `	pSet->nCursor = 0;` |
|     49834 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     53914 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     53916 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     20498 |   79 | `		pSet->nCursor = 0;` |
|     20498 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     33420 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     33420 |   83 | `	if( ppEntry ){` |
|     33420 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     16709 |   85 | `	}` |
|     33420 |   86 | `	pSet->nCursor++;` |
|     33420 |   87 | `	return SXRET_OK;` |
|     26959 |   88 |  |
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
|    182352 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    182354 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       110 |  103 | `		pSet->nUsed = nNewSize;` |
|        54 |  104 | `	}` |
|    182354 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8981928 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8981930 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8981930 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4537014 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2268506 |  112 | `	}` |
|   8981930 |  113 | `	pSet->pBase = 0;` |
|   8981930 |  114 | `	pSet->nUsed = 0;` |
|   8981930 |  115 | `	pSet->nCursor = 0;` |
|   8981930 |  116 | `	return rc;` |
|         2 |  117 |  |
|   5064520 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5064522 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       108 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5064416 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5064416 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2532262 |  126 |  |
|   3348742 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3348744 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2147622 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1201124 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1201124 |  135 | `	pSet->nUsed--;` |
|   1201124 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1201124 |  137 | `	return pData;` |
|   1674373 |  138 |  |
|  11668977 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  11668979 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  11668979 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  11668979 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5834601 |  148 |  |
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
|    292152 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    292154 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    292154 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    292154 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    292154 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    292154 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    292154 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    292154 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    292154 |  180 | `	pHash->nEntry = 0;` |
|    292154 |  181 | `	pHash->apBucket = apNew;` |
|    292154 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    292154 |  183 | `	return SXRET_OK;` |
|    146078 |  184 |  |
|     84812 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     84814 |  193 | `	pEntry = pHash->pList;` |
|     44888 |  194 | `	for(;;){` |
|     89778 |  195 | `		if( pHash->nEntry == 0 ){` |
|     84814 |  196 | `			break;` |
|         - |  197 | `		}` |
|      4966 |  198 | `		pNext = pEntry->pNext;` |
|      4966 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      4966 |  200 | `		pEntry = pNext;` |
|      4966 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     84814 |  203 | `	if( pHash->apBucket ){` |
|     84814 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     42406 |  205 | `	}` |
|     84814 |  206 | `	pHash->apBucket = 0;` |
|     84814 |  207 | `	pHash->nBucketSize = 0;` |
|     84814 |  208 | `	pHash->pAllocator = 0;` |
|     84814 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  13084742 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  13084744 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  13084744 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  11746967 |  218 | `	for(;;){` |
|  23376826 |  219 | `		if( pEntry == 0 ){` |
|   7139732 |  220 | `			break;` |
|         - |  221 | `		}` |
|  19209472 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5945016 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5945014 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  10292084 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   7139732 |  229 | `	return 0;` |
|   6542637 |  230 |  |
|  13638646 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  13638648 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    554048 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  13084602 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  13084602 |  244 | `	if( pEntry == 0 ){` |
|   7139732 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5944872 |  247 | `	return (SyHashEntry *)pEntry;` |
|   6819589 |  248 |  |
|    100370 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    100372 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     76598 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     38300 |  254 | `	}else{` |
|     23776 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    100372 |  257 | `	if( pEntry->pNextCollide ){` |
|      4935 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2467 |  259 | `	}` |
|    100372 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    100372 |  261 | `	pHash->nEntry--;` |
|    100372 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    100372 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    100372 |  268 | `	return rc;` |
|         2 |  269 |  |
|       142 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       144 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       144 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       144 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       144 |  284 | `	return rc;` |
|        73 |  285 |  |
|    100228 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|    100230 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    100230 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    100230 |  296 | `	return rc;` |
|         2 |  297 |  |
|    365782 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    365784 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    365784 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2830150 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2830152 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    365348 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    365348 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2464806 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2464806 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2464806 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1415077 |  325 |  |
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
|      1791 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1781 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1781 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1781 |  344 | `		pEntry = pEntry->pNext;` |
|       891 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     24250 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     24252 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     24252 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     24252 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     24252 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3079644 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3055394 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3055394 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3055394 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3055394 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1463180 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    731545 |  371 | `		}` |
|   3055394 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3055394 |  374 | `		pEntry = pEntry->pNext;` |
|   1527698 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     24252 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     24252 |  378 | `	pHash->apBucket = apNew;` |
|     24252 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     24252 |  380 | `	return SXRET_OK;` |
|     12127 |  381 |  |
|   3192788 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   3192790 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   3192790 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   3192790 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2055105 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1027607 |  389 | `	}` |
|   3192790 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   3192790 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   3192790 |  393 | `	if( pHash->nEntry == 0 ){` |
|    144752 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     72375 |  395 | `	}` |
|   3192790 |  396 | `	pHash->nEntry++;` |
|   3192790 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   3192788 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   3192790 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     24252 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     24252 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     12125 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   3192790 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   3192790 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   3192790 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   3192790 |  421 | `	pEntry->pHash = pHash;` |
|   3192790 |  422 | `	pEntry->pKey = pKey;` |
|   3192790 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   3192790 |  424 | `	pEntry->pUserData = pUserData;` |
|   3192790 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   3192790 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   3192790 |  428 | `	return rc;` |
|   1596396 |  429 |  |
|    127484 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    127486 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
