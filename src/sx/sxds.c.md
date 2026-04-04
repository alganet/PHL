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
|  14522836 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  14522838 |   16 | `	pSet->nSize = 0 ;` |
|  14522838 |   17 | `	pSet->nUsed = 0;` |
|  14522838 |   18 | `	pSet->nCursor = 0;` |
|  14522838 |   19 | `	pSet->eSize = ElemSize;` |
|  14522838 |   20 | `	pSet->pAllocator = pAllocator;` |
|  14522838 |   21 | `	pSet->pBase =  0;` |
|  14522838 |   22 | `	pSet->pUserData = 0;` |
|  14522838 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  24299216 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  24299218 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3887920 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3887920 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3773370 |   34 | `			pSet->nSize = 4;` |
|   1886684 |   35 | `		}` |
|   3887920 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3887920 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3887920 |   40 | `		pSet->pBase = pNew;` |
|   3887920 |   41 | `		pSet->nSize <<= 1;` |
|   1943959 |   42 | `	}` |
|  24299218 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 181033902 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  24299218 |   45 | `	pSet->nUsed++;` |
|  24299218 |   46 | `	return SXRET_OK;` |
|  12149632 |   47 |  |
|    906074 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    906076 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    906076 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    906076 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    906076 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    906076 |   60 | `	pSet->nSize = nItem;` |
|    906076 |   61 | `	return SXRET_OK;` |
|    453039 |   62 |  |
|   1330744 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1330746 |   65 | `	pSet->nUsed   = 0;` |
|   1330746 |   66 | `	pSet->nCursor = 0;` |
|   1330746 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     43148 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     43150 |   71 | `	pSet->nCursor = 0;` |
|     43150 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     47076 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     47078 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     17790 |   79 | `		pSet->nCursor = 0;` |
|     17790 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     29290 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     29290 |   83 | `	if( ppEntry ){` |
|     29290 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     14644 |   85 | `	}` |
|     29290 |   86 | `	pSet->nCursor++;` |
|     29290 |   87 | `	return SXRET_OK;` |
|     23540 |   88 |  |
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
|    151848 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    151850 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|    151850 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8439120 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8439122 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8439122 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4309270 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2154634 |  112 | `	}` |
|   8439122 |  113 | `	pSet->pBase = 0;` |
|   8439122 |  114 | `	pSet->nUsed = 0;` |
|   8439122 |  115 | `	pSet->nCursor = 0;` |
|   8439122 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4724060 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4724062 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4723972 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4723972 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2362032 |  126 |  |
|   3244460 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3244462 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2146126 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1098338 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1098338 |  135 | `	pSet->nUsed--;` |
|   1098338 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1098338 |  137 | `	return pData;` |
|   1622232 |  138 |  |
|  10264770 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  10264772 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  10264772 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  10264772 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5132613 |  148 |  |
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
|    191638 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    191640 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    191640 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    191640 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    191640 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    191640 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    191640 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    191640 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    191640 |  180 | `	pHash->nEntry = 0;` |
|    191640 |  181 | `	pHash->apBucket = apNew;` |
|    191640 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    191640 |  183 | `	return SXRET_OK;` |
|     95821 |  184 |  |
|     30402 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     30404 |  193 | `	pEntry = pHash->pList;` |
|     16940 |  194 | `	for(;;){` |
|     33882 |  195 | `		if( pHash->nEntry == 0 ){` |
|     30404 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3480 |  198 | `		pNext = pEntry->pNext;` |
|      3480 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3480 |  200 | `		pEntry = pNext;` |
|      3480 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     30404 |  203 | `	if( pHash->apBucket ){` |
|     30404 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     15201 |  205 | `	}` |
|     30404 |  206 | `	pHash->apBucket = 0;` |
|     30404 |  207 | `	pHash->nBucketSize = 0;` |
|     30404 |  208 | `	pHash->pAllocator = 0;` |
|     30404 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  11684806 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  11684808 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  11684808 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  10640371 |  218 | `	for(;;){` |
|  21275361 |  219 | `		if( pEntry == 0 ){` |
|   6504908 |  220 | `			break;` |
|         - |  221 | `		}` |
|  17360275 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5179904 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5179902 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   9590555 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   6504908 |  229 | `	return 0;` |
|   5842669 |  230 |  |
|  11800394 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  11800396 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    115600 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  11684798 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  11684798 |  244 | `	if( pEntry == 0 ){` |
|   6504908 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5179892 |  247 | `	return (SyHashEntry *)pEntry;` |
|   5900463 |  248 |  |
|     85866 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     85868 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     65638 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     32820 |  254 | `	}else{` |
|     20232 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     85868 |  257 | `	if( pEntry->pNextCollide ){` |
|      4307 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2153 |  259 | `	}` |
|     85868 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     85868 |  261 | `	pHash->nEntry--;` |
|     85868 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     85868 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     85868 |  268 | `	return rc;` |
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
|     85856 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     85858 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     85858 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     85858 |  296 | `	return rc;` |
|         2 |  297 |  |
|    278630 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    278632 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    278632 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2099410 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2099412 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    278198 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    278198 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   1821216 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   1821216 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   1821216 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1049707 |  325 |  |
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
|      1753 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1743 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1743 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1743 |  344 | `		pEntry = pEntry->pNext;` |
|       872 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     23982 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     23984 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     23984 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     23984 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     23984 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3046928 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3022946 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3022946 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3022946 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3022946 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1444636 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    722348 |  371 | `		}` |
|   3022946 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3022946 |  374 | `		pEntry = pEntry->pNext;` |
|   1511474 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     23984 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     23984 |  378 | `	pHash->apBucket = apNew;` |
|     23984 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     23984 |  380 | `	return SXRET_OK;` |
|     11993 |  381 |  |
|   2966492 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2966494 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2966494 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2966494 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1979480 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    989748 |  389 | `	}` |
|   2966494 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2966494 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2966494 |  393 | `	if( pHash->nEntry == 0 ){` |
|    120938 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     60468 |  395 | `	}` |
|   2966494 |  396 | `	pHash->nEntry++;` |
|   2966494 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2966492 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2966494 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     23984 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     23984 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     11991 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2966494 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2966494 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2966494 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2966494 |  421 | `	pEntry->pHash = pHash;` |
|   2966494 |  422 | `	pEntry->pKey = pKey;` |
|   2966494 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2966494 |  424 | `	pEntry->pUserData = pUserData;` |
|   2966494 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2966494 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2966494 |  428 | `	return rc;` |
|   1483248 |  429 |  |
|    112960 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    112962 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
