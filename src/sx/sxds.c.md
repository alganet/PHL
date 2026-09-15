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
|  184855975 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  184855980 |   16 | `	pSet->nSize = 0 ;` |
|  184855980 |   17 | `	pSet->nUsed = 0;` |
|  184855980 |   18 | `	pSet->nCursor = 0;` |
|  184855980 |   19 | `	pSet->eSize = ElemSize;` |
|  184855980 |   20 | `	pSet->pAllocator = pAllocator;` |
|  184855980 |   21 | `	pSet->pBase =  0;` |
|  184855980 |   22 | `	pSet->pUserData = 0;` |
|  184855980 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  418955696 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  418955701 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   24180359 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   24180359 |   33 | `		if( pSet->nSize <= 0 ){` |
|   20605825 |   34 | `			pSet->nSize = 4;` |
|   10303841 |   35 | `		}` |
|   24180359 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   24180359 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   24180359 |   40 | `		pSet->pBase = pNew;` |
|   24180359 |   41 | `		pSet->nSize <<= 1;` |
|   12091108 |   42 | `	}` |
|  418955701 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3308012307 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  418955701 |   45 | `	pSet->nUsed++;` |
|  418955701 |   46 | `	return SXRET_OK;` |
|  209481012 |   47 | `}` |
|   20831400 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   20831405 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   20831405 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   20831405 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   20831405 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   20831405 |   60 | `	pSet->nSize = nItem;` |
|   20831405 |   61 | `	return SXRET_OK;` |
|   10415705 |   62 | `}` |
|   30827054 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   30827059 |   65 | `	pSet->nUsed   = 0;` |
|   30827059 |   66 | `	pSet->nCursor = 0;` |
|   30827059 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69150 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69155 |   71 | `	pSet->nCursor = 0;` |
|      69155 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      69394 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      69399 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30059 |   79 | `		pSet->nCursor = 0;` |
|      30059 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      39345 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      39345 |   83 | `	if( ppEntry ){` |
|      39345 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      19670 |   85 | `	}` |
|      39345 |   86 | `	pSet->nCursor++;` |
|      39345 |   87 | `	return SXRET_OK;` |
|      34702 |   88 | `}` |
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
|    3291590 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3291595 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1213 |  103 | `		pSet->nUsed = nNewSize;` |
|        604 |  104 | `	}` |
|    3291595 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   63199385 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   63199390 |  109 | `	sxi32 rc = SXRET_OK;` |
|   63199390 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   34218369 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   17110113 |  112 | `	}` |
|   63199390 |  113 | `	pSet->pBase = 0;` |
|   63199390 |  114 | `	pSet->nUsed = 0;` |
|   63199390 |  115 | `	pSet->nCursor = 0;` |
|   63199390 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   74481950 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   74481955 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       4017 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   74477943 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   74477943 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   37240980 |  126 | `}` |
|    9859437 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9859442 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2238075 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7621372 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7621372 |  135 | `	pSet->nUsed--;` |
|    7621372 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7621372 |  137 | `	return pData;` |
|    4930344 |  138 | `}` |
|   37090452 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   37090457 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   37090405 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   37090405 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   18549368 |  148 | `}` |
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
|    2162599 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    2162604 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2162604 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    2162604 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2162604 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    2162604 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    2162604 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    2162604 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    2162604 |  180 | `	pHash->nEntry = 0;` |
|    2162604 |  181 | `	pHash->apBucket = apNew;` |
|    2162604 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    2162604 |  183 | `	return SXRET_OK;` |
|    1081408 |  184 | `}` |
|     521311 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     521316 |  193 | `	pEntry = pHash->pList;` |
|     276038 |  194 | `	for(;;){` |
|     551874 |  195 | `		if( pHash->nEntry == 0 ){` |
|     521316 |  196 | `			break;` |
|          - |  197 | `		}` |
|      30563 |  198 | `		pNext = pEntry->pNext;` |
|      30563 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      30563 |  200 | `		pEntry = pNext;` |
|      30563 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     521316 |  203 | `	if( pHash->apBucket ){` |
|     521316 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     260759 |  205 | `	}` |
|     521316 |  206 | `	pHash->apBucket = 0;` |
|     521316 |  207 | `	pHash->nBucketSize = 0;` |
|     521316 |  208 | `	pHash->pAllocator = 0;` |
|     521316 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   75060014 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   75060019 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   75060019 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   67924341 |  218 | `	for(;;){` |
|  135829847 |  219 | `		if( pEntry == 0 ){` |
|   27248665 |  220 | `			break;` |
|          - |  221 | `		}` |
|  132486324 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   47815368 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   47811359 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   60769833 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   27248665 |  229 | `	return 0;` |
|   37536151 |  230 | `}` |
|   84017435 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   84017440 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    8957884 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   75059561 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   75059561 |  244 | `	if( pEntry == 0 ){` |
|   27248647 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   47810919 |  247 | `	return (SyHashEntry *)pEntry;` |
|   42014965 |  248 | `}` |
|     495800 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     495805 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     407010 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     204025 |  254 | `	}else{` |
|      88800 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     495805 |  257 | `	if( pEntry->pNextCollide ){` |
|       4316 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2154 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     495805 |  261 | `	if( pHash->pLast == pEntry ){` |
|     486219 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     243728 |  263 | `	}` |
|     495805 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     495805 |  265 | `	pHash->nEntry--;` |
|     495805 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     495805 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     495805 |  272 | `	return rc;` |
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
|     495360 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     495365 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     495365 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     495365 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3378224 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3378229 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3378229 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   26111128 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   26111133 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3377963 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3377963 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   22733175 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   22733175 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   22733175 |  328 | `	return (SyHashEntry *)pEntry;` |
|   13055569 |  329 | `}` |
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
|       4833 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4819 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4819 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4819 |  348 | `		pEntry = pEntry->pNext;` |
|       2410 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|     100950 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100955 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100955 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100955 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100955 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18695195 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18594245 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18594245 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18594245 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18594245 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8978429 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4489526 |  375 | `		}` |
|   18594245 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18594245 |  378 | `		pEntry = pEntry->pNext;` |
|    9297125 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100955 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100955 |  382 | `	pHash->apBucket = apNew;` |
|     100955 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100955 |  384 | `	return SXRET_OK;` |
|      50480 |  385 | `}` |
|   22497558 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   22497563 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   22497563 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   22497563 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   14199665 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    7099496 |  393 | `	}` |
|   22497563 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   22497563 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|     882547 |  399 | `		pHash->pLast->pNext = pEntry;` |
|     882547 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|     882547 |  401 | `		pHash->pLast = pEntry;` |
|     441276 |  402 | `	}else{` |
|   21615021 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   22497563 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1195876 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1195876 |  408 | `		pHash->pLast = pEntry;` |
|     598039 |  409 | `	}` |
|   22497563 |  410 | `	pHash->nEntry++;` |
|   22497563 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   22497558 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   22497563 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100955 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100955 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50475 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   22497563 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   22497563 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   22497563 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   22497563 |  435 | `	pEntry->pHash = pHash;` |
|   22497563 |  436 | `	pEntry->pKey = pKey;` |
|   22497563 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   22497563 |  438 | `	pEntry->pUserData = pUserData;` |
|   22497563 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   22497563 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   22497563 |  442 | `	return rc;` |
|   11249405 |  443 | `}` |
|   21352982 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   21352987 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|    1144576 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  455 | `{` |
|    1144581 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          5 |  457 | `}` |
|     535924 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     535929 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
