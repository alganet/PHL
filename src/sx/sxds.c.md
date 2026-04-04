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
|  14383184 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  14383186 |   16 | `	pSet->nSize = 0 ;` |
|  14383186 |   17 | `	pSet->nUsed = 0;` |
|  14383186 |   18 | `	pSet->nCursor = 0;` |
|  14383186 |   19 | `	pSet->eSize = ElemSize;` |
|  14383186 |   20 | `	pSet->pAllocator = pAllocator;` |
|  14383186 |   21 | `	pSet->pBase =  0;` |
|  14383186 |   22 | `	pSet->pUserData = 0;` |
|  14383186 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  24029962 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  24029964 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3870904 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3870904 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3758206 |   34 | `			pSet->nSize = 4;` |
|   1879102 |   35 | `		}` |
|   3870904 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3870904 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3870904 |   40 | `		pSet->pBase = pNew;` |
|   3870904 |   41 | `		pSet->nSize <<= 1;` |
|   1935451 |   42 | `	}` |
|  24029964 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 179052084 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  24029964 |   45 | `	pSet->nUsed++;` |
|  24029964 |   46 | `	return SXRET_OK;` |
|  12015005 |   47 |  |
|    890942 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    890944 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    890944 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    890944 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    890944 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    890944 |   60 | `	pSet->nSize = nItem;` |
|    890944 |   61 | `	return SXRET_OK;` |
|    445473 |   62 |  |
|   1315638 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1315640 |   65 | `	pSet->nUsed   = 0;` |
|   1315640 |   66 | `	pSet->nCursor = 0;` |
|   1315640 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     42956 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     42958 |   71 | `	pSet->nCursor = 0;` |
|     42958 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     46884 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     46886 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     17694 |   79 | `		pSet->nCursor = 0;` |
|     17694 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     29194 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     29194 |   83 | `	if( ppEntry ){` |
|     29194 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     14596 |   85 | `	}` |
|     29194 |   86 | `	pSet->nCursor++;` |
|     29194 |   87 | `	return SXRET_OK;` |
|     23444 |   88 |  |
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
|    149156 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    149158 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|    149158 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8393046 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8393048 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8393048 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4285640 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2142819 |  112 | `	}` |
|   8393048 |  113 | `	pSet->pBase = 0;` |
|   8393048 |  114 | `	pSet->nUsed = 0;` |
|   8393048 |  115 | `	pSet->nCursor = 0;` |
|   8393048 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4679314 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4679316 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4679226 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4679226 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2339659 |  126 |  |
|   3237926 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3237928 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2145288 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1092642 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1092642 |  135 | `	pSet->nUsed--;` |
|   1092642 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1092642 |  137 | `	return pData;` |
|   1618965 |  138 |  |
|  10233245 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  10233247 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  10233247 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  10233247 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5116838 |  148 |  |
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
|    188538 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    188540 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    188540 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    188540 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    188540 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    188540 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    188540 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    188540 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    188540 |  180 | `	pHash->nEntry = 0;` |
|    188540 |  181 | `	pHash->apBucket = apNew;` |
|    188540 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    188540 |  183 | `	return SXRET_OK;` |
|     94271 |  184 |  |
|     30108 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     30110 |  193 | `	pEntry = pHash->pList;` |
|     16775 |  194 | `	for(;;){` |
|     33552 |  195 | `		if( pHash->nEntry == 0 ){` |
|     30110 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3444 |  198 | `		pNext = pEntry->pNext;` |
|      3444 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3444 |  200 | `		pEntry = pNext;` |
|      3444 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     30110 |  203 | `	if( pHash->apBucket ){` |
|     30110 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     15054 |  205 | `	}` |
|     30110 |  206 | `	pHash->apBucket = 0;` |
|     30110 |  207 | `	pHash->nBucketSize = 0;` |
|     30110 |  208 | `	pHash->pAllocator = 0;` |
|     30110 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  11580680 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  11580682 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  11580682 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  10583833 |  218 | `	for(;;){` |
|  21061649 |  219 | `		if( pEntry == 0 ){` |
|   6440212 |  220 | `			break;` |
|         - |  221 | `		}` |
|  17191544 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5140474 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5140472 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   9480969 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   6440212 |  229 | `	return 0;` |
|   5790606 |  230 |  |
|  11694362 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  11694364 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    113694 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  11580672 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  11580672 |  244 | `	if( pEntry == 0 ){` |
|   6440212 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5140462 |  247 | `	return (SyHashEntry *)pEntry;` |
|   5847447 |  248 |  |
|     85262 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     85264 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     65130 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     32566 |  254 | `	}else{` |
|     20136 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     85264 |  257 | `	if( pEntry->pNextCollide ){` |
|      4307 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2153 |  259 | `	}` |
|     85264 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     85264 |  261 | `	pHash->nEntry--;` |
|     85264 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     85264 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     85264 |  268 | `	return rc;` |
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
|     85252 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     85254 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     85254 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     85254 |  296 | `	return rc;` |
|         2 |  297 |  |
|    274362 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    274364 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    274364 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2066964 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2066966 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    273930 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    273930 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   1793038 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   1793038 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   1793038 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1033484 |  325 |  |
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
|     23528 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     23530 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     23530 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     23530 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     23530 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2988682 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2965154 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2965154 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2965154 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2965154 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1416903 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    708400 |  371 | `		}` |
|   2965154 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2965154 |  374 | `		pEntry = pEntry->pNext;` |
|   1482578 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     23530 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     23530 |  378 | `	pHash->apBucket = apNew;` |
|     23530 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     23530 |  380 | `	return SXRET_OK;` |
|     11766 |  381 |  |
|   2912142 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2912144 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2912144 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2912144 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1942295 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    971136 |  389 | `	}` |
|   2912144 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2912144 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2912144 |  393 | `	if( pHash->nEntry == 0 ){` |
|    118928 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     59463 |  395 | `	}` |
|   2912144 |  396 | `	pHash->nEntry++;` |
|   2912144 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2912142 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2912144 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     23530 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     23530 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     11764 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2912144 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2912144 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2912144 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2912144 |  421 | `	pEntry->pHash = pHash;` |
|   2912144 |  422 | `	pEntry->pKey = pKey;` |
|   2912144 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2912144 |  424 | `	pEntry->pUserData = pUserData;` |
|   2912144 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2912144 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2912144 |  428 | `	return rc;` |
|   1456073 |  429 |  |
|    111806 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    111808 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
