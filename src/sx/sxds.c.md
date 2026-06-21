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
|  17721952 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 |  |
|  17721957 |   16 | `	pSet->nSize = 0 ;` |
|  17721957 |   17 | `	pSet->nUsed = 0;` |
|  17721957 |   18 | `	pSet->nCursor = 0;` |
|  17721957 |   19 | `	pSet->eSize = ElemSize;` |
|  17721957 |   20 | `	pSet->pAllocator = pAllocator;` |
|  17721957 |   21 | `	pSet->pBase =  0;` |
|  17721957 |   22 | `	pSet->pUserData = 0;` |
|  17721957 |   23 | `	return SXRET_OK;` |
|         5 |   24 |  |
|  29052452 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  29052457 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4337893 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4337893 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4196469 |   34 | `			pSet->nSize = 4;` |
|   2098232 |   35 | `		}` |
|   4337893 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4337893 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4337893 |   40 | `		pSet->pBase = pNew;` |
|   4337893 |   41 | `		pSet->nSize <<= 1;` |
|   2168944 |   42 | `	}` |
|  29052457 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 217398335 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  29052457 |   45 | `	pSet->nUsed++;` |
|  29052457 |   46 | `	return SXRET_OK;` |
|  14526253 |   47 |  |
|   1166724 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 |  |
|   1166729 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1166729 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1166729 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1166729 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1166729 |   60 | `	pSet->nSize = nItem;` |
|   1166729 |   61 | `	return SXRET_OK;` |
|    583367 |   62 |  |
|   1653484 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 |  |
|   1653489 |   65 | `	pSet->nUsed   = 0;` |
|   1653489 |   66 | `	pSet->nCursor = 0;` |
|   1653489 |   67 | `	return SXRET_OK;` |
|         5 |   68 |  |
|     54458 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 |  |
|     54463 |   71 | `	pSet->nCursor = 0;` |
|     54463 |   72 | `	return SXRET_OK;` |
|         5 |   73 |  |
|     58636 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     58641 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     22443 |   79 | `		pSet->nCursor = 0;` |
|     22443 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     36203 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     36203 |   83 | `	if( ppEntry ){` |
|     36203 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     18099 |   85 | `	}` |
|     36203 |   86 | `	pSet->nCursor++;` |
|     36203 |   87 | `	return SXRET_OK;` |
|     29323 |   88 |  |
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
|    197114 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 |  |
|    197119 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       118 |  103 | `		pSet->nUsed = nNewSize;` |
|        57 |  104 | `	}` |
|    197119 |  105 | `	return SXRET_OK;` |
|         5 |  106 |  |
|   9486950 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 |  |
|   9486955 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9486955 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4751489 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2375742 |  112 | `	}` |
|   9486955 |  113 | `	pSet->pBase = 0;` |
|   9486955 |  114 | `	pSet->nUsed = 0;` |
|   9486955 |  115 | `	pSet->nCursor = 0;` |
|   9486955 |  116 | `	return rc;` |
|         5 |  117 |  |
|   5346254 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5346259 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       115 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5346149 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5346149 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2673132 |  126 |  |
|   3464464 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3464469 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2152181 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1312293 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1312293 |  135 | `	pSet->nUsed--;` |
|   1312293 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1312293 |  137 | `	return pData;` |
|   1732237 |  138 |  |
|  12668654 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  12668659 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  12668659 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  12668659 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6334458 |  148 |  |
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
|    507324 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    507329 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    507329 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    507329 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    507329 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    507329 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    507329 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    507329 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    507329 |  180 | `	pHash->nEntry = 0;` |
|    507329 |  181 | `	pHash->apBucket = apNew;` |
|    507329 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    507329 |  183 | `	return SXRET_OK;` |
|    253667 |  184 |  |
|     94258 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     94263 |  193 | `	pEntry = pHash->pList;` |
|     50242 |  194 | `	for(;;){` |
|    100489 |  195 | `		if( pHash->nEntry == 0 ){` |
|     94263 |  196 | `			break;` |
|         - |  197 | `		}` |
|      6231 |  198 | `		pNext = pEntry->pNext;` |
|      6231 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      6231 |  200 | `		pEntry = pNext;` |
|      6231 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|     94263 |  203 | `	if( pHash->apBucket ){` |
|     94263 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     47129 |  205 | `	}` |
|     94263 |  206 | `	pHash->apBucket = 0;` |
|     94263 |  207 | `	pHash->nBucketSize = 0;` |
|     94263 |  208 | `	pHash->pAllocator = 0;` |
|     94263 |  209 | `	return SXRET_OK;` |
|         5 |  210 |  |
|  15934762 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  15934767 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  15934767 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  14273255 |  218 | `	for(;;){` |
|  28427023 |  219 | `		if( pEntry == 0 ){` |
|   8389677 |  220 | `			break;` |
|         - |  221 | `		}` |
|  23809766 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   7545094 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   7545095 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  12492261 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   8389677 |  229 | `	return 0;` |
|   7967650 |  230 |  |
|  16679512 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  16679517 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    744921 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  15934601 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  15934601 |  244 | `	if( pEntry == 0 ){` |
|   8389677 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   7544929 |  247 | `	return (SyHashEntry *)pEntry;` |
|   8340025 |  248 |  |
|    112400 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    112405 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     86117 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     43061 |  254 | `	}else{` |
|     26293 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    112405 |  257 | `	if( pEntry->pNextCollide ){` |
|      4937 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2468 |  259 | `	}` |
|    112405 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    112405 |  261 | `	pHash->nEntry--;` |
|    112405 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    112405 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    112405 |  268 | `	return rc;` |
|         5 |  269 |  |
|       166 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       171 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       171 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       171 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       171 |  284 | `	return rc;` |
|        88 |  285 |  |
|    112234 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  287 |  |
|    112239 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    112239 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    112239 |  296 | `	return rc;` |
|         5 |  297 |  |
|   1031610 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|   1031615 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   1031615 |  306 | `	return SXRET_OK;` |
|         5 |  307 |  |
|   6455436 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   6455441 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1031179 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   1031179 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   5424267 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   5424267 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   5424267 |  324 | `	return (SyHashEntry *)pEntry;` |
|   3227723 |  325 |  |
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
|      1909 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1899 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1899 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1899 |  344 | `		pEntry = pEntry->pNext;` |
|       950 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     26274 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  349 |  |
|     26279 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     26279 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     26279 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     26279 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3339143 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3312869 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3312869 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3312869 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3312869 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1577909 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    788981 |  371 | `		}` |
|   3312869 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3312869 |  374 | `		pEntry = pEntry->pNext;` |
|   1656437 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     26279 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     26279 |  378 | `	pHash->apBucket = apNew;` |
|     26279 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     26279 |  380 | `	return SXRET_OK;` |
|     13142 |  381 |  |
|   4294636 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         5 |  383 |  |
|   4294641 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   4294641 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4294641 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2399015 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1199506 |  389 | `	}` |
|   4294641 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   4294641 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   4294641 |  393 | `	if( pHash->nEntry == 0 ){` |
|    273833 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    136914 |  395 | `	}` |
|   4294641 |  396 | `	pHash->nEntry++;` |
|   4294641 |  397 | `	return SXRET_OK;` |
|         5 |  398 |  |
|   4294636 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   4294641 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     26279 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     26279 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     13137 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   4294641 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4294641 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   4294641 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4294641 |  421 | `	pEntry->pHash = pHash;` |
|   4294641 |  422 | `	pEntry->pKey = pKey;` |
|   4294641 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   4294641 |  424 | `	pEntry->pUserData = pUserData;` |
|   4294641 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4294641 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   4294641 |  428 | `	return rc;` |
|   2147323 |  429 |  |
|    141940 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    141945 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  439 |  |
|         - |  440 |  |
