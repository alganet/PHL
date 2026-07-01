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
|  19763400 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 | `{` |
|  19763405 |   16 | `	pSet->nSize = 0 ;` |
|  19763405 |   17 | `	pSet->nUsed = 0;` |
|  19763405 |   18 | `	pSet->nCursor = 0;` |
|  19763405 |   19 | `	pSet->eSize = ElemSize;` |
|  19763405 |   20 | `	pSet->pAllocator = pAllocator;` |
|  19763405 |   21 | `	pSet->pBase =  0;` |
|  19763405 |   22 | `	pSet->pUserData = 0;` |
|  19763405 |   23 | `	return SXRET_OK;` |
|         5 |   24 | `}` |
|  32673029 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 | `{` |
|         - |   27 | `	unsigned char *zbase;` |
|  32673034 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4663695 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4663695 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4502177 |   34 | `			pSet->nSize = 4;` |
|   2251086 |   35 | `		}` |
|   4663695 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4663695 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4663695 |   40 | `		pSet->pBase = pNew;` |
|   4663695 |   41 | `		pSet->nSize <<= 1;` |
|   2331845 |   42 | `	}` |
|  32673034 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 244832002 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  32673034 |   45 | `	pSet->nUsed++;` |
|  32673034 |   46 | `	return SXRET_OK;` |
|  16336563 |   47 | `}` |
|   1346010 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 | `{` |
|   1346015 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1346015 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1346015 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1346015 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1346015 |   60 | `	pSet->nSize = nItem;` |
|   1346015 |   61 | `	return SXRET_OK;` |
|    673010 |   62 | `}` |
|   1856241 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 | `{` |
|   1856246 |   65 | `	pSet->nUsed   = 0;` |
|   1856246 |   66 | `	pSet->nCursor = 0;` |
|   1856246 |   67 | `	return SXRET_OK;` |
|         5 |   68 | `}` |
|     58518 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 | `{` |
|     58523 |   71 | `	pSet->nCursor = 0;` |
|     58523 |   72 | `	return SXRET_OK;` |
|         5 |   73 | `}` |
|     62722 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 | `{` |
|         - |   76 | `	register unsigned char *zSrc;` |
|     62727 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     24253 |   79 | `		pSet->nCursor = 0;` |
|     24253 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     38479 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     38479 |   83 | `	if( ppEntry ){` |
|     38479 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     19237 |   85 | `	}` |
|     38479 |   86 | `	pSet->nCursor++;` |
|     38479 |   87 | `	return SXRET_OK;` |
|     31366 |   88 | `}` |
|         - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         8 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|         1 |   91 | `{` |
|         - |   92 | `	register unsigned char *zSrc;` |
|         9 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         3 |   94 | `		return 0;` |
|         - |   95 | `	}` |
|         7 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|         7 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|         5 |   98 | `}` |
|         - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    226492 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 | `{` |
|    226497 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       141 |  103 | `		pSet->nUsed = nNewSize;` |
|        68 |  104 | `	}` |
|    226497 |  105 | `	return SXRET_OK;` |
|         5 |  106 | `}` |
|  10174248 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 | `{` |
|  10174253 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10174253 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5104353 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2552174 |  112 | `	}` |
|  10174253 |  113 | `	pSet->pBase = 0;` |
|  10174253 |  114 | `	pSet->nUsed = 0;` |
|  10174253 |  115 | `	pSet->nCursor = 0;` |
|  10174253 |  116 | `	return rc;` |
|         5 |  117 | `}` |
|   5916120 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 | `{` |
|         - |  120 | `	const char *zBase;` |
|   5916125 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       133 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5915997 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5915997 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2958065 |  126 | `}` |
|   3623634 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 | `{` |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3623639 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2183709 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1439935 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1439935 |  135 | `	pSet->nUsed--;` |
|   1439935 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1439935 |  137 | `	return pData;` |
|   1811822 |  138 | `}` |
|  13652162 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	const char *zBase;` |
|  13652167 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13652167 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13652167 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6826445 |  148 | `}` |
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
|    592810 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 | `{` |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    592815 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    592815 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    592815 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    592815 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    592815 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    592815 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    592815 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    592815 |  180 | `	pHash->nEntry = 0;` |
|    592815 |  181 | `	pHash->apBucket = apNew;` |
|    592815 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    592815 |  183 | `	return SXRET_OK;` |
|    296410 |  184 | `}` |
|    106472 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 | `{` |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    106477 |  193 | `	pEntry = pHash->pList;` |
|     57118 |  194 | `	for(;;){` |
|    114241 |  195 | `		if( pHash->nEntry == 0 ){` |
|    106477 |  196 | `			break;` |
|         - |  197 | `		}` |
|      7769 |  198 | `		pNext = pEntry->pNext;` |
|      7769 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      7769 |  200 | `		pEntry = pNext;` |
|      7769 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    106477 |  203 | `	if( pHash->apBucket ){` |
|    106477 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     53236 |  205 | `	}` |
|    106477 |  206 | `	pHash->apBucket = 0;` |
|    106477 |  207 | `	pHash->nBucketSize = 0;` |
|    106477 |  208 | `	pHash->pAllocator = 0;` |
|    106477 |  209 | `	return SXRET_OK;` |
|         5 |  210 | `}` |
|  17946528 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 | `{` |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  17946533 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  17946533 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  16043004 |  218 | `	for(;;){` |
|  32284972 |  219 | `		if( pEntry == 0 ){` |
|   9549895 |  220 | `			break;` |
|         - |  221 | `		}` |
|  26933145 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   8396648 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   8396643 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  14338444 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   9549895 |  229 | `	return 0;` |
|   8973791 |  230 | `}` |
|  18841034 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 | `{` |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  18841039 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    894721 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  17946323 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  17946323 |  244 | `	if( pEntry == 0 ){` |
|   9549895 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   8396433 |  247 | `	return (SyHashEntry *)pEntry;` |
|   9421044 |  248 | `}` |
|    132370 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 | `{` |
|         - |  251 | `	sxi32 rc;` |
|    132375 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    102519 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     51262 |  254 | `	}else{` |
|     29861 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    132375 |  257 | `	if( pEntry->pNextCollide ){` |
|      5111 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2555 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    132375 |  261 | `	if( pHash->pLast == pEntry ){` |
|    126067 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     63031 |  263 | `	}` |
|    132375 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    132375 |  265 | `	pHash->nEntry--;` |
|    132375 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    132375 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    132375 |  272 | `	return rc;` |
|         5 |  273 | `}` |
|       210 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 | `{` |
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
|       110 |  289 | `}` |
|    132160 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 | `{` |
|    132165 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    132165 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    132165 |  300 | `	return rc;` |
|         5 |  301 | `}` |
|   1192052 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 | `{` |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1192057 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1192057 |  310 | `	return SXRET_OK;` |
|         5 |  311 | `}` |
|   7562614 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 | `{` |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   7562619 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1191795 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1191795 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   6370829 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   6370829 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   6370829 |  328 | `	return (SyHashEntry *)pEntry;` |
|   3781312 |  329 | `}` |
|        10 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         1 |  331 | `{` |
|         - |  332 | `	SyHashEntry_Pr *pEntry;` |
|         - |  333 | `	sxi32 rc;` |
|         - |  334 | `	sxu32 n;` |
|         - |  335 | `#if defined(UNTRUST)` |
|         - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  337 | `		return 0;` |
|         - |  338 | `	}` |
|         - |  339 | `#endif` |
|        11 |  340 | `	pEntry = pHash->pList;` |
|      2001 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|      1991 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1991 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|      1991 |  348 | `		pEntry = pEntry->pNext;` |
|       996 |  349 | `	}` |
|        11 |  350 | `	return SXRET_OK;` |
|         6 |  351 | `}` |
|     31030 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  353 | `{` |
|     31035 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  355 | `	SyHashEntry_Pr *pEntry;` |
|         - |  356 | `	SyHashEntry_Pr **apNew;` |
|         - |  357 | `	sxu32 n,iBucket;` |
|         - |  358 |  |
|         - |  359 | `	/* Allocate a new larger table */` |
|     31035 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     31035 |  361 | `	if( apNew == 0 ){` |
|         - |  362 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  363 | `		return SXRET_OK;` |
|         - |  364 | `	}` |
|         - |  365 | `	/* Zero the new table */` |
|     31035 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  367 | `	/* Rehash all entries */` |
|   3912123 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3881093 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  370 | `		/* Install in the new bucket */` |
|   3881093 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3881093 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3881093 |  373 | `		if( apNew[iBucket] != 0 ){` |
|   1862815 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    931483 |  375 | `		}` |
|   3881093 |  376 | `		apNew[iBucket] = pEntry;` |
|         - |  377 | `		/* Point to the next entry */` |
|   3881093 |  378 | `		pEntry = pEntry->pNext;` |
|   1940549 |  379 | `	}` |
|         - |  380 | `	/* Release the old table and reflect the change */` |
|     31035 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     31035 |  382 | `	pHash->apBucket = apNew;` |
|     31035 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     31035 |  384 | `	return SXRET_OK;` |
|     15520 |  385 | `}` |
|   5116118 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 | `{` |
|   5116123 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   5116123 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   5116123 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2893595 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1446760 |  393 | `	}` |
|   5116123 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   5116123 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        51 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        51 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        51 |  401 | `		pHash->pLast = pEntry;` |
|        26 |  402 | `	}else{` |
|   5116073 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   5116123 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    319553 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    319553 |  408 | `		pHash->pLast = pEntry;` |
|    159774 |  409 | `	}` |
|   5116123 |  410 | `	pHash->nEntry++;` |
|   5116123 |  411 | `	return SXRET_OK;` |
|         5 |  412 | `}` |
|   5116118 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 | `{` |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   5116123 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     31035 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     31035 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     15515 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   5116123 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   5116123 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   5116123 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   5116123 |  435 | `	pEntry->pHash = pHash;` |
|   5116123 |  436 | `	pEntry->pKey = pKey;` |
|   5116123 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   5116123 |  438 | `	pEntry->pUserData = pUserData;` |
|   5116123 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   5116123 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   5116123 |  442 | `	return rc;` |
|   2558064 |  443 | `}` |
|   5116002 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 | `{` |
|   5116007 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|         5 |  447 | `}` |
|         - |  448 | `/*` |
|         - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|         - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|         - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|         - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|         - |  453 | ` */` |
|       116 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  455 | `{` |
|       118 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|         2 |  457 | `}` |
|    169552 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 | `{` |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    169557 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 | `}` |
|         - |  468 |  |
