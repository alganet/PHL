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
|   9617562 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|   9617564 |   16 | `	pSet->nSize = 0 ;` |
|   9617564 |   17 | `	pSet->nUsed = 0;` |
|   9617564 |   18 | `	pSet->nCursor = 0;` |
|   9617564 |   19 | `	pSet->eSize = ElemSize;` |
|   9617564 |   20 | `	pSet->pAllocator = pAllocator;` |
|   9617564 |   21 | `	pSet->pBase =  0;` |
|   9617564 |   22 | `	pSet->pUserData = 0;` |
|   9617564 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  15162842 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  15162844 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3177318 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3177318 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3120782 |   34 | `			pSet->nSize = 4;` |
|   1560390 |   35 | `		}` |
|   3177318 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3177318 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3177318 |   40 | `		pSet->pBase = pNew;` |
|   3177318 |   41 | `		pSet->nSize <<= 1;` |
|   1588658 |   42 | `	}` |
|  15162844 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 114966688 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  15162844 |   45 | `	pSet->nUsed++;` |
|  15162844 |   46 | `	return SXRET_OK;` |
|   7581445 |   47 |  |
|    405346 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    405348 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    405348 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    405348 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    405348 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    405348 |   60 | `	pSet->nSize = nItem;` |
|    405348 |   61 | `	return SXRET_OK;` |
|    202675 |   62 |  |
|    807776 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|    807778 |   65 | `	pSet->nUsed   = 0;` |
|    807778 |   66 | `	pSet->nCursor = 0;` |
|    807778 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     33134 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     33136 |   71 | `	pSet->nCursor = 0;` |
|     33136 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     36250 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     36252 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     13238 |   79 | `		pSet->nCursor = 0;` |
|     13238 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     23016 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     23016 |   83 | `	if( ppEntry ){` |
|     23016 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     11507 |   85 | `	}` |
|     23016 |   86 | `	pSet->nCursor++;` |
|     23016 |   87 | `	return SXRET_OK;` |
|     18127 |   88 |  |
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
|     51132 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     51134 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     51134 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   6577704 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   6577706 |  109 | `	sxi32 rc = SXRET_OK;` |
|   6577706 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3375700 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1687849 |  112 | `	}` |
|   6577706 |  113 | `	pSet->pBase = 0;` |
|   6577706 |  114 | `	pSet->nUsed = 0;` |
|   6577706 |  115 | `	pSet->nCursor = 0;` |
|   6577706 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3213150 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3213152 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3213062 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3213062 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1606577 |  126 |  |
|   2884822 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   2884824 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2119424 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    765402 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    765402 |  135 | `	pSet->nUsed--;` |
|    765402 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    765402 |  137 | `	return pData;` |
|   1442413 |  138 |  |
|   8087941 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   8087943 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   8087943 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   8087943 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4044169 |  148 |  |
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
|     72796 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     72798 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     72798 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     72798 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     72798 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     72798 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     72798 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     72798 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     72798 |  180 | `	pHash->nEntry = 0;` |
|     72798 |  181 | `	pHash->apBucket = apNew;` |
|     72798 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     72798 |  183 | `	return SXRET_OK;` |
|     36400 |  184 |  |
|      9342 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|      9344 |  193 | `	pEntry = pHash->pList;` |
|      5325 |  194 | `	for(;;){` |
|     10652 |  195 | `		if( pHash->nEntry == 0 ){` |
|      9344 |  196 | `			break;` |
|         - |  197 | `		}` |
|      1310 |  198 | `		pNext = pEntry->pNext;` |
|      1310 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      1310 |  200 | `		pEntry = pNext;` |
|      1310 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|      9344 |  203 | `	if( pHash->apBucket ){` |
|      9344 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      4671 |  205 | `	}` |
|      9344 |  206 | `	pHash->apBucket = 0;` |
|      9344 |  207 | `	pHash->nBucketSize = 0;` |
|      9344 |  208 | `	pHash->pAllocator = 0;` |
|      9344 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   7388946 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   7388948 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   7388948 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   6492946 |  218 | `	for(;;){` |
|  12931495 |  219 | `		if( pEntry == 0 ){` |
|   3996258 |  220 | `			break;` |
|         - |  221 | `		}` |
|  10631454 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   3392694 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   3392692 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   5542549 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   3996258 |  229 | `	return 0;` |
|   3694739 |  230 |  |
|   7430186 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   7430188 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     41248 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   7388942 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   7388942 |  244 | `	if( pEntry == 0 ){` |
|   3996258 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   3392686 |  247 | `	return (SyHashEntry *)pEntry;` |
|   3715359 |  248 |  |
|     60234 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     60236 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     45070 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     22536 |  254 | `	}else{` |
|     15168 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     60236 |  257 | `	if( pEntry->pNextCollide ){` |
|      3607 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      1803 |  259 | `	}` |
|     60236 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     60236 |  261 | `	pHash->nEntry--;` |
|     60236 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     60236 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     60236 |  268 | `	return rc;` |
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
|     60228 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     60230 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     60230 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     60230 |  296 | `	return rc;` |
|         2 |  297 |  |
|    107368 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    107370 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    107370 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    749572 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    749574 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    106936 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    106936 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    642640 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    642640 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    642640 |  324 | `	return (SyHashEntry *)pEntry;` |
|    374788 |  325 |  |
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
|     10124 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     10126 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     10126 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     10126 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     10126 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1384558 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1374434 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1374434 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1374434 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1374434 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    660037 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    330014 |  371 | `		}` |
|   1374434 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1374434 |  374 | `		pEntry = pEntry->pNext;` |
|    687218 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     10126 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     10126 |  378 | `	pHash->apBucket = apNew;` |
|     10126 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     10126 |  380 | `	return SXRET_OK;` |
|      5064 |  381 |  |
|   1255368 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1255370 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1255370 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1255370 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    834486 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    417203 |  389 | `	}` |
|   1255370 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1255370 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1255370 |  393 | `	if( pHash->nEntry == 0 ){` |
|     52150 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     26074 |  395 | `	}` |
|   1255370 |  396 | `	pHash->nEntry++;` |
|   1255370 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1255368 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1255370 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     10126 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     10126 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      5062 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1255370 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1255370 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1255370 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1255370 |  421 | `	pEntry->pHash = pHash;` |
|   1255370 |  422 | `	pEntry->pKey = pKey;` |
|   1255370 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1255370 |  424 | `	pEntry->pUserData = pUserData;` |
|   1255370 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1255370 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1255370 |  428 | `	return rc;` |
|    627686 |  429 |  |
|     72998 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     73000 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
