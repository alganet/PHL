# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 286/304 lines (94.08%)

[Root index](../../index.md) | [Directory index](index.md)

|       Hits | Line | Source |
| ---------: | ---: | :--- |
|          - |    1 | `/**` |
|          - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|          - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|          - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|          - |    5 | ` */` |
|          - |    6 | `#include "sxtypes.h"` |
|          - |    7 | `#include "sxmacros.h"` |
|          - |    8 | `#include "sxset.h"` |
|          - |    9 | `#include "sxmem.h"` |
|          - |   10 | `#include "sxhashtable.h"` |
|          - |   11 | `#include "sxhash.h"` |
|          - |   12 | `#include "sxstr.h"` |
|          - |   13 |  |
|  184593557 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  184593562 |   16 | `	pSet->nSize = 0 ;` |
|  184593562 |   17 | `	pSet->nUsed = 0;` |
|  184593562 |   18 | `	pSet->nCursor = 0;` |
|  184593562 |   19 | `	pSet->eSize = ElemSize;` |
|  184593562 |   20 | `	pSet->pAllocator = pAllocator;` |
|  184593562 |   21 | `	pSet->pBase =  0;` |
|  184593562 |   22 | `	pSet->pUserData = 0;` |
|  184593562 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  418309036 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  418309041 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   24177551 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   24177551 |   33 | `		if( pSet->nSize <= 0 ){` |
|   20606247 |   34 | `			pSet->nSize = 4;` |
|   10304052 |   35 | `		}` |
|   24177551 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   24177551 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   24177551 |   40 | `		pSet->pBase = pNew;` |
|   24177551 |   41 | `		pSet->nSize <<= 1;` |
|   12089704 |   42 | `	}` |
|  418309041 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3302396807 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  418309041 |   45 | `	pSet->nUsed++;` |
|  418309041 |   46 | `	return SXRET_OK;` |
|  209157683 |   47 | `}` |
|   20789350 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   20789355 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   20789355 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   20789355 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   20789355 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   20789355 |   60 | `	pSet->nSize = nItem;` |
|   20789355 |   61 | `	return SXRET_OK;` |
|   10394680 |   62 | `}` |
|   30783766 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   30783771 |   65 | `	pSet->nUsed   = 0;` |
|   30783771 |   66 | `	pSet->nCursor = 0;` |
|   30783771 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69196 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69201 |   71 | `	pSet->nCursor = 0;` |
|      69201 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      69456 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      69461 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30125 |   79 | `		pSet->nCursor = 0;` |
|      30125 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      39341 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      39341 |   83 | `	if( ppEntry ){` |
|      39341 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      19668 |   85 | `	}` |
|      39341 |   86 | `	pSet->nCursor++;` |
|      39341 |   87 | `	return SXRET_OK;` |
|      34733 |   88 | `}` |
|          - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        ! 0 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|        ! 0 |   91 | `{` |
|          - |   92 | `	register unsigned char *zSrc;` |
|        ! 0 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|        ! 0 |   94 | `		return 0;` |
|          - |   95 | `	}` |
|        ! 0 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|        ! 0 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|        ! 0 |   98 | `}` |
|          - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    3284920 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3284925 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1253 |  103 | `		pSet->nUsed = nNewSize;` |
|        624 |  104 | `	}` |
|    3284925 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   63151609 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   63151614 |  109 | `	sxi32 rc = SXRET_OK;` |
|   63151614 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   34183651 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   17092754 |  112 | `	}` |
|   63151614 |  113 | `	pSet->pBase = 0;` |
|   63151614 |  114 | `	pSet->nUsed = 0;` |
|   63151614 |  115 | `	pSet->nCursor = 0;` |
|   63151614 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   74337806 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   74337811 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       4009 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   74333807 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   74333807 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   37168908 |  126 | `}` |
|    9864321 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9864326 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2237819 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7626512 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7626512 |  135 | `	pSet->nUsed--;` |
|    7626512 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7626512 |  137 | `	return pData;` |
|    4932786 |  138 | `}` |
|   37228697 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   37228702 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   37228650 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   37228650 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   18618498 |  148 | `}` |
|          - |  149 | `/* Private hash entry */` |
|          - |  150 | `struct SyHashEntry_Pr` |
|          - |  151 | `{` |
|          - |  152 | `	const void *pKey; /* Hash key */` |
|          - |  153 | `	sxu32 nKeyLen;    /* Key length */` |
|          - |  154 | `	void *pUserData;  /* User private data */` |
|          - |  155 | `	/* Private fields */` |
|          - |  156 | `	sxu32 nHash;` |
|          - |  157 | `	SyHash *pHash;` |
|          - |  158 | `	SyHashEntry_Pr *pNext,*pPrev; /* Next and previous entry in the list */` |
|          - |  159 | `	SyHashEntry_Pr *pNextCollide,*pPrevCollide; /* Collision list */` |
|          - |  160 | `};` |
|          - |  161 | `#define INVALID_HASH(H) ((H)->apBucket == 0)` |
|    2186129 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    2186134 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2186134 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    2186134 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2186134 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    2186134 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    2186134 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    2186134 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    2186134 |  180 | `	pHash->nEntry = 0;` |
|    2186134 |  181 | `	pHash->apBucket = apNew;` |
|    2186134 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    2186134 |  183 | `	return SXRET_OK;` |
|    1093173 |  184 | `}` |
|     520921 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     520926 |  193 | `	pEntry = pHash->pList;` |
|     276097 |  194 | `	for(;;){` |
|     551992 |  195 | `		if( pHash->nEntry == 0 ){` |
|     520926 |  196 | `			break;` |
|          - |  197 | `		}` |
|      31071 |  198 | `		pNext = pEntry->pNext;` |
|      31071 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      31071 |  200 | `		pEntry = pNext;` |
|      31071 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     520926 |  203 | `	if( pHash->apBucket ){` |
|     520926 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     260564 |  205 | `	}` |
|     520926 |  206 | `	pHash->apBucket = 0;` |
|     520926 |  207 | `	pHash->nBucketSize = 0;` |
|     520926 |  208 | `	pHash->pAllocator = 0;` |
|     520926 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   75249960 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   75249965 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   75249965 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   67999506 |  218 | `	for(;;){` |
|  136064297 |  219 | `		if( pEntry == 0 ){` |
|   27356133 |  220 | `			break;` |
|          - |  221 | `		}` |
|  132654537 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   47897840 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   47893837 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   60814337 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   27356133 |  229 | `	return 0;` |
|   37631134 |  230 | `}` |
|   84252469 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   84252474 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    9002972 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   75249507 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   75249507 |  244 | `	if( pEntry == 0 ){` |
|   27356115 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   47893397 |  247 | `	return (SyHashEntry *)pEntry;` |
|   42132492 |  248 | `}` |
|     509460 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     509465 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     414685 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     207861 |  254 | `	}else{` |
|      94785 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     509465 |  257 | `	if( pEntry->pNextCollide ){` |
|       4396 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2192 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     509465 |  261 | `	if( pHash->pLast == pEntry ){` |
|     499915 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     250576 |  263 | `	}` |
|     509465 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     509465 |  265 | `	pHash->nEntry--;` |
|     509465 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     509465 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     509465 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        458 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        463 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        463 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        445 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        445 |  288 | `	return rc;` |
|        234 |  289 | `}` |
|     509020 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     509025 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     509025 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     509025 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3427460 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3427465 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3427465 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   26407292 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   26407297 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3427199 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3427199 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   22980103 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   22980103 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   22980103 |  328 | `	return (SyHashEntry *)pEntry;` |
|   13203651 |  329 | `}` |
|         14 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|          1 |  331 | `{` |
|          - |  332 | `	SyHashEntry_Pr *pEntry;` |
|          - |  333 | `	sxi32 rc;` |
|          - |  334 | `	sxu32 n;` |
|          - |  335 | `#if defined(UNTRUST)` |
|          - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|          - |  337 | `		return 0;` |
|          - |  338 | `	}` |
|          - |  339 | `#endif` |
|         15 |  340 | `	pEntry = pHash->pList;` |
|       4857 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4843 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4843 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4843 |  348 | `		pEntry = pEntry->pNext;` |
|       2422 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|     100646 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100651 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100651 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100651 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100651 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18627883 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18527237 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18527237 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18527237 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18527237 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8937083 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4468619 |  375 | `		}` |
|   18527237 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18527237 |  378 | `		pEntry = pEntry->pNext;` |
|    9263621 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100651 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100651 |  382 | `	pHash->apBucket = apNew;` |
|     100651 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100651 |  384 | `	return SXRET_OK;` |
|      50328 |  385 | `}` |
|   22584138 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   22584143 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   22584143 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   22584143 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   14184026 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    7092213 |  393 | `	}` |
|   22584143 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   22584143 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|     881299 |  399 | `		pHash->pLast->pNext = pEntry;` |
|     881299 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|     881299 |  401 | `		pHash->pLast = pEntry;` |
|     440652 |  402 | `	}else{` |
|   21702849 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   22584143 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1213664 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1213664 |  408 | `		pHash->pLast = pEntry;` |
|     606933 |  409 | `	}` |
|   22584143 |  410 | `	pHash->nEntry++;` |
|   22584143 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   22584138 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   22584143 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100651 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100651 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50323 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   22584143 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   22584143 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   22584143 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   22584143 |  435 | `	pEntry->pHash = pHash;` |
|   22584143 |  436 | `	pEntry->pKey = pKey;` |
|   22584143 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   22584143 |  438 | `	pEntry->pUserData = pUserData;` |
|   22584143 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   22584143 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   22584143 |  442 | `	return rc;` |
|   11292695 |  443 | `}` |
|   21441200 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   21441205 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|    1142938 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  455 | `{` |
|    1142943 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          5 |  457 | `}` |
|     549426 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     549431 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
