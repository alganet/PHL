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
|  19095316 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 |  |
|  19095321 |   16 | `	pSet->nSize = 0 ;` |
|  19095321 |   17 | `	pSet->nUsed = 0;` |
|  19095321 |   18 | `	pSet->nCursor = 0;` |
|  19095321 |   19 | `	pSet->eSize = ElemSize;` |
|  19095321 |   20 | `	pSet->pAllocator = pAllocator;` |
|  19095321 |   21 | `	pSet->pBase =  0;` |
|  19095321 |   22 | `	pSet->pUserData = 0;` |
|  19095321 |   23 | `	return SXRET_OK;` |
|         5 |   24 |  |
|  31389143 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  31389148 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4547149 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4547149 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4390787 |   34 | `			pSet->nSize = 4;` |
|   2195391 |   35 | `		}` |
|   4547149 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4547149 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4547149 |   40 | `		pSet->pBase = pNew;` |
|   4547149 |   41 | `		pSet->nSize <<= 1;` |
|   2273572 |   42 | `	}` |
|  31389148 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 234700540 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  31389148 |   45 | `	pSet->nUsed++;` |
|  31389148 |   46 | `	return SXRET_OK;` |
|  15694619 |   47 |  |
|   1291808 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 |  |
|   1291813 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1291813 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1291813 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1291813 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1291813 |   60 | `	pSet->nSize = nItem;` |
|   1291813 |   61 | `	return SXRET_OK;` |
|    645909 |   62 |  |
|   1788747 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 |  |
|   1788752 |   65 | `	pSet->nUsed   = 0;` |
|   1788752 |   66 | `	pSet->nCursor = 0;` |
|   1788752 |   67 | `	return SXRET_OK;` |
|         5 |   68 |  |
|     57296 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 |  |
|     57301 |   71 | `	pSet->nCursor = 0;` |
|     57301 |   72 | `	return SXRET_OK;` |
|         5 |   73 |  |
|     61502 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     61507 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     23717 |   79 | `		pSet->nCursor = 0;` |
|     23717 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     37795 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     37795 |   83 | `	if( ppEntry ){` |
|     37795 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     18895 |   85 | `	}` |
|     37795 |   86 | `	pSet->nCursor++;` |
|     37795 |   87 | `	return SXRET_OK;` |
|     30756 |   88 |  |
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
|    219032 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 |  |
|    219037 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       137 |  103 | `		pSet->nUsed = nNewSize;` |
|        66 |  104 | `	}` |
|    219037 |  105 | `	return SXRET_OK;` |
|         5 |  106 |  |
|   9950464 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 |  |
|   9950469 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9950469 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4977733 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2488864 |  112 | `	}` |
|   9950469 |  113 | `	pSet->pBase = 0;` |
|   9950469 |  114 | `	pSet->nUsed = 0;` |
|   9950469 |  115 | `	pSet->nCursor = 0;` |
|   9950469 |  116 | `	return rc;` |
|         5 |  117 |  |
|   5721700 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5721705 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       131 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5721579 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5721579 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2860855 |  126 |  |
|   3578358 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3578363 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2180357 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1398011 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1398011 |  135 | `	pSet->nUsed--;` |
|   1398011 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1398011 |  137 | `	return pData;` |
|   1789184 |  138 |  |
|  13290455 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  13290460 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13290460 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13290460 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6645587 |  148 |  |
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
|    561214 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    561219 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    561219 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    561219 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    561219 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    561219 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    561219 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    561219 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    561219 |  180 | `	pHash->nEntry = 0;` |
|    561219 |  181 | `	pHash->apBucket = apNew;` |
|    561219 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    561219 |  183 | `	return SXRET_OK;` |
|    280612 |  184 |  |
|    101958 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    101963 |  193 | `	pEntry = pHash->pList;` |
|     54522 |  194 | `	for(;;){` |
|    109049 |  195 | `		if( pHash->nEntry == 0 ){` |
|    101963 |  196 | `			break;` |
|         - |  197 | `		}` |
|      7091 |  198 | `		pNext = pEntry->pNext;` |
|      7091 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      7091 |  200 | `		pEntry = pNext;` |
|      7091 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    101963 |  203 | `	if( pHash->apBucket ){` |
|    101963 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     50979 |  205 | `	}` |
|    101963 |  206 | `	pHash->apBucket = 0;` |
|    101963 |  207 | `	pHash->nBucketSize = 0;` |
|    101963 |  208 | `	pHash->pAllocator = 0;` |
|    101963 |  209 | `	return SXRET_OK;` |
|         5 |  210 |  |
|  17340206 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  17340211 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  17340211 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  15557809 |  218 | `	for(;;){` |
|  30914047 |  219 | `		if( pEntry == 0 ){` |
|   9201655 |  220 | `			break;` |
|         - |  221 | `		}` |
|  25781422 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   8138560 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   8138561 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  13573841 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   9201655 |  229 | `	return 0;` |
|   8670618 |  230 |  |
|  18168654 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  18168659 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    828655 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  17340009 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  17340009 |  244 | `	if( pEntry == 0 ){` |
|   9201655 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   8138359 |  247 | `	return (SyHashEntry *)pEntry;` |
|   9084842 |  248 |  |
|    121952 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    121957 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     93899 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     46952 |  254 | `	}else{` |
|     28063 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    121957 |  257 | `	if( pEntry->pNextCollide ){` |
|      5039 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2519 |  259 | `	}` |
|    121957 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    121957 |  261 | `	pHash->nEntry--;` |
|    121957 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    121957 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    121957 |  268 | `	return rc;` |
|         5 |  269 |  |
|       202 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       207 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       207 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       207 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       207 |  284 | `	return rc;` |
|       106 |  285 |  |
|    121750 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  287 |  |
|    121755 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    121755 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    121755 |  296 | `	return rc;` |
|         5 |  297 |  |
|   1140194 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|   1140199 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   1140199 |  306 | `	return SXRET_OK;` |
|         5 |  307 |  |
|   7208232 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   7208237 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1139747 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   1139747 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   6068495 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   6068495 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   6068495 |  324 | `	return (SyHashEntry *)pEntry;` |
|   3604121 |  325 |  |
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
|     29228 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  349 |  |
|     29233 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     29233 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     29233 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     29233 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3715153 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3685925 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3685925 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3685925 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3685925 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1768319 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    884111 |  371 | `		}` |
|   3685925 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3685925 |  374 | `		pEntry = pEntry->pNext;` |
|   1842965 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     29233 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     29233 |  378 | `	pHash->apBucket = apNew;` |
|     29233 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     29233 |  380 | `	return SXRET_OK;` |
|     14619 |  381 |  |
|   4901266 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         5 |  383 |  |
|   4901271 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   4901271 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4901271 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2771185 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1385589 |  389 | `	}` |
|   4901271 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   4901271 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   4901271 |  393 | `	if( pHash->nEntry == 0 ){` |
|    307189 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    153592 |  395 | `	}` |
|   4901271 |  396 | `	pHash->nEntry++;` |
|   4901271 |  397 | `	return SXRET_OK;` |
|         5 |  398 |  |
|   4901266 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   4901271 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     29233 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     29233 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     14614 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   4901271 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4901271 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   4901271 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4901271 |  421 | `	pEntry->pHash = pHash;` |
|   4901271 |  422 | `	pEntry->pKey = pKey;` |
|   4901271 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   4901271 |  424 | `	pEntry->pUserData = pUserData;` |
|   4901271 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4901271 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   4901271 |  428 | `	return rc;` |
|   2450638 |  429 |  |
|    157992 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    157997 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  439 |  |
|         - |  440 |  |
