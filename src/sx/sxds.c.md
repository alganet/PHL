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
|  17646860 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  17646862 |   16 | `	pSet->nSize = 0 ;` |
|  17646862 |   17 | `	pSet->nUsed = 0;` |
|  17646862 |   18 | `	pSet->nCursor = 0;` |
|  17646862 |   19 | `	pSet->eSize = ElemSize;` |
|  17646862 |   20 | `	pSet->pAllocator = pAllocator;` |
|  17646862 |   21 | `	pSet->pBase =  0;` |
|  17646862 |   22 | `	pSet->pUserData = 0;` |
|  17646862 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  28959816 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  28959818 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4322750 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4322750 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4181706 |   34 | `			pSet->nSize = 4;` |
|   2090852 |   35 | `		}` |
|   4322750 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4322750 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4322750 |   40 | `		pSet->pBase = pNew;` |
|   4322750 |   41 | `		pSet->nSize <<= 1;` |
|   2161374 |   42 | `	}` |
|  28959818 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 216815172 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  28959818 |   45 | `	pSet->nUsed++;` |
|  28959818 |   46 | `	return SXRET_OK;` |
|  14479932 |   47 |  |
|   1162858 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|   1162860 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1162860 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1162860 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1162860 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1162860 |   60 | `	pSet->nSize = nItem;` |
|   1162860 |   61 | `	return SXRET_OK;` |
|    581431 |   62 |  |
|   1644206 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1644208 |   65 | `	pSet->nUsed   = 0;` |
|   1644208 |   66 | `	pSet->nCursor = 0;` |
|   1644208 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     53984 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     53986 |   71 | `	pSet->nCursor = 0;` |
|     53986 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     58162 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     58164 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     22248 |   79 | `		pSet->nCursor = 0;` |
|     22248 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     35918 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     35918 |   83 | `	if( ppEntry ){` |
|     35918 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     17958 |   85 | `	}` |
|     35918 |   86 | `	pSet->nCursor++;` |
|     35918 |   87 | `	return SXRET_OK;` |
|     29083 |   88 |  |
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
|    196522 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    196524 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       116 |  103 | `		pSet->nUsed = nNewSize;` |
|        57 |  104 | `	}` |
|    196524 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   9453638 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   9453640 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9453640 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4735194 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2367596 |  112 | `	}` |
|   9453640 |  113 | `	pSet->pBase = 0;` |
|   9453640 |  114 | `	pSet->nUsed = 0;` |
|   9453640 |  115 | `	pSet->nCursor = 0;` |
|   9453640 |  116 | `	return rc;` |
|         2 |  117 |  |
|   5335730 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5335732 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       112 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5335622 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5335622 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2667867 |  126 |  |
|   3452232 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3452234 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2151962 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1300274 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1300274 |  135 | `	pSet->nUsed--;` |
|   1300274 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1300274 |  137 | `	return pData;` |
|   1726118 |  138 |  |
|  12596246 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  12596248 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  12596248 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  12596248 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6298262 |  148 |  |
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
|    504854 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    504856 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    504856 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    504856 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    504856 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    504856 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    504856 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    504856 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    504856 |  180 | `	pHash->nEntry = 0;` |
|    504856 |  181 | `	pHash->apBucket = apNew;` |
|    504856 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    504856 |  183 | `	return SXRET_OK;` |
|    252429 |  184 |  |
|     92976 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     92978 |  193 | `	pEntry = pHash->pList;` |
|     49628 |  194 | `	for(;;){` |
|     99258 |  195 | `		if( pHash->nEntry == 0 ){` |
|     92978 |  196 | `			break;` |
|         - |  197 | `		}` |
|      6282 |  198 | `		pNext = pEntry->pNext;` |
|      6282 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      6282 |  200 | `		pEntry = pNext;` |
|      6282 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     92978 |  203 | `	if( pHash->apBucket ){` |
|     92978 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     46488 |  205 | `	}` |
|     92978 |  206 | `	pHash->apBucket = 0;` |
|     92978 |  207 | `	pHash->nBucketSize = 0;` |
|     92978 |  208 | `	pHash->pAllocator = 0;` |
|     92978 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  15853174 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  15853176 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  15853176 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  14036482 |  218 | `	for(;;){` |
|  28247781 |  219 | `		if( pEntry == 0 ){` |
|   8345530 |  220 | `			break;` |
|         - |  221 | `		}` |
|  23655946 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   7507650 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   7507648 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  12394607 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   8345530 |  229 | `	return 0;` |
|   7926853 |  230 |  |
|  16595280 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  16595282 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    742270 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  15853014 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  15853014 |  244 | `	if( pEntry == 0 ){` |
|   8345530 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   7507486 |  247 | `	return (SyHashEntry *)pEntry;` |
|   8297906 |  248 |  |
|    111472 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    111474 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     85332 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     42667 |  254 | `	}else{` |
|     26144 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    111474 |  257 | `	if( pEntry->pNextCollide ){` |
|      4979 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2489 |  259 | `	}` |
|    111474 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    111474 |  261 | `	pHash->nEntry--;` |
|    111474 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    111474 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    111474 |  268 | `	return rc;` |
|         2 |  269 |  |
|       162 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       164 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       164 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       164 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       164 |  284 | `	return rc;` |
|        83 |  285 |  |
|    111310 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|    111312 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    111312 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    111312 |  296 | `	return rc;` |
|         2 |  297 |  |
|   1027376 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|   1027378 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   1027378 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   6404622 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   6404624 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1026942 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   1026942 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   5377684 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   5377684 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   5377684 |  324 | `	return (SyHashEntry *)pEntry;` |
|   3202313 |  325 |  |
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
|      1899 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1889 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1889 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1889 |  344 | `		pEntry = pEntry->pNext;` |
|       945 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     26200 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     26202 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     26202 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     26202 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     26202 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3329466 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3303266 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3303266 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3303266 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3303266 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1573384 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    786639 |  371 | `		}` |
|   3303266 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3303266 |  374 | `		pEntry = pEntry->pNext;` |
|   1651634 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     26202 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     26202 |  378 | `	pHash->apBucket = apNew;` |
|     26202 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     26202 |  380 | `	return SXRET_OK;` |
|     13102 |  381 |  |
|   4273488 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   4273490 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   4273490 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4273490 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2386732 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1193370 |  389 | `	}` |
|   4273490 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   4273490 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   4273490 |  393 | `	if( pHash->nEntry == 0 ){` |
|    272886 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    136442 |  395 | `	}` |
|   4273490 |  396 | `	pHash->nEntry++;` |
|   4273490 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   4273488 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   4273490 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     26202 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     26202 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     13100 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   4273490 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4273490 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   4273490 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4273490 |  421 | `	pEntry->pHash = pHash;` |
|   4273490 |  422 | `	pEntry->pKey = pKey;` |
|   4273490 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   4273490 |  424 | `	pEntry->pUserData = pUserData;` |
|   4273490 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4273490 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   4273490 |  428 | `	return rc;` |
|   2136746 |  429 |  |
|    140874 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    140876 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
