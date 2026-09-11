# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 293/304 lines (96.38%)

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
|  161264608 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  161264613 |   16 | `	pSet->nSize = 0 ;` |
|  161264613 |   17 | `	pSet->nUsed = 0;` |
|  161264613 |   18 | `	pSet->nCursor = 0;` |
|  161264613 |   19 | `	pSet->eSize = ElemSize;` |
|  161264613 |   20 | `	pSet->pAllocator = pAllocator;` |
|  161264613 |   21 | `	pSet->pBase =  0;` |
|  161264613 |   22 | `	pSet->pUserData = 0;` |
|  161264613 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  367017959 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  367017964 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   21425341 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   21425341 |   33 | `		if( pSet->nSize <= 0 ){` |
|   18286089 |   34 | `			pSet->nSize = 4;` |
|    9143042 |   35 | `		}` |
|   21425341 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   21425341 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   21425341 |   40 | `		pSet->pBase = pNew;` |
|   21425341 |   41 | `		pSet->nSize <<= 1;` |
|   10712668 |   42 | `	}` |
|  367017964 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2895586360 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  367017964 |   45 | `	pSet->nUsed++;` |
|  367017964 |   46 | `	return SXRET_OK;` |
|  183509008 |   47 | `}` |
|   18197584 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   18197589 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   18197589 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   18197589 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   18197589 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   18197589 |   60 | `	pSet->nSize = nItem;` |
|   18197589 |   61 | `	return SXRET_OK;` |
|    9098797 |   62 | `}` |
|   27027073 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   27027078 |   65 | `	pSet->nUsed   = 0;` |
|   27027078 |   66 | `	pSet->nCursor = 0;` |
|   27027078 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      70044 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      70049 |   71 | `	pSet->nCursor = 0;` |
|      70049 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      74216 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      74221 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30299 |   79 | `		pSet->nCursor = 0;` |
|      30299 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43927 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43927 |   83 | `	if( ppEntry ){` |
|      43927 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21961 |   85 | `	}` |
|      43927 |   86 | `	pSet->nCursor++;` |
|      43927 |   87 | `	return SXRET_OK;` |
|      37113 |   88 | `}` |
|          - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|          8 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|          1 |   91 | `{` |
|          - |   92 | `	register unsigned char *zSrc;` |
|          9 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          3 |   94 | `		return 0;` |
|          - |   95 | `	}` |
|          7 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|          7 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|          5 |   98 | `}` |
|          - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    2743416 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2743421 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1199 |  103 | `		pSet->nUsed = nNewSize;` |
|        597 |  104 | `	}` |
|    2743421 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   55988276 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   55988281 |  109 | `	sxi32 rc = SXRET_OK;` |
|   55988281 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   30479623 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   15239809 |  112 | `	}` |
|   55988281 |  113 | `	pSet->pBase = 0;` |
|   55988281 |  114 | `	pSet->nUsed = 0;` |
|   55988281 |  115 | `	pSet->nCursor = 0;` |
|   55988281 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   65192384 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   65192389 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19187 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   65173207 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   65173207 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   32596197 |  126 | `}` |
|    9344660 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9344665 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2232285 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7112385 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7112385 |  135 | `	pSet->nUsed--;` |
|    7112385 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7112385 |  137 | `	return pData;` |
|    4672335 |  138 | `}` |
|   34226031 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   34226036 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   34226014 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   34226014 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   17113160 |  148 | `}` |
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
|    1767122 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1767127 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1767127 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1767127 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1767127 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1767127 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1767127 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1767127 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1767127 |  180 | `	pHash->nEntry = 0;` |
|    1767127 |  181 | `	pHash->apBucket = apNew;` |
|    1767127 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1767127 |  183 | `	return SXRET_OK;` |
|     883566 |  184 | `}` |
|     411818 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     411823 |  193 | `	pEntry = pHash->pList;` |
|     220414 |  194 | `	for(;;){` |
|     440833 |  195 | `		if( pHash->nEntry == 0 ){` |
|     411823 |  196 | `			break;` |
|          - |  197 | `		}` |
|      29015 |  198 | `		pNext = pEntry->pNext;` |
|      29015 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      29015 |  200 | `		pEntry = pNext;` |
|      29015 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     411823 |  203 | `	if( pHash->apBucket ){` |
|     411823 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     205909 |  205 | `	}` |
|     411823 |  206 | `	pHash->apBucket = 0;` |
|     411823 |  207 | `	pHash->nBucketSize = 0;` |
|     411823 |  208 | `	pHash->pAllocator = 0;` |
|     411823 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   67000327 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   67000332 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   67000332 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   60768794 |  218 | `	for(;;){` |
|  121504165 |  219 | `		if( pEntry == 0 ){` |
|   24264154 |  220 | `			break;` |
|          - |  221 | `		}` |
|  118609951 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   42740153 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   42736183 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   54503838 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   24264154 |  229 | `	return 0;` |
|   33500452 |  230 | `}` |
|   73968691 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   73968696 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    6968821 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   66999880 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   66999880 |  244 | `	if( pEntry == 0 ){` |
|   24264136 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   42735749 |  247 | `	return (SyHashEntry *)pEntry;` |
|   36984634 |  248 | `}` |
|     433028 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     433033 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     356501 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     178253 |  254 | `	}else{` |
|      76537 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     433033 |  257 | `	if( pEntry->pNextCollide ){` |
|       4240 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2120 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     433033 |  261 | `	if( pHash->pLast == pEntry ){` |
|     423093 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     211544 |  263 | `	}` |
|     433033 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     433033 |  265 | `	pHash->nEntry--;` |
|     433033 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     433033 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     433033 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        452 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        457 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        457 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        439 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        439 |  288 | `	return rc;` |
|        231 |  289 | `}` |
|     432594 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     432599 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     432599 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     432599 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2827254 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2827259 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2827259 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   21132072 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   21132077 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2826993 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2826993 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18305089 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18305089 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18305089 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10566041 |  329 | `}` |
|         10 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|          1 |  331 | `{` |
|          - |  332 | `	SyHashEntry_Pr *pEntry;` |
|          - |  333 | `	sxi32 rc;` |
|          - |  334 | `	sxu32 n;` |
|          - |  335 | `#if defined(UNTRUST)` |
|          - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|          - |  337 | `		return 0;` |
|          - |  338 | `	}` |
|          - |  339 | `#endif` |
|         11 |  340 | `	pEntry = pHash->pList;` |
|       4071 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4061 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4061 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4061 |  348 | `		pEntry = pEntry->pNext;` |
|       2031 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      91726 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      91731 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      91731 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      91731 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      91731 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14399763 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14308037 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14308037 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14308037 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14308037 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    6857152 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3428736 |  375 | `		}` |
|   14308037 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14308037 |  378 | `		pEntry = pEntry->pNext;` |
|    7154021 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      91731 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      91731 |  382 | `	pHash->apBucket = apNew;` |
|      91731 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      91731 |  384 | `	return SXRET_OK;` |
|      45868 |  385 | `}` |
|   18426792 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   18426797 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   18426797 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   18426797 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11676507 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5838101 |  393 | `	}` |
|   18426797 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   18426797 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   18426745 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   18426797 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     974613 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     974613 |  408 | `		pHash->pLast = pEntry;` |
|     487304 |  409 | `	}` |
|   18426797 |  410 | `	pHash->nEntry++;` |
|   18426797 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   18426792 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   18426797 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      91731 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      91731 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45863 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   18426797 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   18426797 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   18426797 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   18426797 |  435 | `	pEntry->pHash = pHash;` |
|   18426797 |  436 | `	pEntry->pKey = pKey;` |
|   18426797 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   18426797 |  438 | `	pEntry->pUserData = pUserData;` |
|   18426797 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   18426797 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   18426797 |  442 | `	return rc;` |
|    9213401 |  443 | `}` |
|   18426658 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   18426663 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        134 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          2 |  455 | `{` |
|        136 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          2 |  457 | `}` |
|     472250 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     472255 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
