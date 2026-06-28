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
|  19368184 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 |  |
|  19368189 |   16 | `	pSet->nSize = 0 ;` |
|  19368189 |   17 | `	pSet->nUsed = 0;` |
|  19368189 |   18 | `	pSet->nCursor = 0;` |
|  19368189 |   19 | `	pSet->eSize = ElemSize;` |
|  19368189 |   20 | `	pSet->pAllocator = pAllocator;` |
|  19368189 |   21 | `	pSet->pBase =  0;` |
|  19368189 |   22 | `	pSet->pUserData = 0;` |
|  19368189 |   23 | `	return SXRET_OK;` |
|         5 |   24 |  |
|  31855609 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  31855614 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4577787 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4577787 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4419521 |   34 | `			pSet->nSize = 4;` |
|   2209758 |   35 | `		}` |
|   4577787 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4577787 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4577787 |   40 | `		pSet->pBase = pNew;` |
|   4577787 |   41 | `		pSet->nSize <<= 1;` |
|   2288891 |   42 | `	}` |
|  31855614 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 238313598 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  31855614 |   45 | `	pSet->nUsed++;` |
|  31855614 |   46 | `	return SXRET_OK;` |
|  15927853 |   47 |  |
|   1315280 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 |  |
|   1315285 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1315285 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1315285 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1315285 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1315285 |   60 | `	pSet->nSize = nItem;` |
|   1315285 |   61 | `	return SXRET_OK;` |
|    657645 |   62 |  |
|   1811599 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 |  |
|   1811604 |   65 | `	pSet->nUsed   = 0;` |
|   1811604 |   66 | `	pSet->nCursor = 0;` |
|   1811604 |   67 | `	return SXRET_OK;` |
|         5 |   68 |  |
|     57756 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 |  |
|     57761 |   71 | `	pSet->nCursor = 0;` |
|     57761 |   72 | `	return SXRET_OK;` |
|         5 |   73 |  |
|     61962 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     61967 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     23917 |   79 | `		pSet->nCursor = 0;` |
|     23917 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     38055 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     38055 |   83 | `	if( ppEntry ){` |
|     38055 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     19025 |   85 | `	}` |
|     38055 |   86 | `	pSet->nCursor++;` |
|     38055 |   87 | `	return SXRET_OK;` |
|     30986 |   88 |  |
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
|    225474 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 |  |
|    225479 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       141 |  103 | `		pSet->nUsed = nNewSize;` |
|        68 |  104 | `	}` |
|    225479 |  105 | `	return SXRET_OK;` |
|         5 |  106 |  |
|  10048388 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 |  |
|  10048393 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10048393 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5016551 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2508273 |  112 | `	}` |
|  10048393 |  113 | `	pSet->pBase = 0;` |
|  10048393 |  114 | `	pSet->nUsed = 0;` |
|  10048393 |  115 | `	pSet->nCursor = 0;` |
|  10048393 |  116 | `	return rc;` |
|         5 |  117 |  |
|   5793538 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5793543 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       131 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5793417 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5793417 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2896774 |  126 |  |
|   3594352 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3594357 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2182193 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1412169 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1412169 |  135 | `	pSet->nUsed--;` |
|   1412169 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1412169 |  137 | `	return pData;` |
|   1797181 |  138 |  |
|  13375776 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  13375781 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13375781 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13375781 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6688247 |  148 |  |
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
|    579642 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    579647 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    579647 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    579647 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    579647 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    579647 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    579647 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    579647 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    579647 |  180 | `	pHash->nEntry = 0;` |
|    579647 |  181 | `	pHash->apBucket = apNew;` |
|    579647 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    579647 |  183 | `	return SXRET_OK;` |
|    289826 |  184 |  |
|    103412 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    103417 |  193 | `	pEntry = pHash->pList;` |
|     55373 |  194 | `	for(;;){` |
|    110751 |  195 | `		if( pHash->nEntry == 0 ){` |
|    103417 |  196 | `			break;` |
|         - |  197 | `		}` |
|      7339 |  198 | `		pNext = pEntry->pNext;` |
|      7339 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      7339 |  200 | `		pEntry = pNext;` |
|      7339 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    103417 |  203 | `	if( pHash->apBucket ){` |
|    103417 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     51706 |  205 | `	}` |
|    103417 |  206 | `	pHash->apBucket = 0;` |
|    103417 |  207 | `	pHash->nBucketSize = 0;` |
|    103417 |  208 | `	pHash->pAllocator = 0;` |
|    103417 |  209 | `	return SXRET_OK;` |
|         5 |  210 |  |
|  17545484 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  17545489 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  17545489 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  15668850 |  218 | `	for(;;){` |
|  31231861 |  219 | `		if( pEntry == 0 ){` |
|   9315881 |  220 | `			break;` |
|         - |  221 | `		}` |
|  26030530 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   8229612 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   8229613 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  13686377 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   9315881 |  229 | `	return 0;` |
|   8773269 |  230 |  |
|  18392202 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  18392207 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    846927 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  17545285 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  17545285 |  244 | `	if( pEntry == 0 ){` |
|   9315881 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   8229409 |  247 | `	return (SyHashEntry *)pEntry;` |
|   9196628 |  248 |  |
|    123632 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    123637 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     95285 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     47645 |  254 | `	}else{` |
|     28357 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    123637 |  257 | `	if( pEntry->pNextCollide ){` |
|      5153 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2576 |  259 | `	}` |
|    123637 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    123637 |  261 | `	pHash->nEntry--;` |
|    123637 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    123637 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    123637 |  268 | `	return rc;` |
|         5 |  269 |  |
|       204 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       209 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       209 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       209 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       209 |  284 | `	return rc;` |
|       107 |  285 |  |
|    123428 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  287 |  |
|    123433 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    123433 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    123433 |  296 | `	return rc;` |
|         5 |  297 |  |
|   1171316 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|   1171321 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   1171321 |  306 | `	return SXRET_OK;` |
|         5 |  307 |  |
|   7375148 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   7375153 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1170869 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   1170869 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   6204289 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   6204289 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   6204289 |  324 | `	return (SyHashEntry *)pEntry;` |
|   3687579 |  325 |  |
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
|      1997 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1987 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1987 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1987 |  344 | `		pEntry = pEntry->pNext;` |
|       994 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     29740 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  349 |  |
|     29745 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     29745 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     29745 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     29745 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3775953 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3746213 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3746213 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3746213 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3746213 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1800910 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    900390 |  371 | `		}` |
|   3746213 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3746213 |  374 | `		pEntry = pEntry->pNext;` |
|   1873109 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     29745 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     29745 |  378 | `	pHash->apBucket = apNew;` |
|     29745 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     29745 |  380 | `	return SXRET_OK;` |
|     14875 |  381 |  |
|   4993804 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         5 |  383 |  |
|   4993809 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   4993809 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4993809 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2816494 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1408246 |  389 | `	}` |
|   4993809 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   4993809 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   4993809 |  393 | `	if( pHash->nEntry == 0 ){` |
|    318833 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    159414 |  395 | `	}` |
|   4993809 |  396 | `	pHash->nEntry++;` |
|   4993809 |  397 | `	return SXRET_OK;` |
|         5 |  398 |  |
|   4993804 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   4993809 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     29745 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     29745 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     14870 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   4993809 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4993809 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   4993809 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4993809 |  421 | `	pEntry->pHash = pHash;` |
|   4993809 |  422 | `	pEntry->pKey = pKey;` |
|   4993809 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   4993809 |  424 | `	pEntry->pUserData = pUserData;` |
|   4993809 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4993809 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   4993809 |  428 | `	return rc;` |
|   2496907 |  429 |  |
|    160174 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    160179 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  439 |  |
|         - |  440 |  |
