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
|  17400696 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  17400698 |   16 | `	pSet->nSize = 0 ;` |
|  17400698 |   17 | `	pSet->nUsed = 0;` |
|  17400698 |   18 | `	pSet->nCursor = 0;` |
|  17400698 |   19 | `	pSet->eSize = ElemSize;` |
|  17400698 |   20 | `	pSet->pAllocator = pAllocator;` |
|  17400698 |   21 | `	pSet->pBase =  0;` |
|  17400698 |   22 | `	pSet->pUserData = 0;` |
|  17400698 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  28562850 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  28562852 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4267820 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4267820 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4127168 |   34 | `			pSet->nSize = 4;` |
|   2063583 |   35 | `		}` |
|   4267820 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4267820 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4267820 |   40 | `		pSet->pBase = pNew;` |
|   4267820 |   41 | `		pSet->nSize <<= 1;` |
|   2133909 |   42 | `	}` |
|  28562852 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 213716666 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  28562852 |   45 | `	pSet->nUsed++;` |
|  28562852 |   46 | `	return SXRET_OK;` |
|  14281449 |   47 |  |
|   1156614 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|   1156616 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1156616 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1156616 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1156616 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1156616 |   60 | `	pSet->nSize = nItem;` |
|   1156616 |   61 | `	return SXRET_OK;` |
|    578309 |   62 |  |
|   1636404 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1636406 |   65 | `	pSet->nUsed   = 0;` |
|   1636406 |   66 | `	pSet->nCursor = 0;` |
|   1636406 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     53574 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     53576 |   71 | `	pSet->nCursor = 0;` |
|     53576 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     57734 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     57736 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     22082 |   79 | `		pSet->nCursor = 0;` |
|     22082 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     35656 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     35656 |   83 | `	if( ppEntry ){` |
|     35656 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     17827 |   85 | `	}` |
|     35656 |   86 | `	pSet->nCursor++;` |
|     35656 |   87 | `	return SXRET_OK;` |
|     28869 |   88 |  |
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
|    196118 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    196120 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       110 |  103 | `		pSet->nUsed = nNewSize;` |
|        54 |  104 | `	}` |
|    196120 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   9342086 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   9342088 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9342088 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4719644 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2359821 |  112 | `	}` |
|   9342088 |  113 | `	pSet->pBase = 0;` |
|   9342088 |  114 | `	pSet->nUsed = 0;` |
|   9342088 |  115 | `	pSet->nCursor = 0;` |
|   9342088 |  116 | `	return rc;` |
|         2 |  117 |  |
|   5323262 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5323264 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       112 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5323154 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5323154 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2661633 |  126 |  |
|   3440474 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3440476 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2151598 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1288880 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1288880 |  135 | `	pSet->nUsed--;` |
|   1288880 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1288880 |  137 | `	return pData;` |
|   1720239 |  138 |  |
|  12529360 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  12529362 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  12529362 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  12529362 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6264861 |  148 |  |
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
|    371702 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    371704 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    371704 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    371704 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    371704 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    371704 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    371704 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    371704 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    371704 |  180 | `	pHash->nEntry = 0;` |
|    371704 |  181 | `	pHash->apBucket = apNew;` |
|    371704 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    371704 |  183 | `	return SXRET_OK;` |
|    185853 |  184 |  |
|     92292 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     92294 |  193 | `	pEntry = pHash->pList;` |
|     49173 |  194 | `	for(;;){` |
|     98348 |  195 | `		if( pHash->nEntry == 0 ){` |
|     92294 |  196 | `			break;` |
|         - |  197 | `		}` |
|      6056 |  198 | `		pNext = pEntry->pNext;` |
|      6056 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      6056 |  200 | `		pEntry = pNext;` |
|      6056 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     92294 |  203 | `	if( pHash->apBucket ){` |
|     92294 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     46146 |  205 | `	}` |
|     92294 |  206 | `	pHash->apBucket = 0;` |
|     92294 |  207 | `	pHash->nBucketSize = 0;` |
|     92294 |  208 | `	pHash->pAllocator = 0;` |
|     92294 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  14237880 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  14237882 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  14237882 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  12774206 |  218 | `	for(;;){` |
|  25448917 |  219 | `		if( pEntry == 0 ){` |
|   7737846 |  220 | `			break;` |
|         - |  221 | `		}` |
|  20960961 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   6500040 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   6500038 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  11211037 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   7737846 |  229 | `	return 0;` |
|   7119206 |  230 |  |
|  14860676 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  14860678 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    622952 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  14237728 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  14237728 |  244 | `	if( pEntry == 0 ){` |
|   7737846 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   6499884 |  247 | `	return (SyHashEntry *)pEntry;` |
|   7430604 |  248 |  |
|    110672 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    110674 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     84700 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     42351 |  254 | `	}else{` |
|     25976 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    110674 |  257 | `	if( pEntry->pNextCollide ){` |
|      4851 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2425 |  259 | `	}` |
|    110674 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    110674 |  261 | `	pHash->nEntry--;` |
|    110674 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    110674 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    110674 |  268 | `	return rc;` |
|         2 |  269 |  |
|       154 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       156 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       156 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       156 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       156 |  284 | `	return rc;` |
|        79 |  285 |  |
|    110518 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|    110520 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    110520 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    110520 |  296 | `	return rc;` |
|         2 |  297 |  |
|    609236 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    609238 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    609238 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   3495704 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   3495706 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    608802 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    608802 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2886906 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2886906 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2886906 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1747854 |  325 |  |
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
|      1895 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1885 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1885 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1885 |  344 | `		pEntry = pEntry->pNext;` |
|       943 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     26138 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     26140 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     26140 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     26140 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     26140 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3321916 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3295778 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3295778 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3295778 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3295778 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1569773 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    784943 |  371 | `		}` |
|   3295778 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3295778 |  374 | `		pEntry = pEntry->pNext;` |
|   1647890 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     26140 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     26140 |  378 | `	pHash->apBucket = apNew;` |
|     26140 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     26140 |  380 | `	return SXRET_OK;` |
|     13071 |  381 |  |
|   3559230 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   3559232 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   3559232 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   3559232 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2290930 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1145497 |  389 | `	}` |
|   3559232 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   3559232 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   3559232 |  393 | `	if( pHash->nEntry == 0 ){` |
|    178130 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     89064 |  395 | `	}` |
|   3559232 |  396 | `	pHash->nEntry++;` |
|   3559232 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   3559230 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   3559232 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     26140 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     26140 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     13069 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   3559232 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   3559232 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   3559232 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   3559232 |  421 | `	pEntry->pHash = pHash;` |
|   3559232 |  422 | `	pEntry->pKey = pKey;` |
|   3559232 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   3559232 |  424 | `	pEntry->pUserData = pUserData;` |
|   3559232 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   3559232 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   3559232 |  428 | `	return rc;` |
|   1779617 |  429 |  |
|    140000 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    140002 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
