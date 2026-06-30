# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 289/304 lines (95.07%)

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
|  19494180 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 |  |
|  19494185 |   16 | `	pSet->nSize = 0 ;` |
|  19494185 |   17 | `	pSet->nUsed = 0;` |
|  19494185 |   18 | `	pSet->nCursor = 0;` |
|  19494185 |   19 | `	pSet->eSize = ElemSize;` |
|  19494185 |   20 | `	pSet->pAllocator = pAllocator;` |
|  19494185 |   21 | `	pSet->pBase =  0;` |
|  19494185 |   22 | `	pSet->pUserData = 0;` |
|  19494185 |   23 | `	return SXRET_OK;` |
|         5 |   24 |  |
|  32190935 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  32190940 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4621015 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4621015 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4462449 |   34 | `			pSet->nSize = 4;` |
|   2231222 |   35 | `		}` |
|   4621015 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4621015 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4621015 |   40 | `		pSet->pBase = pNew;` |
|   4621015 |   41 | `		pSet->nSize <<= 1;` |
|   2310505 |   42 | `	}` |
|  32190940 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 241349224 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  32190940 |   45 | `	pSet->nUsed++;` |
|  32190940 |   46 | `	return SXRET_OK;` |
|  16095515 |   47 |  |
|   1321374 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 |  |
|   1321379 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1321379 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1321379 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1321379 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1321379 |   60 | `	pSet->nSize = nItem;` |
|   1321379 |   61 | `	return SXRET_OK;` |
|    660692 |   62 |  |
|   1825047 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 |  |
|   1825052 |   65 | `	pSet->nUsed   = 0;` |
|   1825052 |   66 | `	pSet->nCursor = 0;` |
|   1825052 |   67 | `	return SXRET_OK;` |
|         5 |   68 |  |
|     58086 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 |  |
|     58091 |   71 | `	pSet->nCursor = 0;` |
|     58091 |   72 | `	return SXRET_OK;` |
|         5 |   73 |  |
|     62292 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     62297 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     24053 |   79 | `		pSet->nCursor = 0;` |
|     24053 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     38249 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     38249 |   83 | `	if( ppEntry ){` |
|     38249 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     19122 |   85 | `	}` |
|     38249 |   86 | `	pSet->nCursor++;` |
|     38249 |   87 | `	return SXRET_OK;` |
|     31151 |   88 |  |
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
|    222238 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 |  |
|    222243 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       141 |  103 | `		pSet->nUsed = nNewSize;` |
|        68 |  104 | `	}` |
|    222243 |  105 | `	return SXRET_OK;` |
|         5 |  106 |  |
|  10075308 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 |  |
|  10075313 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10075313 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5053851 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2526923 |  112 | `	}` |
|  10075313 |  113 | `	pSet->pBase = 0;` |
|  10075313 |  114 | `	pSet->nUsed = 0;` |
|  10075313 |  115 | `	pSet->nCursor = 0;` |
|  10075313 |  116 | `	return rc;` |
|         5 |  117 |  |
|   5843654 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5843659 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       133 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5843531 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5843531 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2921832 |  126 |  |
|   3603618 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3603623 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2182307 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1421321 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1421321 |  135 | `	pSet->nUsed--;` |
|   1421321 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1421321 |  137 | `	return pData;` |
|   1801814 |  138 |  |
|  13467637 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  13467642 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13467642 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13467642 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6734180 |  148 |  |
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
|    581118 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    581123 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    581123 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    581123 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    581123 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    581123 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    581123 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    581123 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    581123 |  180 | `	pHash->nEntry = 0;` |
|    581123 |  181 | `	pHash->apBucket = apNew;` |
|    581123 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    581123 |  183 | `	return SXRET_OK;` |
|    290564 |  184 |  |
|    104074 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    104079 |  193 | `	pEntry = pHash->pList;` |
|     55763 |  194 | `	for(;;){` |
|    111531 |  195 | `		if( pHash->nEntry == 0 ){` |
|    104079 |  196 | `			break;` |
|         - |  197 | `		}` |
|      7457 |  198 | `		pNext = pEntry->pNext;` |
|      7457 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      7457 |  200 | `		pEntry = pNext;` |
|      7457 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    104079 |  203 | `	if( pHash->apBucket ){` |
|    104079 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     52037 |  205 | `	}` |
|    104079 |  206 | `	pHash->apBucket = 0;` |
|    104079 |  207 | `	pHash->nBucketSize = 0;` |
|    104079 |  208 | `	pHash->pAllocator = 0;` |
|    104079 |  209 | `	return SXRET_OK;` |
|         5 |  210 |  |
|  17599334 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  17599339 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  17599339 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  15892445 |  218 | `	for(;;){` |
|  31691810 |  219 | `		if( pEntry == 0 ){` |
|   9364589 |  220 | `			break;` |
|         - |  221 | `		}` |
|  26444348 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   8234754 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   8234755 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  14092476 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   9364589 |  229 | `	return 0;` |
|   8800182 |  230 |  |
|  18475788 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  18475793 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    876669 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  17599129 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  17599129 |  244 | `	if( pEntry == 0 ){` |
|   9364589 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   8234545 |  247 | `	return (SyHashEntry *)pEntry;` |
|   9238409 |  248 |  |
|    125108 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    125113 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     96461 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     48233 |  254 | `	}else{` |
|     28657 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    125113 |  257 | `	if( pEntry->pNextCollide ){` |
|      5099 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2549 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    125113 |  261 | `	if( pHash->pLast == pEntry ){` |
|    118805 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     59400 |  263 | `	}` |
|    125113 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    125113 |  265 | `	pHash->nEntry--;` |
|    125113 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    125113 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    125113 |  272 | `	return rc;` |
|         5 |  273 |  |
|       210 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 |  |
|         - |  276 | `	SyHashEntry_Pr *pEntry;` |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( INVALID_HASH(pHash) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|       215 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       215 |  284 | `	if( pEntry == 0 ){` |
|       ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|         - |  286 | `	}` |
|       215 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       215 |  288 | `	return rc;` |
|       110 |  289 |  |
|    124898 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 |  |
|    124903 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    124903 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    124903 |  300 | `	return rc;` |
|         5 |  301 |  |
|   1165040 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 |  |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1165045 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1165045 |  310 | `	return SXRET_OK;` |
|         5 |  311 |  |
|   7400000 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 |  |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   7400005 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1164783 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1164783 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   6235227 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   6235227 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   6235227 |  328 | `	return (SyHashEntry *)pEntry;` |
|   3700005 |  329 |  |
|        10 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         1 |  331 |  |
|         - |  332 | `	SyHashEntry_Pr *pEntry;` |
|         - |  333 | `	sxi32 rc;` |
|         - |  334 | `	sxu32 n;` |
|         - |  335 | `#if defined(UNTRUST)` |
|         - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  337 | `		return 0;` |
|         - |  338 | `	}` |
|         - |  339 | `#endif` |
|        11 |  340 | `	pEntry = pHash->pList;` |
|      1999 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|      1989 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1989 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|      1989 |  348 | `		pEntry = pEntry->pNext;` |
|       995 |  349 | `	}` |
|        11 |  350 | `	return SXRET_OK;` |
|         6 |  351 |  |
|     30526 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  353 |  |
|     30531 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  355 | `	SyHashEntry_Pr *pEntry;` |
|         - |  356 | `	SyHashEntry_Pr **apNew;` |
|         - |  357 | `	sxu32 n,iBucket;` |
|         - |  358 |  |
|         - |  359 | `	/* Allocate a new larger table */` |
|     30531 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     30531 |  361 | `	if( apNew == 0 ){` |
|         - |  362 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  363 | `		return SXRET_OK;` |
|         - |  364 | `	}` |
|         - |  365 | `	/* Zero the new table */` |
|     30531 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  367 | `	/* Rehash all entries */` |
|   3850659 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3820133 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  370 | `		/* Install in the new bucket */` |
|   3820133 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3820133 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3820133 |  373 | `		if( apNew[iBucket] != 0 ){` |
|   1833435 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    916721 |  375 | `		}` |
|   3820133 |  376 | `		apNew[iBucket] = pEntry;` |
|         - |  377 | `		/* Point to the next entry */` |
|   3820133 |  378 | `		pEntry = pEntry->pNext;` |
|   1910069 |  379 | `	}` |
|         - |  380 | `	/* Release the old table and reflect the change */` |
|     30531 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     30531 |  382 | `	pHash->apBucket = apNew;` |
|     30531 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     30531 |  384 | `	return SXRET_OK;` |
|     15268 |  385 |  |
|   5023102 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 |  |
|   5023107 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   5023107 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   5023107 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2845935 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1422991 |  393 | `	}` |
|   5023107 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   5023107 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        45 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        45 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        45 |  401 | `		pHash->pLast = pEntry;` |
|        23 |  402 | `	}else{` |
|   5023063 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   5023107 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    312581 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    312581 |  408 | `		pHash->pLast = pEntry;` |
|    156288 |  409 | `	}` |
|   5023107 |  410 | `	pHash->nEntry++;` |
|   5023107 |  411 | `	return SXRET_OK;` |
|         5 |  412 |  |
|   5023102 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 |  |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   5023107 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     30531 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     30531 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     15263 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   5023107 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   5023107 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   5023107 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   5023107 |  435 | `	pEntry->pHash = pHash;` |
|   5023107 |  436 | `	pEntry->pKey = pKey;` |
|   5023107 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   5023107 |  438 | `	pEntry->pUserData = pUserData;` |
|   5023107 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   5023107 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   5023107 |  442 | `	return rc;` |
|   2511556 |  443 |  |
|   5023004 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 |  |
|   5023009 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|         5 |  447 |  |
|         - |  448 | `/*` |
|         - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|         - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|         - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|         - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|         - |  453 | ` */` |
|        98 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  455 |  |
|       100 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|         2 |  457 |  |
|    161716 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 |  |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    161721 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 |  |
|         - |  468 |  |
