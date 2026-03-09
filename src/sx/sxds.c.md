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
|  10626288 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  10626290 |   16 | `	pSet->nSize = 0 ;` |
|  10626290 |   17 | `	pSet->nUsed = 0;` |
|  10626290 |   18 | `	pSet->nCursor = 0;` |
|  10626290 |   19 | `	pSet->eSize = ElemSize;` |
|  10626290 |   20 | `	pSet->pAllocator = pAllocator;` |
|  10626290 |   21 | `	pSet->pBase =  0;` |
|  10626290 |   22 | `	pSet->pUserData = 0;` |
|  10626290 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  16845092 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  16845094 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3383908 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3383908 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3315148 |   34 | `			pSet->nSize = 4;` |
|   1657573 |   35 | `		}` |
|   3383908 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3383908 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3383908 |   40 | `		pSet->pBase = pNew;` |
|   3383908 |   41 | `		pSet->nSize <<= 1;` |
|   1691953 |   42 | `	}` |
|  16845094 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 126381010 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  16845094 |   45 | `	pSet->nUsed++;` |
|  16845094 |   46 | `	return SXRET_OK;` |
|   8422570 |   47 |  |
|    482522 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    482524 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    482524 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    482524 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    482524 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    482524 |   60 | `	pSet->nSize = nItem;` |
|    482524 |   61 | `	return SXRET_OK;` |
|    241263 |   62 |  |
|    931730 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|    931732 |   65 | `	pSet->nUsed   = 0;` |
|    931732 |   66 | `	pSet->nCursor = 0;` |
|    931732 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     37144 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     37146 |   71 | `	pSet->nCursor = 0;` |
|     37146 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     40796 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     40798 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     14966 |   79 | `		pSet->nCursor = 0;` |
|     14966 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     25834 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     25834 |   83 | `	if( ppEntry ){` |
|     25834 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     12916 |   85 | `	}` |
|     25834 |   86 | `	pSet->nCursor++;` |
|     25834 |   87 | `	return SXRET_OK;` |
|     20400 |   88 |  |
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
|     60942 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     60944 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     60944 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   7028452 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   7028454 |  109 | `	sxi32 rc = SXRET_OK;` |
|   7028454 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3617332 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1808665 |  112 | `	}` |
|   7028454 |  113 | `	pSet->pBase = 0;` |
|   7028454 |  114 | `	pSet->nUsed = 0;` |
|   7028454 |  115 | `	pSet->nCursor = 0;` |
|   7028454 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3446134 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3446136 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3446046 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3446046 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1723069 |  126 |  |
|   3031460 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3031462 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2126176 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    905288 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    905288 |  135 | `	pSet->nUsed--;` |
|    905288 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    905288 |  137 | `	return pData;` |
|   1515732 |  138 |  |
|   9064220 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   9064222 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   9064222 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   9064222 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4532322 |  148 |  |
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
|     86316 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     86318 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     86318 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     86318 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     86318 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     86318 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     86318 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     86318 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     86318 |  180 | `	pHash->nEntry = 0;` |
|     86318 |  181 | `	pHash->apBucket = apNew;` |
|     86318 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     86318 |  183 | `	return SXRET_OK;` |
|     43160 |  184 |  |
|     10766 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     10768 |  193 | `	pEntry = pHash->pList;` |
|      6477 |  194 | `	for(;;){` |
|     12956 |  195 | `		if( pHash->nEntry == 0 ){` |
|     10768 |  196 | `			break;` |
|         - |  197 | `		}` |
|      2190 |  198 | `		pNext = pEntry->pNext;` |
|      2190 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      2190 |  200 | `		pEntry = pNext;` |
|      2190 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     10768 |  203 | `	if( pHash->apBucket ){` |
|     10768 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      5383 |  205 | `	}` |
|     10768 |  206 | `	pHash->apBucket = 0;` |
|     10768 |  207 | `	pHash->nBucketSize = 0;` |
|     10768 |  208 | `	pHash->pAllocator = 0;` |
|     10768 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   8688322 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   8688324 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   8688324 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   7506663 |  218 | `	for(;;){` |
|  14896289 |  219 | `		if( pEntry == 0 ){` |
|   4721216 |  220 | `			break;` |
|         - |  221 | `		}` |
|  12158499 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   3967112 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   3967110 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   6207967 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   4721216 |  229 | `	return 0;` |
|   4344427 |  230 |  |
|   8736902 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   8736904 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     48588 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   8688318 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   8688318 |  244 | `	if( pEntry == 0 ){` |
|   4721216 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   3967104 |  247 | `	return (SyHashEntry *)pEntry;` |
|   4368717 |  248 |  |
|     68456 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     68458 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     51340 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     25671 |  254 | `	}else{` |
|     17120 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     68458 |  257 | `	if( pEntry->pNextCollide ){` |
|      4083 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2041 |  259 | `	}` |
|     68458 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     68458 |  261 | `	pHash->nEntry--;` |
|     68458 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     68458 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     68458 |  268 | `	return rc;` |
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
|     68450 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     68452 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     68452 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     68452 |  296 | `	return rc;` |
|         2 |  297 |  |
|    124308 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    124310 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    124310 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    864202 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    864204 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    123876 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    123876 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    740330 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    740330 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    740330 |  324 | `	return (SyHashEntry *)pEntry;` |
|    432103 |  325 |  |
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
|      1589 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1579 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1579 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1579 |  344 | `		pEntry = pEntry->pNext;` |
|       790 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     12428 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     12430 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     12430 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     12430 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     12430 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1704814 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1692386 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1692386 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1692386 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1692386 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    812698 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    406341 |  371 | `		}` |
|   1692386 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1692386 |  374 | `		pEntry = pEntry->pNext;` |
|    846194 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     12430 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     12430 |  378 | `	pHash->apBucket = apNew;` |
|     12430 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     12430 |  380 | `	return SXRET_OK;` |
|      6216 |  381 |  |
|   1537436 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1537438 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1537438 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1537438 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1031590 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    515805 |  389 | `	}` |
|   1537438 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1537438 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1537438 |  393 | `	if( pHash->nEntry == 0 ){` |
|     61932 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     30965 |  395 | `	}` |
|   1537438 |  396 | `	pHash->nEntry++;` |
|   1537438 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1537436 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1537438 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     12430 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     12430 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      6214 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1537438 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1537438 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1537438 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1537438 |  421 | `	pEntry->pHash = pHash;` |
|   1537438 |  422 | `	pEntry->pKey = pKey;` |
|   1537438 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1537438 |  424 | `	pEntry->pUserData = pUserData;` |
|   1537438 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1537438 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1537438 |  428 | `	return rc;` |
|    768720 |  429 |  |
|     84106 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     84108 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
