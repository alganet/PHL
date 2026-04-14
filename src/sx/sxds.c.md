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
|  15290652 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  15290654 |   16 | `	pSet->nSize = 0 ;` |
|  15290654 |   17 | `	pSet->nUsed = 0;` |
|  15290654 |   18 | `	pSet->nCursor = 0;` |
|  15290654 |   19 | `	pSet->eSize = ElemSize;` |
|  15290654 |   20 | `	pSet->pAllocator = pAllocator;` |
|  15290654 |   21 | `	pSet->pBase =  0;` |
|  15290654 |   22 | `	pSet->pUserData = 0;` |
|  15290654 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  24847704 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  24847706 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3995168 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3995168 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3878238 |   34 | `			pSet->nSize = 4;` |
|   1939118 |   35 | `		}` |
|   3995168 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3995168 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3995168 |   40 | `		pSet->pBase = pNew;` |
|   3995168 |   41 | `		pSet->nSize <<= 1;` |
|   1997583 |   42 | `	}` |
|  24847706 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 185263112 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  24847706 |   45 | `	pSet->nUsed++;` |
|  24847706 |   46 | `	return SXRET_OK;` |
|  12423876 |   47 |  |
|    921720 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    921722 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    921722 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    921722 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    921722 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    921722 |   60 | `	pSet->nSize = nItem;` |
|    921722 |   61 | `	return SXRET_OK;` |
|    460862 |   62 |  |
|   1439016 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1439018 |   65 | `	pSet->nUsed   = 0;` |
|   1439018 |   66 | `	pSet->nCursor = 0;` |
|   1439018 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     48984 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     48986 |   71 | `	pSet->nCursor = 0;` |
|     48986 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     53066 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     53068 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     20138 |   79 | `		pSet->nCursor = 0;` |
|     20138 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     32932 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     32932 |   83 | `	if( ppEntry ){` |
|     32932 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     16465 |   85 | `	}` |
|     32932 |   86 | `	pSet->nCursor++;` |
|     32932 |   87 | `	return SXRET_OK;` |
|     26535 |   88 |  |
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
|    153294 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    153296 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       110 |  103 | `		pSet->nUsed = nNewSize;` |
|        54 |  104 | `	}` |
|    153296 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8703500 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8703502 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8703502 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4425524 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2212761 |  112 | `	}` |
|   8703502 |  113 | `	pSet->pBase = 0;` |
|   8703502 |  114 | `	pSet->nUsed = 0;` |
|   8703502 |  115 | `	pSet->nCursor = 0;` |
|   8703502 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4764684 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4764686 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       108 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4764580 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4764580 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2382344 |  126 |  |
|   3335576 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3335578 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2147068 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1188512 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1188512 |  135 | `	pSet->nUsed--;` |
|   1188512 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1188512 |  137 | `	return pData;` |
|   1667790 |  138 |  |
|  11379886 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  11379888 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  11379888 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  11379888 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5690117 |  148 |  |
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
|    277930 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    277932 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    277932 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    277932 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    277932 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    277932 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    277932 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    277932 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    277932 |  180 | `	pHash->nEntry = 0;` |
|    277932 |  181 | `	pHash->apBucket = apNew;` |
|    277932 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    277932 |  183 | `	return SXRET_OK;` |
|    138967 |  184 |  |
|     83062 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     83064 |  193 | `	pEntry = pHash->pList;` |
|     43695 |  194 | `	for(;;){` |
|     87392 |  195 | `		if( pHash->nEntry == 0 ){` |
|     83064 |  196 | `			break;` |
|         - |  197 | `		}` |
|      4330 |  198 | `		pNext = pEntry->pNext;` |
|      4330 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      4330 |  200 | `		pEntry = pNext;` |
|      4330 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     83064 |  203 | `	if( pHash->apBucket ){` |
|     83064 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     41531 |  205 | `	}` |
|     83064 |  206 | `	pHash->apBucket = 0;` |
|     83064 |  207 | `	pHash->nBucketSize = 0;` |
|     83064 |  208 | `	pHash->pAllocator = 0;` |
|     83064 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  12625846 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  12625848 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  12625848 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  11461078 |  218 | `	for(;;){` |
|  22842136 |  219 | `		if( pEntry == 0 ){` |
|   6970026 |  220 | `			break;` |
|         - |  221 | `		}` |
|  18699893 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5655826 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5655824 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  10216290 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   6970026 |  229 | `	return 0;` |
|   6313189 |  230 |  |
|  13134946 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  13134948 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    509206 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  12625744 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  12625744 |  244 | `	if( pEntry == 0 ){` |
|   6970026 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5655720 |  247 | `	return (SyHashEntry *)pEntry;` |
|   6567739 |  248 |  |
|     98026 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     98028 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     74704 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     37353 |  254 | `	}else{` |
|     23326 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     98028 |  257 | `	if( pEntry->pNextCollide ){` |
|      4847 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2423 |  259 | `	}` |
|     98028 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     98028 |  261 | `	pHash->nEntry--;` |
|     98028 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     98028 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     98028 |  268 | `	return rc;` |
|         2 |  269 |  |
|       104 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       106 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       106 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       106 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       106 |  284 | `	return rc;` |
|        54 |  285 |  |
|     97922 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     97924 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     97924 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     97924 |  296 | `	return rc;` |
|         2 |  297 |  |
|    332506 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    332508 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    332508 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2664298 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2664300 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    332072 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    332072 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2332230 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2332230 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2332230 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1332151 |  325 |  |
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
|      1773 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1763 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1763 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1763 |  344 | `		pEntry = pEntry->pNext;` |
|       882 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     23986 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     23988 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     23988 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     23988 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     23988 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3047508 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3023522 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3023522 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3023522 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3023522 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1444893 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    722487 |  371 | `		}` |
|   3023522 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3023522 |  374 | `		pEntry = pEntry->pNext;` |
|   1511762 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     23988 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     23988 |  378 | `	pHash->apBucket = apNew;` |
|     23988 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     23988 |  380 | `	return SXRET_OK;` |
|     11995 |  381 |  |
|   3099842 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   3099844 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   3099844 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   3099844 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2006822 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1003440 |  389 | `	}` |
|   3099844 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   3099844 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   3099844 |  393 | `	if( pHash->nEntry == 0 ){` |
|    138918 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     69458 |  395 | `	}` |
|   3099844 |  396 | `	pHash->nEntry++;` |
|   3099844 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   3099842 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   3099844 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     23988 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     23988 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     11993 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   3099844 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   3099844 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   3099844 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   3099844 |  421 | `	pEntry->pHash = pHash;` |
|   3099844 |  422 | `	pEntry->pKey = pKey;` |
|   3099844 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   3099844 |  424 | `	pEntry->pUserData = pUserData;` |
|   3099844 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   3099844 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   3099844 |  428 | `	return rc;` |
|   1549923 |  429 |  |
|    124862 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    124864 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
