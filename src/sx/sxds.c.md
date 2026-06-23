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
|  18424956 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 |  |
|  18424961 |   16 | `	pSet->nSize = 0 ;` |
|  18424961 |   17 | `	pSet->nUsed = 0;` |
|  18424961 |   18 | `	pSet->nCursor = 0;` |
|  18424961 |   19 | `	pSet->eSize = ElemSize;` |
|  18424961 |   20 | `	pSet->pAllocator = pAllocator;` |
|  18424961 |   21 | `	pSet->pBase =  0;` |
|  18424961 |   22 | `	pSet->pUserData = 0;` |
|  18424961 |   23 | `	return SXRET_OK;` |
|         5 |   24 |  |
|  30192513 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  30192518 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4464773 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4464773 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4316397 |   34 | `			pSet->nSize = 4;` |
|   2158196 |   35 | `		}` |
|   4464773 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4464773 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4464773 |   40 | `		pSet->pBase = pNew;` |
|   4464773 |   41 | `		pSet->nSize <<= 1;` |
|   2232384 |   42 | `	}` |
|  30192518 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 225735398 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  30192518 |   45 | `	pSet->nUsed++;` |
|  30192518 |   46 | `	return SXRET_OK;` |
|  15096304 |   47 |  |
|   1224222 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 |  |
|   1224227 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1224227 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1224227 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1224227 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1224227 |   60 | `	pSet->nSize = nItem;` |
|   1224227 |   61 | `	return SXRET_OK;` |
|    612116 |   62 |  |
|   1720551 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 |  |
|   1720556 |   65 | `	pSet->nUsed   = 0;` |
|   1720556 |   66 | `	pSet->nCursor = 0;` |
|   1720556 |   67 | `	return SXRET_OK;` |
|         5 |   68 |  |
|     56112 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 |  |
|     56117 |   71 | `	pSet->nCursor = 0;` |
|     56117 |   72 | `	return SXRET_OK;` |
|         5 |   73 |  |
|     60318 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     60323 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     23181 |   79 | `		pSet->nCursor = 0;` |
|     23181 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     37147 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     37147 |   83 | `	if( ppEntry ){` |
|     37147 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     18571 |   85 | `	}` |
|     37147 |   86 | `	pSet->nCursor++;` |
|     37147 |   87 | `	return SXRET_OK;` |
|     30164 |   88 |  |
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
|    207030 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 |  |
|    207035 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       118 |  103 | `		pSet->nUsed = nNewSize;` |
|        57 |  104 | `	}` |
|    207035 |  105 | `	return SXRET_OK;` |
|         5 |  106 |  |
|   9737600 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 |  |
|   9737605 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9737605 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4874791 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2437393 |  112 | `	}` |
|   9737605 |  113 | `	pSet->pBase = 0;` |
|   9737605 |  114 | `	pSet->nUsed = 0;` |
|   9737605 |  115 | `	pSet->nCursor = 0;` |
|   9737605 |  116 | `	return rc;` |
|         5 |  117 |  |
|   5529834 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5529839 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       121 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5529723 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5529723 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2764922 |  126 |  |
|   3546432 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3546437 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2175399 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1371043 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1371043 |  135 | `	pSet->nUsed--;` |
|   1371043 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1371043 |  137 | `	return pData;` |
|   1773221 |  138 |  |
|  13092574 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  13092579 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13092579 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13092579 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6546575 |  148 |  |
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
|    532060 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    532065 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    532065 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    532065 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    532065 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    532065 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    532065 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    532065 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    532065 |  180 | `	pHash->nEntry = 0;` |
|    532065 |  181 | `	pHash->apBucket = apNew;` |
|    532065 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    532065 |  183 | `	return SXRET_OK;` |
|    266035 |  184 |  |
|     98058 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     98063 |  193 | `	pEntry = pHash->pList;` |
|     52391 |  194 | `	for(;;){` |
|    104787 |  195 | `		if( pHash->nEntry == 0 ){` |
|     98063 |  196 | `			break;` |
|         - |  197 | `		}` |
|      6729 |  198 | `		pNext = pEntry->pNext;` |
|      6729 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      6729 |  200 | `		pEntry = pNext;` |
|      6729 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|     98063 |  203 | `	if( pHash->apBucket ){` |
|     98063 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     49029 |  205 | `	}` |
|     98063 |  206 | `	pHash->apBucket = 0;` |
|     98063 |  207 | `	pHash->nBucketSize = 0;` |
|     98063 |  208 | `	pHash->pAllocator = 0;` |
|     98063 |  209 | `	return SXRET_OK;` |
|         5 |  210 |  |
|  16743256 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  16743261 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  16743261 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  14844975 |  218 | `	for(;;){` |
|  29707790 |  219 | `		if( pEntry == 0 ){` |
|   8869843 |  220 | `			break;` |
|         - |  221 | `		}` |
|  24774408 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   7873422 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   7873423 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  12964534 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   8869843 |  229 | `	return 0;` |
|   8372143 |  230 |  |
|  17527526 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  17527531 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    784469 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  16743067 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  16743067 |  244 | `	if( pEntry == 0 ){` |
|   8869843 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   7873229 |  247 | `	return (SyHashEntry *)pEntry;` |
|   8764278 |  248 |  |
|    118366 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    118371 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     90921 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     45463 |  254 | `	}else{` |
|     27455 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    118371 |  257 | `	if( pEntry->pNextCollide ){` |
|      4993 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2496 |  259 | `	}` |
|    118371 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    118371 |  261 | `	pHash->nEntry--;` |
|    118371 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    118371 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    118371 |  268 | `	return rc;` |
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
|    118172 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  287 |  |
|    118177 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    118177 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    118177 |  296 | `	return rc;` |
|         5 |  297 |  |
|   1080734 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|   1080739 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   1080739 |  306 | `	return SXRET_OK;` |
|         5 |  307 |  |
|   6826002 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   6826007 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1080289 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   1080289 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   5745723 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   5745723 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   5745723 |  324 | `	return (SyHashEntry *)pEntry;` |
|   3413006 |  325 |  |
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
|      1991 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1981 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1981 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1981 |  344 | `		pEntry = pEntry->pNext;` |
|       991 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     27524 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  349 |  |
|     27529 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     27529 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     27529 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     27529 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3495913 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3468389 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3468389 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3468389 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3468389 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1663781 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    831785 |  371 | `		}` |
|   3468389 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3468389 |  374 | `		pEntry = pEntry->pNext;` |
|   1734197 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     27529 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     27529 |  378 | `	pHash->apBucket = apNew;` |
|     27529 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     27529 |  380 | `	return SXRET_OK;` |
|     13767 |  381 |  |
|   4619334 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         5 |  383 |  |
|   4619339 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   4619339 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4619339 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2605131 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1302537 |  389 | `	}` |
|   4619339 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   4619339 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   4619339 |  393 | `	if( pHash->nEntry == 0 ){` |
|    290419 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    145207 |  395 | `	}` |
|   4619339 |  396 | `	pHash->nEntry++;` |
|   4619339 |  397 | `	return SXRET_OK;` |
|         5 |  398 |  |
|   4619334 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   4619339 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     27529 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     27529 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     13762 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   4619339 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4619339 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   4619339 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4619339 |  421 | `	pEntry->pHash = pHash;` |
|   4619339 |  422 | `	pEntry->pKey = pKey;` |
|   4619339 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   4619339 |  424 | `	pEntry->pUserData = pUserData;` |
|   4619339 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4619339 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   4619339 |  428 | `	return rc;` |
|   2309672 |  429 |  |
|    152232 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    152237 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  439 |  |
|         - |  440 |  |
