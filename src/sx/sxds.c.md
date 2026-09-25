# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 296/315 lines (93.97%)

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
| 117913043 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 | `{` |
| 117913048 |   16 | `	pSet->nSize = 0 ;` |
| 117913048 |   17 | `	pSet->nUsed = 0;` |
| 117913048 |   18 | `	pSet->nCursor = 0;` |
| 117913048 |   19 | `	pSet->eSize = ElemSize;` |
| 117913048 |   20 | `	pSet->pAllocator = pAllocator;` |
| 117913048 |   21 | `	pSet->pBase =  0;` |
| 117913048 |   22 | `	pSet->pUserData = 0;` |
| 117913048 |   23 | `	return SXRET_OK;` |
|         5 |   24 | `}` |
|  85404026 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 | `{` |
|         - |   27 | `	unsigned char *zbase;` |
|  85404031 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|  15397112 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|  15397112 |   33 | `		if( pSet->nSize <= 0 ){` |
|  14997720 |   34 | `			pSet->nSize = 4;` |
|   7500834 |   35 | `		}` |
|  15397112 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|  15397112 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|  15397112 |   40 | `		pSet->pBase = pNew;` |
|  15397112 |   41 | `		pSet->nSize <<= 1;` |
|   7700530 |   42 | `	}` |
|  85404031 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 571519807 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  85404031 |   45 | `	pSet->nUsed++;` |
|  85404031 |   46 | `	return SXRET_OK;` |
|  42708796 |   47 | `}` |
|   6367746 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 | `{` |
|   6367751 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   6367751 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   6367751 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   6367751 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   6367751 |   60 | `	pSet->nSize = nItem;` |
|   6367751 |   61 | `	return SXRET_OK;` |
|   3183878 |   62 | `}` |
|   6405862 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 | `{` |
|   6405867 |   65 | `	pSet->nUsed   = 0;` |
|   6405867 |   66 | `	pSet->nCursor = 0;` |
|   6405867 |   67 | `	return SXRET_OK;` |
|         5 |   68 | `}` |
|    121404 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 | `{` |
|    121409 |   71 | `	pSet->nCursor = 0;` |
|    121409 |   72 | `	return SXRET_OK;` |
|         5 |   73 | `}` |
|    122752 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 | `{` |
|         - |   76 | `	register unsigned char *zSrc;` |
|    122757 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     55733 |   79 | `		pSet->nCursor = 0;` |
|     55733 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     67029 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     67029 |   83 | `	if( ppEntry ){` |
|     67029 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     33512 |   85 | `	}` |
|     67029 |   86 | `	pSet->nCursor++;` |
|     67029 |   87 | `	return SXRET_OK;` |
|     61381 |   88 | `}` |
|         - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|       ! 0 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|       ! 0 |   91 | `{` |
|         - |   92 | `	register unsigned char *zSrc;` |
|       ! 0 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|       ! 0 |   94 | `		return 0;` |
|         - |   95 | `	}` |
|       ! 0 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|       ! 0 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|       ! 0 |   98 | `}` |
|         - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    154816 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 | `{` |
|    154821 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|      1583 |  103 | `		pSet->nUsed = nNewSize;` |
|       789 |  104 | `	}` |
|    154821 |  105 | `	return SXRET_OK;` |
|         5 |  106 | `}` |
|  58063151 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 | `{` |
|  58063156 |  109 | `	sxi32 rc = SXRET_OK;` |
|  58063156 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|  16344316 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   8174138 |  112 | `	}` |
|  58063156 |  113 | `	pSet->pBase = 0;` |
|  58063156 |  114 | `	pSet->nUsed = 0;` |
|  58063156 |  115 | `	pSet->nCursor = 0;` |
|  58063156 |  116 | `	return rc;` |
|         5 |  117 | `}` |
|  14009960 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 | `{` |
|         - |  120 | `	const char *zBase;` |
|  14009965 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       132 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|  14009837 |  124 | `	zBase = (const char *)pSet->pBase;` |
|  14009837 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   7004983 |  126 | `}` |
|  22087594 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 | `{` |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|  22087599 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2973069 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|  19114535 |  134 | `	zBase = (const char *)pSet->pBase;` |
|  19114535 |  135 | `	pSet->nUsed--;` |
|  19114535 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|  19114535 |  137 | `	return pData;` |
|  11045114 |  138 | `}` |
|  68323907 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	const char *zBase;` |
|  68323912 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       113 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  68323802 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  68323802 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|  34173829 |  148 | `}` |
|         - |  149 | `/* Private hash entry */` |
|         - |  150 | `struct SyHashEntry_Pr` |
|         - |  151 | `{` |
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
|   8419554 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 | `{` |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|   8419559 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|   8419559 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|   8419559 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|   8419559 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|   8419559 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|   8419559 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|   8419559 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|   8419559 |  180 | `	pHash->nEntry = 0;` |
|   8419559 |  181 | `	pHash->apBucket = apNew;` |
|   8419559 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|   8419559 |  183 | `	return SXRET_OK;` |
|   4210004 |  184 | `}` |
|   5250954 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 | `{` |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|   5250959 |  193 | `	pEntry = pHash->pList;` |
|   8028740 |  194 | `	for(;;){` |
|  16054377 |  195 | `		if( pHash->nEntry == 0 ){` |
|   5250959 |  196 | `			break;` |
|         - |  197 | `		}` |
|  10803423 |  198 | `		pNext = pEntry->pNext;` |
|  10803423 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|  10803423 |  200 | `		pEntry = pNext;` |
|  10803423 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|   5250959 |  203 | `	if( pHash->apBucket ){` |
|   5250959 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|   2625699 |  205 | `	}` |
|   5250959 |  206 | `	pHash->apBucket = 0;` |
|   5250959 |  207 | `	pHash->nBucketSize = 0;` |
|   5250959 |  208 | `	pHash->pAllocator = 0;` |
|   5250959 |  209 | `	return SXRET_OK;` |
|         5 |  210 | `}` |
| 101448097 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 | `{` |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
| 101448102 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
| 101448102 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  90135933 |  218 | `	for(;;){` |
| 179175607 |  219 | `		if( pEntry == 0 ){` |
|  37648615 |  220 | `			break;` |
|         - |  221 | `		}` |
| 173657708 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|  64278407 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|  63799492 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  77727510 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|  37648615 |  229 | `	return 0;` |
|  50743288 |  230 | `}` |
|  99491833 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 | `{` |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  99491838 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|   4841355 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  94650488 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  94650488 |  244 | `	if( pEntry == 0 ){` |
|  37643515 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|  57006978 |  247 | `	return (SyHashEntry *)pEntry;` |
|  49765378 |  248 | `}` |
|   6799206 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 | `{` |
|         - |  251 | `	sxi32 rc;` |
|   6799211 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|   6782155 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|   3390069 |  254 | `	}else{` |
|     17061 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|   6799211 |  257 | `	if( pEntry->pNextCollide ){` |
|   6446481 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|   3221698 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|   6799211 |  261 | `	if( pHash->pLast == pEntry ){` |
|     44743 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     22369 |  263 | `	}` |
|   6799211 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|   6799211 |  265 | `	pHash->nEntry--;` |
|   6799211 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|        74 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        36 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|   6799211 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|   6799211 |  272 | `	return rc;` |
|         5 |  273 | `}` |
|   6797614 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 | `{` |
|         - |  276 | `	SyHashEntry_Pr *pEntry;` |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( INVALID_HASH(pHash) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|   6797619 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   6797619 |  284 | `	if( pEntry == 0 ){` |
|      5102 |  285 | `		return SXERR_NOTFOUND;` |
|         - |  286 | `	}` |
|   6792519 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|   6792519 |  288 | `	return rc;` |
|   3398812 |  289 | `}` |
|      6692 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 | `{` |
|      6697 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|      6697 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|      6697 |  300 | `	return rc;` |
|         5 |  301 | `}` |
|   9477340 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 | `{` |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   9477345 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   9477345 |  310 | `	return SXRET_OK;` |
|         5 |  311 | `}` |
|  65574356 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 | `{` |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|  65574361 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   9477043 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   9477043 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|  56097323 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|  56097323 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|  56097323 |  328 | `	return (SyHashEntry *)pEntry;` |
|  32787183 |  329 | `}` |
|        74 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         4 |  331 | `{` |
|         - |  332 | `	SyHashEntry_Pr *pEntry;` |
|         - |  333 | `	sxi32 rc;` |
|         - |  334 | `	sxu32 n;` |
|         - |  335 | `#if defined(UNTRUST)` |
|         - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  337 | `		return 0;` |
|         - |  338 | `	}` |
|         - |  339 | `#endif` |
|        78 |  340 | `	pEntry = pHash->pList;` |
|     35990 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|     35916 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|     35916 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|     35916 |  348 | `		pEntry = pEntry->pNext;` |
|     17960 |  349 | `	}` |
|        78 |  350 | `	return SXRET_OK;` |
|        41 |  351 | `}` |
|         - |  352 | `/*` |
|         - |  353 | ` * Like SyHashForEach but walks the entries from the tail (pLast) back to the` |
|         - |  354 | ` * head via pPrev. The frame's local-variable table is built with SyHashInsert` |
|         - |  355 | ` * (head-push), so its forward pList order is reverse-insertion (LIFO); walking` |
|         - |  356 | ` * it backward yields DECLARATION order, which is what php's get_defined_vars()` |
|         - |  357 | ` * reports. Kept as its own primitive so the shared head-push insert path — and` |
|         - |  358 | ` * the SyHashLastEntry()==pList head contract every RefObj install relies on —` |
|         - |  359 | ` * stays untouched.` |
|         - |  360 | ` */` |
|       138 |  361 | `PH7_PRIVATE sxi32 SyHashForEachReverse(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         5 |  362 | `{` |
|         - |  363 | `	SyHashEntry_Pr *pEntry;` |
|         - |  364 | `	sxi32 rc;` |
|         - |  365 | `	sxu32 n;` |
|         - |  366 | `#if defined(UNTRUST)` |
|         - |  367 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  368 | `		return 0;` |
|         - |  369 | `	}` |
|         - |  370 | `#endif` |
|       143 |  371 | `	pEntry = pHash->pLast;` |
|     91129 |  372 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  373 | `		/* Invoke the callback */` |
|     90991 |  374 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|     90991 |  375 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  376 | `			return rc;` |
|         - |  377 | `		}` |
|         - |  378 | `		/* Point to the previous entry */` |
|     90991 |  379 | `		pEntry = pEntry->pPrev;` |
|     45498 |  380 | `	}` |
|       143 |  381 | `	return SXRET_OK;` |
|        74 |  382 | `}` |
|    109136 |  383 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  384 | `{` |
|    109141 |  385 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  386 | `	SyHashEntry_Pr *pEntry;` |
|         - |  387 | `	SyHashEntry_Pr **apNew;` |
|         - |  388 | `	sxu32 n,iBucket;` |
|         - |  389 |  |
|         - |  390 | `	/* Allocate a new larger table */` |
|    109141 |  391 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|    109141 |  392 | `	if( apNew == 0 ){` |
|         - |  393 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  394 | `		return SXRET_OK;` |
|         - |  395 | `	}` |
|         - |  396 | `	/* Zero the new table */` |
|    109141 |  397 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  398 | `	/* Rehash all entries */` |
|  17900341 |  399 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|  17791205 |  400 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  401 | `		/* Install in the new bucket */` |
|  17791205 |  402 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|  17791205 |  403 | `		pEntry->pNextCollide = apNew[iBucket];` |
|  17791205 |  404 | `		if( apNew[iBucket] != 0 ){` |
|   8836364 |  405 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|   4417840 |  406 | `		}` |
|  17791205 |  407 | `		apNew[iBucket] = pEntry;` |
|         - |  408 | `		/* Point to the next entry */` |
|  17791205 |  409 | `		pEntry = pEntry->pNext;` |
|   8895605 |  410 | `	}` |
|         - |  411 | `	/* Release the old table and reflect the change */` |
|    109141 |  412 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|    109141 |  413 | `	pHash->apBucket = apNew;` |
|    109141 |  414 | `	pHash->nBucketSize = nNewSize;` |
|    109141 |  415 | `	return SXRET_OK;` |
|     54573 |  416 | `}` |
|  43216740 |  417 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  418 | `{` |
|  43216745 |  419 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  420 | `	/* Insert the entry in its corresponding bucket */` |
|  43216745 |  421 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|  43216745 |  422 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|  20946687 |  423 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|  10472392 |  424 | `	}` |
|  43216745 |  425 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  426 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  427 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  428 | `	 * callers that need a FIFO traversal. */` |
|  43216745 |  429 | `	if( bTail && pHash->pLast != 0 ){` |
|  10052423 |  430 | `		pHash->pLast->pNext = pEntry;` |
|  10052423 |  431 | `		pEntry->pPrev = pHash->pLast;` |
|  10052423 |  432 | `		pHash->pLast = pEntry;` |
|   5026214 |  433 | `	}else{` |
|  33164327 |  434 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  435 | `	}` |
|  43216745 |  436 | `	if( pHash->nEntry == 0 ){` |
|         - |  437 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|   3676363 |  438 | `		pHash->pCurrent = pHash->pList;` |
|   3676363 |  439 | `		pHash->pLast = pEntry;` |
|   1838401 |  440 | `	}` |
|  43216745 |  441 | `	pHash->nEntry++;` |
|  43216745 |  442 | `	return SXRET_OK;` |
|         5 |  443 | `}` |
|  43216740 |  444 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  445 | `{` |
|         - |  446 | `	SyHashEntry_Pr *pEntry;` |
|         - |  447 | `	sxi32 rc;` |
|         - |  448 | `#if defined(UNTRUST)` |
|         - |  449 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  450 | `		return SXERR_CORRUPT;` |
|         - |  451 | `	}` |
|         - |  452 | `#endif` |
|  43216745 |  453 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|    109141 |  454 | `		rc = HashGrowTable(&(*pHash));` |
|    109141 |  455 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  456 | `			return rc;` |
|         - |  457 | `		}` |
|     54568 |  458 | `	}` |
|         - |  459 | `	/* Allocate a new hash entry */` |
|  43216745 |  460 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|  43216745 |  461 | `	if( pEntry == 0 ){` |
|       ! 0 |  462 | `		return SXERR_MEM;` |
|         - |  463 | `	}` |
|         - |  464 | `	/* Zero the entry */` |
|  43216745 |  465 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|  43216745 |  466 | `	pEntry->pHash = pHash;` |
|  43216745 |  467 | `	pEntry->pKey = pKey;` |
|  43216745 |  468 | `	pEntry->nKeyLen = nKeyLen;` |
|  43216745 |  469 | `	pEntry->pUserData = pUserData;` |
|  43216745 |  470 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  471 | `	/* Finally insert the entry in its corresponding bucket */` |
|  43216745 |  472 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|  43216745 |  473 | `	return rc;` |
|  21609707 |  474 | `}` |
|  31224372 |  475 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  476 | `{` |
|  31224377 |  477 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|         5 |  478 | `}` |
|         - |  479 | `/*` |
|         - |  480 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|         - |  481 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|         - |  482 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|         - |  483 | ` * object properties, where PHP preserves property-creation order.` |
|         - |  484 | ` */` |
|  11992368 |  485 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  486 | `{` |
|  11992373 |  487 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|         5 |  488 | `}` |
|   1201254 |  489 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  490 | `{` |
|         - |  491 | `#if defined(UNTRUST)` |
|         - |  492 | `	if( INVALID_HASH(pHash) ){` |
|         - |  493 | `		return 0;` |
|         - |  494 | `	}` |
|         - |  495 | `#endif` |
|         - |  496 | `	/* Last inserted entry */` |
|   1201259 |  497 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  498 | `}` |
|         - |  499 |  |
