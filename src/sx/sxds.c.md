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
|  19099528 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 |  |
|  19099533 |   16 | `	pSet->nSize = 0 ;` |
|  19099533 |   17 | `	pSet->nUsed = 0;` |
|  19099533 |   18 | `	pSet->nCursor = 0;` |
|  19099533 |   19 | `	pSet->eSize = ElemSize;` |
|  19099533 |   20 | `	pSet->pAllocator = pAllocator;` |
|  19099533 |   21 | `	pSet->pBase =  0;` |
|  19099533 |   22 | `	pSet->pUserData = 0;` |
|  19099533 |   23 | `	return SXRET_OK;` |
|         5 |   24 |  |
|  31361823 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  31361828 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4571633 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4571633 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4413389 |   34 | `			pSet->nSize = 4;` |
|   2206692 |   35 | `		}` |
|   4571633 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4571633 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4571633 |   40 | `		pSet->pBase = pNew;` |
|   4571633 |   41 | `		pSet->nSize <<= 1;` |
|   2285814 |   42 | `	}` |
|  31361828 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 234396404 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  31361828 |   45 | `	pSet->nUsed++;` |
|  31361828 |   46 | `	return SXRET_OK;` |
|  15680959 |   47 |  |
|   1279594 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 |  |
|   1279599 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1279599 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1279599 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1279599 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1279599 |   60 | `	pSet->nSize = nItem;` |
|   1279599 |   61 | `	return SXRET_OK;` |
|    639802 |   62 |  |
|   1794867 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 |  |
|   1794872 |   65 | `	pSet->nUsed   = 0;` |
|   1794872 |   66 | `	pSet->nCursor = 0;` |
|   1794872 |   67 | `	return SXRET_OK;` |
|         5 |   68 |  |
|     57910 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 |  |
|     57915 |   71 | `	pSet->nCursor = 0;` |
|     57915 |   72 | `	return SXRET_OK;` |
|         5 |   73 |  |
|     62116 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     62121 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     23981 |   79 | `		pSet->nCursor = 0;` |
|     23981 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     38145 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     38145 |   83 | `	if( ppEntry ){` |
|     38145 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     19070 |   85 | `	}` |
|     38145 |   86 | `	pSet->nCursor++;` |
|     38145 |   87 | `	return SXRET_OK;` |
|     31063 |   88 |  |
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
|    207666 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 |  |
|    207671 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       141 |  103 | `		pSet->nUsed = nNewSize;` |
|        68 |  104 | `	}` |
|    207671 |  105 | `	return SXRET_OK;` |
|         5 |  106 |  |
|   9957916 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 |  |
|   9957921 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9957921 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4999849 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2499922 |  112 | `	}` |
|   9957921 |  113 | `	pSet->pBase = 0;` |
|   9957921 |  114 | `	pSet->nUsed = 0;` |
|   9957921 |  115 | `	pSet->nCursor = 0;` |
|   9957921 |  116 | `	return rc;` |
|         5 |  117 |  |
|   5714884 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5714889 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       131 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5714763 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5714763 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2857447 |  126 |  |
|   3595592 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3595597 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2182167 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1413435 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1413435 |  135 | `	pSet->nUsed--;` |
|   1413435 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1413435 |  137 | `	return pData;` |
|   1797801 |  138 |  |
|  13356015 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  13356020 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13356020 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13356020 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6678329 |  148 |  |
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
|    579624 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    579629 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    579629 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    579629 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    579629 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    579629 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    579629 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    579629 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    579629 |  180 | `	pHash->nEntry = 0;` |
|    579629 |  181 | `	pHash->apBucket = apNew;` |
|    579629 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    579629 |  183 | `	return SXRET_OK;` |
|    289817 |  184 |  |
|    103570 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    103575 |  193 | `	pEntry = pHash->pList;` |
|     55498 |  194 | `	for(;;){` |
|    111001 |  195 | `		if( pHash->nEntry == 0 ){` |
|    103575 |  196 | `			break;` |
|         - |  197 | `		}` |
|      7431 |  198 | `		pNext = pEntry->pNext;` |
|      7431 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      7431 |  200 | `		pEntry = pNext;` |
|      7431 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    103575 |  203 | `	if( pHash->apBucket ){` |
|    103575 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     51785 |  205 | `	}` |
|    103575 |  206 | `	pHash->apBucket = 0;` |
|    103575 |  207 | `	pHash->nBucketSize = 0;` |
|    103575 |  208 | `	pHash->pAllocator = 0;` |
|    103575 |  209 | `	return SXRET_OK;` |
|         5 |  210 |  |
|  17428886 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  17428891 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  17428891 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  15463085 |  218 | `	for(;;){` |
|  30955428 |  219 | `		if( pEntry == 0 ){` |
|   9270337 |  220 | `			break;` |
|         - |  221 | `		}` |
|  25764120 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   8158558 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   8158559 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  13526542 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   9270337 |  229 | `	return 0;` |
|   8714958 |  230 |  |
|  18285454 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  18285459 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    856781 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  17428683 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  17428683 |  244 | `	if( pEntry == 0 ){` |
|   9270337 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   8158351 |  247 | `	return (SyHashEntry *)pEntry;` |
|   9143242 |  248 |  |
|    123774 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    123779 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     95355 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     47680 |  254 | `	}else{` |
|     28429 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    123779 |  257 | `	if( pEntry->pNextCollide ){` |
|      5133 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2566 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    123779 |  261 | `	if( pHash->pLast == pEntry ){` |
|    117489 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     58742 |  263 | `	}` |
|    123779 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    123779 |  265 | `	pHash->nEntry--;` |
|    123779 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    123779 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    123779 |  272 | `	return rc;` |
|         5 |  273 |  |
|       208 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 |  |
|         - |  276 | `	SyHashEntry_Pr *pEntry;` |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( INVALID_HASH(pHash) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|       213 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       213 |  284 | `	if( pEntry == 0 ){` |
|       ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|         - |  286 | `	}` |
|       213 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       213 |  288 | `	return rc;` |
|       109 |  289 |  |
|    123566 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 |  |
|    123571 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    123571 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    123571 |  300 | `	return rc;` |
|         5 |  301 |  |
|   1162406 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 |  |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1162411 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1162411 |  310 | `	return SXRET_OK;` |
|         5 |  311 |  |
|   7313354 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 |  |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   7313359 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1162149 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1162149 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   6151215 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   6151215 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   6151215 |  328 | `	return (SyHashEntry *)pEntry;` |
|   3656682 |  329 |  |
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
|      1987 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|      1977 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1977 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|      1977 |  348 | `		pEntry = pEntry->pNext;` |
|       989 |  349 | `	}` |
|        11 |  350 | `	return SXRET_OK;` |
|         6 |  351 |  |
|     29670 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  353 |  |
|     29675 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  355 | `	SyHashEntry_Pr *pEntry;` |
|         - |  356 | `	SyHashEntry_Pr **apNew;` |
|         - |  357 | `	sxu32 n,iBucket;` |
|         - |  358 |  |
|         - |  359 | `	/* Allocate a new larger table */` |
|     29675 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     29675 |  361 | `	if( apNew == 0 ){` |
|         - |  362 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  363 | `		return SXRET_OK;` |
|         - |  364 | `	}` |
|         - |  365 | `	/* Zero the new table */` |
|     29675 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  367 | `	/* Rehash all entries */` |
|   3771179 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3741509 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  370 | `		/* Install in the new bucket */` |
|   3741509 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3741509 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3741509 |  373 | `		if( apNew[iBucket] != 0 ){` |
|   1794814 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    897335 |  375 | `		}` |
|   3741509 |  376 | `		apNew[iBucket] = pEntry;` |
|         - |  377 | `		/* Point to the next entry */` |
|   3741509 |  378 | `		pEntry = pEntry->pNext;` |
|   1870757 |  379 | `	}` |
|         - |  380 | `	/* Release the old table and reflect the change */` |
|     29675 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     29675 |  382 | `	pHash->apBucket = apNew;` |
|     29675 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     29675 |  384 | `	return SXRET_OK;` |
|     14840 |  385 |  |
|   4945340 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 |  |
|   4945345 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   4945345 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4945345 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2796591 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1398292 |  393 | `	}` |
|   4945345 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   4945345 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        33 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        33 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        33 |  401 | `		pHash->pLast = pEntry;` |
|        17 |  402 | `	}else{` |
|   4945313 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   4945345 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    311639 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    311639 |  408 | `		pHash->pLast = pEntry;` |
|    155817 |  409 | `	}` |
|   4945345 |  410 | `	pHash->nEntry++;` |
|   4945345 |  411 | `	return SXRET_OK;` |
|         5 |  412 |  |
|   4945340 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 |  |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   4945345 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     29675 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     29675 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     14835 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   4945345 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4945345 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   4945345 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4945345 |  435 | `	pEntry->pHash = pHash;` |
|   4945345 |  436 | `	pEntry->pKey = pKey;` |
|   4945345 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   4945345 |  438 | `	pEntry->pUserData = pUserData;` |
|   4945345 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4945345 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   4945345 |  442 | `	return rc;` |
|   2472675 |  443 |  |
|   4945270 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 |  |
|   4945275 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|         5 |  447 |  |
|         - |  448 | `/*` |
|         - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|         - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|         - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|         - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|         - |  453 | ` */` |
|        70 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  455 |  |
|        72 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|         2 |  457 |  |
|    160290 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 |  |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    160295 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 |  |
|         - |  468 |  |
