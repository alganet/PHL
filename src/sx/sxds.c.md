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
|  18882806 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 |  |
|  18882811 |   16 | `	pSet->nSize = 0 ;` |
|  18882811 |   17 | `	pSet->nUsed = 0;` |
|  18882811 |   18 | `	pSet->nCursor = 0;` |
|  18882811 |   19 | `	pSet->eSize = ElemSize;` |
|  18882811 |   20 | `	pSet->pAllocator = pAllocator;` |
|  18882811 |   21 | `	pSet->pBase =  0;` |
|  18882811 |   22 | `	pSet->pUserData = 0;` |
|  18882811 |   23 | `	return SXRET_OK;` |
|         5 |   24 |  |
|  30999511 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  30999516 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4521223 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4521223 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4367453 |   34 | `			pSet->nSize = 4;` |
|   2183724 |   35 | `		}` |
|   4521223 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4521223 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4521223 |   40 | `		pSet->pBase = pNew;` |
|   4521223 |   41 | `		pSet->nSize <<= 1;` |
|   2260609 |   42 | `	}` |
|  30999516 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 231761232 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  30999516 |   45 | `	pSet->nUsed++;` |
|  30999516 |   46 | `	return SXRET_OK;` |
|  15499803 |   47 |  |
|   1269842 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 |  |
|   1269847 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1269847 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1269847 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1269847 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1269847 |   60 | `	pSet->nSize = nItem;` |
|   1269847 |   61 | `	return SXRET_OK;` |
|    634926 |   62 |  |
|   1767893 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 |  |
|   1767898 |   65 | `	pSet->nUsed   = 0;` |
|   1767898 |   66 | `	pSet->nCursor = 0;` |
|   1767898 |   67 | `	return SXRET_OK;` |
|         5 |   68 |  |
|     57056 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 |  |
|     57061 |   71 | `	pSet->nCursor = 0;` |
|     57061 |   72 | `	return SXRET_OK;` |
|         5 |   73 |  |
|     61262 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     61267 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     23597 |   79 | `		pSet->nCursor = 0;` |
|     23597 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     37675 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     37675 |   83 | `	if( ppEntry ){` |
|     37675 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     18835 |   85 | `	}` |
|     37675 |   86 | `	pSet->nCursor++;` |
|     37675 |   87 | `	return SXRET_OK;` |
|     30636 |   88 |  |
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
|    215090 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 |  |
|    215095 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       119 |  103 | `		pSet->nUsed = nNewSize;` |
|        57 |  104 | `	}` |
|    215095 |  105 | `	return SXRET_OK;` |
|         5 |  106 |  |
|   9882710 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 |  |
|   9882715 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9882715 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4945313 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2472654 |  112 | `	}` |
|   9882715 |  113 | `	pSet->pBase = 0;` |
|   9882715 |  114 | `	pSet->nUsed = 0;` |
|   9882715 |  115 | `	pSet->nCursor = 0;` |
|   9882715 |  116 | `	return rc;` |
|         5 |  117 |  |
|   5658902 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5658907 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       131 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5658781 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5658781 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2829456 |  126 |  |
|   3568832 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3568837 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2178599 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1390243 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1390243 |  135 | `	pSet->nUsed--;` |
|   1390243 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1390243 |  137 | `	return pData;` |
|   1784421 |  138 |  |
|  13230989 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  13230994 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13230994 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13230994 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6615828 |  148 |  |
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
|    551748 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    551753 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    551753 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    551753 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    551753 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    551753 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    551753 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    551753 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    551753 |  180 | `	pHash->nEntry = 0;` |
|    551753 |  181 | `	pHash->apBucket = apNew;` |
|    551753 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    551753 |  183 | `	return SXRET_OK;` |
|    275879 |  184 |  |
|    100806 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    100811 |  193 | `	pEntry = pHash->pList;` |
|     53850 |  194 | `	for(;;){` |
|    107705 |  195 | `		if( pHash->nEntry == 0 ){` |
|    100811 |  196 | `			break;` |
|         - |  197 | `		}` |
|      6899 |  198 | `		pNext = pEntry->pNext;` |
|      6899 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      6899 |  200 | `		pEntry = pNext;` |
|      6899 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    100811 |  203 | `	if( pHash->apBucket ){` |
|    100811 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     50403 |  205 | `	}` |
|    100811 |  206 | `	pHash->apBucket = 0;` |
|    100811 |  207 | `	pHash->nBucketSize = 0;` |
|    100811 |  208 | `	pHash->pAllocator = 0;` |
|    100811 |  209 | `	return SXRET_OK;` |
|         5 |  210 |  |
|  17150990 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  17150995 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  17150995 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  15251060 |  218 | `	for(;;){` |
|  30513370 |  219 | `		if( pEntry == 0 ){` |
|   9097383 |  220 | `			break;` |
|         - |  221 | `		}` |
|  25442545 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   8053616 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   8053617 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  13362380 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   9097383 |  229 | `	return 0;` |
|   8576010 |  230 |  |
|  17964904 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  17964909 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    814117 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  17150797 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  17150797 |  244 | `	if( pEntry == 0 ){` |
|   9097383 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   8053419 |  247 | `	return (SyHashEntry *)pEntry;` |
|   8982967 |  248 |  |
|    120678 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    120683 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     92817 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     46411 |  254 | `	}else{` |
|     27871 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    120683 |  257 | `	if( pEntry->pNextCollide ){` |
|      5039 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2519 |  259 | `	}` |
|    120683 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    120683 |  261 | `	pHash->nEntry--;` |
|    120683 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    120683 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    120683 |  268 | `	return rc;` |
|         5 |  269 |  |
|       198 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       203 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       203 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       203 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       203 |  284 | `	return rc;` |
|       104 |  285 |  |
|    120480 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  287 |  |
|    120485 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    120485 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    120485 |  296 | `	return rc;` |
|         5 |  297 |  |
|   1122092 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|   1122097 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   1122097 |  306 | `	return SXRET_OK;` |
|         5 |  307 |  |
|   7098420 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   7098425 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1121645 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   1121645 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   5976785 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   5976785 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   5976785 |  324 | `	return (SyHashEntry *)pEntry;` |
|   3549215 |  325 |  |
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
|      1995 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1985 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1985 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1985 |  344 | `		pEntry = pEntry->pNext;` |
|       993 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     28676 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  349 |  |
|     28681 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     28681 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     28681 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     28681 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3644521 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3615845 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3615845 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3615845 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3615845 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1734708 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    867340 |  371 | `		}` |
|   3615845 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3615845 |  374 | `		pEntry = pEntry->pNext;` |
|   1807925 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     28681 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     28681 |  378 | `	pHash->apBucket = apNew;` |
|     28681 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     28681 |  380 | `	return SXRET_OK;` |
|     14343 |  381 |  |
|   4811954 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         5 |  383 |  |
|   4811959 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   4811959 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4811959 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2719871 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1359902 |  389 | `	}` |
|   4811959 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   4811959 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   4811959 |  393 | `	if( pHash->nEntry == 0 ){` |
|    301751 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    150873 |  395 | `	}` |
|   4811959 |  396 | `	pHash->nEntry++;` |
|   4811959 |  397 | `	return SXRET_OK;` |
|         5 |  398 |  |
|   4811954 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   4811959 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     28681 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     28681 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     14338 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   4811959 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4811959 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   4811959 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4811959 |  421 | `	pEntry->pHash = pHash;` |
|   4811959 |  422 | `	pEntry->pKey = pKey;` |
|   4811959 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   4811959 |  424 | `	pEntry->pUserData = pUserData;` |
|   4811959 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4811959 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   4811959 |  428 | `	return rc;` |
|   2405982 |  429 |  |
|    156012 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    156017 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  439 |  |
|         - |  440 |  |
