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
| 134135896 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 | `{` |
| 134135901 |   16 | `	pSet->nSize = 0 ;` |
| 134135901 |   17 | `	pSet->nUsed = 0;` |
| 134135901 |   18 | `	pSet->nCursor = 0;` |
| 134135901 |   19 | `	pSet->eSize = ElemSize;` |
| 134135901 |   20 | `	pSet->pAllocator = pAllocator;` |
| 134135901 |   21 | `	pSet->pBase =  0;` |
| 134135901 |   22 | `	pSet->pUserData = 0;` |
| 134135901 |   23 | `	return SXRET_OK;` |
|         5 |   24 | `}` |
|  87858493 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 | `{` |
|         - |   27 | `	unsigned char *zbase;` |
|  87858498 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|  15790912 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|  15790912 |   33 | `		if( pSet->nSize <= 0 ){` |
|  15379289 |   34 | `			pSet->nSize = 4;` |
|   7691608 |   35 | `		}` |
|  15790912 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|  15790912 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|  15790912 |   40 | `		pSet->pBase = pNew;` |
|  15790912 |   41 | `		pSet->nSize <<= 1;` |
|   7897419 |   42 | `	}` |
|  87858498 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 586826142 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  87858498 |   45 | `	pSet->nUsed++;` |
|  87858498 |   46 | `	return SXRET_OK;` |
|  43935975 |   47 | `}` |
|   7621326 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 | `{` |
|   7621331 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   7621331 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   7621331 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   7621331 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   7621331 |   60 | `	pSet->nSize = nItem;` |
|   7621331 |   61 | `	return SXRET_OK;` |
|   3810668 |   62 | `}` |
|   6586094 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 | `{` |
|   6586099 |   65 | `	pSet->nUsed   = 0;` |
|   6586099 |   66 | `	pSet->nCursor = 0;` |
|   6586099 |   67 | `	return SXRET_OK;` |
|         5 |   68 | `}` |
|    127104 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 | `{` |
|    127109 |   71 | `	pSet->nCursor = 0;` |
|    127109 |   72 | `	return SXRET_OK;` |
|         5 |   73 | `}` |
|    129280 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 | `{` |
|         - |   76 | `	register unsigned char *zSrc;` |
|    129285 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     58433 |   79 | `		pSet->nCursor = 0;` |
|     58433 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     70857 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     70857 |   83 | `	if( ppEntry ){` |
|     70857 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     35426 |   85 | `	}` |
|     70857 |   86 | `	pSet->nCursor++;` |
|     70857 |   87 | `	return SXRET_OK;` |
|     64645 |   88 | `}` |
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
|    159348 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 | `{` |
|    159353 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|      2179 |  103 | `		pSet->nUsed = nNewSize;` |
|      1087 |  104 | `	}` |
|    159353 |  105 | `	return SXRET_OK;` |
|         5 |  106 | `}` |
|  58741270 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 | `{` |
|  58741275 |  109 | `	sxi32 rc = SXRET_OK;` |
|  58741275 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|  16730002 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   8366973 |  112 | `	}` |
|  58741275 |  113 | `	pSet->pBase = 0;` |
|  58741275 |  114 | `	pSet->nUsed = 0;` |
|  58741275 |  115 | `	pSet->nCursor = 0;` |
|  58741275 |  116 | `	return rc;` |
|         5 |  117 | `}` |
|  14326854 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 | `{` |
|         - |  120 | `	const char *zBase;` |
|  14326859 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       132 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|  14326731 |  124 | `	zBase = (const char *)pSet->pBase;` |
|  14326731 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   7163418 |  126 | `}` |
|  22362139 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 | `{` |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|  22362144 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   3018948 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|  19343201 |  134 | `	zBase = (const char *)pSet->pBase;` |
|  19343201 |  135 | `	pSet->nUsed--;` |
|  19343201 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|  19343201 |  137 | `	return pData;` |
|  11182393 |  138 | `}` |
|  69875617 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	const char *zBase;` |
|  69875622 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       113 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  69875512 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  69875512 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|  34949727 |  148 | `}` |
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
|   8928856 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 | `{` |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|   8928861 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|   8928861 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|   8928861 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|   8928861 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|   8928861 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|   8928861 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|   8928861 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|   8928861 |  180 | `	pHash->nEntry = 0;` |
|   8928861 |  181 | `	pHash->apBucket = apNew;` |
|   8928861 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|   8928861 |  183 | `	return SXRET_OK;` |
|   4464649 |  184 | `}` |
|   5269294 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 | `{` |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|   5269299 |  193 | `	pEntry = pHash->pList;` |
|   8060244 |  194 | `	for(;;){` |
|  16117455 |  195 | `		if( pHash->nEntry == 0 ){` |
|   5269299 |  196 | `			break;` |
|         - |  197 | `		}` |
|  10848161 |  198 | `		pNext = pEntry->pNext;` |
|  10848161 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|  10848161 |  200 | `		pEntry = pNext;` |
|  10848161 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|   5269299 |  203 | `	if( pHash->apBucket ){` |
|   5269299 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|   2634863 |  205 | `	}` |
|   5269299 |  206 | `	pHash->apBucket = 0;` |
|   5269299 |  207 | `	pHash->nBucketSize = 0;` |
|   5269299 |  208 | `	pHash->pAllocator = 0;` |
|   5269299 |  209 | `	return SXRET_OK;` |
|         5 |  210 | `}` |
| 117587325 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 | `{` |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
| 117587330 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
| 117587330 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
| 108104984 |  218 | `	for(;;){` |
| 216613026 |  219 | `		if( pEntry == 0 ){` |
|  47325271 |  220 | `			break;` |
|         - |  221 | `		}` |
| 204656981 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|  70750997 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|  70262064 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  99025701 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|  47325271 |  229 | `	return 0;` |
|  58812795 |  230 | `}` |
| 116169204 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 | `{` |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
| 116169209 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|   5401239 |  241 | `		return 0;` |
|         - |  242 | `	}` |
| 110767975 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
| 110767975 |  244 | `	if( pEntry == 0 ){` |
|  47318665 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|  63449315 |  247 | `	return (SyHashEntry *)pEntry;` |
|  58103954 |  248 | `}` |
|   6819529 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 | `{` |
|         - |  251 | `	sxi32 rc;` |
|   6819534 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|   6809069 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|   3404533 |  254 | `	}else{` |
|     10470 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|   6819534 |  257 | `	if( pEntry->pNextCollide ){` |
|   6474294 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|   3238411 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|   6819534 |  261 | `	if( pHash->pLast == pEntry ){` |
|     45151 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     22573 |  263 | `	}` |
|   6819534 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|   6819534 |  265 | `	pHash->nEntry--;` |
|   6819534 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|        76 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        37 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|   6819534 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|   6819534 |  272 | `	return rc;` |
|         5 |  273 | `}` |
|   6819355 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 | `{` |
|         - |  276 | `	SyHashEntry_Pr *pEntry;` |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( INVALID_HASH(pHash) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|   6819360 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   6819360 |  284 | `	if( pEntry == 0 ){` |
|      6608 |  285 | `		return SXERR_NOTFOUND;` |
|         - |  286 | `	}` |
|   6812754 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|   6812754 |  288 | `	return rc;` |
|   3409680 |  289 | `}` |
|      6780 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 | `{` |
|      6785 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|      6785 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|      6785 |  300 | `	return rc;` |
|         5 |  301 | `}` |
|  10519908 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 | `{` |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|  10519913 |  309 | `	pHash->pCurrent = pHash->pList;` |
|  10519913 |  310 | `	return SXRET_OK;` |
|         5 |  311 | `}` |
|  80027060 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 | `{` |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|  80027065 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|  10519605 |  321 | `		pHash->pCurrent = pHash->pList;` |
|  10519605 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|  69507465 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|  69507465 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|  69507465 |  328 | `	return (SyHashEntry *)pEntry;` |
|  40013527 |  329 | `}` |
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
|     82180 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|     82106 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|     82106 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|     82106 |  348 | `		pEntry = pEntry->pNext;` |
|     41055 |  349 | `	}` |
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
|       144 |  361 | `PH7_PRIVATE sxi32 SyHashForEachReverse(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         5 |  362 | `{` |
|         - |  363 | `	SyHashEntry_Pr *pEntry;` |
|         - |  364 | `	sxi32 rc;` |
|         - |  365 | `	sxu32 n;` |
|         - |  366 | `#if defined(UNTRUST)` |
|         - |  367 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  368 | `		return 0;` |
|         - |  369 | `	}` |
|         - |  370 | `#endif` |
|       149 |  371 | `	pEntry = pHash->pLast;` |
|    140923 |  372 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  373 | `		/* Invoke the callback */` |
|    140779 |  374 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|    140779 |  375 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  376 | `			return rc;` |
|         - |  377 | `		}` |
|         - |  378 | `		/* Point to the previous entry */` |
|    140779 |  379 | `		pEntry = pEntry->pPrev;` |
|     70392 |  380 | `	}` |
|       149 |  381 | `	return SXRET_OK;` |
|        77 |  382 | `}` |
|    142480 |  383 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  384 | `{` |
|    142485 |  385 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  386 | `	SyHashEntry_Pr *pEntry;` |
|         - |  387 | `	SyHashEntry_Pr **apNew;` |
|         - |  388 | `	sxu32 n,iBucket;` |
|         - |  389 |  |
|         - |  390 | `	/* Allocate a new larger table */` |
|    142485 |  391 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|    142485 |  392 | `	if( apNew == 0 ){` |
|         - |  393 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  394 | `		return SXRET_OK;` |
|         - |  395 | `	}` |
|         - |  396 | `	/* Zero the new table */` |
|    142485 |  397 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  398 | `	/* Rehash all entries */` |
|  26567157 |  399 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|  26424677 |  400 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  401 | `		/* Install in the new bucket */` |
|  26424677 |  402 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|  26424677 |  403 | `		pEntry->pNextCollide = apNew[iBucket];` |
|  26424677 |  404 | `		if( apNew[iBucket] != 0 ){` |
|  12760421 |  405 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|   6380161 |  406 | `		}` |
|  26424677 |  407 | `		apNew[iBucket] = pEntry;` |
|         - |  408 | `		/* Point to the next entry */` |
|  26424677 |  409 | `		pEntry = pEntry->pNext;` |
|  13212341 |  410 | `	}` |
|         - |  411 | `	/* Release the old table and reflect the change */` |
|    142485 |  412 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|    142485 |  413 | `	pHash->apBucket = apNew;` |
|    142485 |  414 | `	pHash->nBucketSize = nNewSize;` |
|    142485 |  415 | `	return SXRET_OK;` |
|     71245 |  416 | `}` |
|  53724749 |  417 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  418 | `{` |
|  53724754 |  419 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  420 | `	/* Insert the entry in its corresponding bucket */` |
|  53724754 |  421 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|  53724754 |  422 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|  28387573 |  423 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|  14195297 |  424 | `	}` |
|  53724754 |  425 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  426 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  427 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  428 | `	 * callers that need a FIFO traversal. */` |
|  53724754 |  429 | `	if( bTail && pHash->pLast != 0 ){` |
|  11456497 |  430 | `		pHash->pLast->pNext = pEntry;` |
|  11456497 |  431 | `		pEntry->pPrev = pHash->pLast;` |
|  11456497 |  432 | `		pHash->pLast = pEntry;` |
|   5728248 |  433 | `	}else{` |
|  42268262 |  434 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  435 | `	}` |
|  53724754 |  436 | `	if( pHash->nEntry == 0 ){` |
|         - |  437 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|   4008218 |  438 | `		pHash->pCurrent = pHash->pList;` |
|   4008218 |  439 | `		pHash->pLast = pEntry;` |
|   2004323 |  440 | `	}` |
|  53724754 |  441 | `	pHash->nEntry++;` |
|  53724754 |  442 | `	return SXRET_OK;` |
|         5 |  443 | `}` |
|  53724749 |  444 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  445 | `{` |
|         - |  446 | `	SyHashEntry_Pr *pEntry;` |
|         - |  447 | `	sxi32 rc;` |
|         - |  448 | `#if defined(UNTRUST)` |
|         - |  449 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  450 | `		return SXERR_CORRUPT;` |
|         - |  451 | `	}` |
|         - |  452 | `#endif` |
|  53724754 |  453 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|    142485 |  454 | `		rc = HashGrowTable(&(*pHash));` |
|    142485 |  455 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  456 | `			return rc;` |
|         - |  457 | `		}` |
|     71240 |  458 | `	}` |
|         - |  459 | `	/* Allocate a new hash entry */` |
|  53724754 |  460 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|  53724754 |  461 | `	if( pEntry == 0 ){` |
|       ! 0 |  462 | `		return SXERR_MEM;` |
|         - |  463 | `	}` |
|         - |  464 | `	/* Zero the entry */` |
|  53724754 |  465 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|  53724754 |  466 | `	pEntry->pHash = pHash;` |
|  53724754 |  467 | `	pEntry->pKey = pKey;` |
|  53724754 |  468 | `	pEntry->nKeyLen = nKeyLen;` |
|  53724754 |  469 | `	pEntry->pUserData = pUserData;` |
|  53724754 |  470 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  471 | `	/* Finally insert the entry in its corresponding bucket */` |
|  53724754 |  472 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|  53724754 |  473 | `	return rc;` |
|  26863680 |  474 | `}` |
|  40192258 |  475 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  476 | `{` |
|  40192263 |  477 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|         5 |  478 | `}` |
|         - |  479 | `/*` |
|         - |  480 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|         - |  481 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|         - |  482 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|         - |  483 | ` * object properties, where PHP preserves property-creation order.` |
|         - |  484 | ` */` |
|  13532491 |  485 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  486 | `{` |
|  13532496 |  487 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|         5 |  488 | `}` |
|   1224532 |  489 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  490 | `{` |
|         - |  491 | `#if defined(UNTRUST)` |
|         - |  492 | `	if( INVALID_HASH(pHash) ){` |
|         - |  493 | `		return 0;` |
|         - |  494 | `	}` |
|         - |  495 | `#endif` |
|         - |  496 | `	/* Last inserted entry */` |
|   1224537 |  497 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  498 | `}` |
|         - |  499 |  |
