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
|  16614330 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  16614332 |   16 | `	pSet->nSize = 0 ;` |
|  16614332 |   17 | `	pSet->nUsed = 0;` |
|  16614332 |   18 | `	pSet->nCursor = 0;` |
|  16614332 |   19 | `	pSet->eSize = ElemSize;` |
|  16614332 |   20 | `	pSet->pAllocator = pAllocator;` |
|  16614332 |   21 | `	pSet->pBase =  0;` |
|  16614332 |   22 | `	pSet->pUserData = 0;` |
|  16614332 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  27242774 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  27242776 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4142632 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4142632 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4008258 |   34 | `			pSet->nSize = 4;` |
|   2004128 |   35 | `		}` |
|   4142632 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4142632 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4142632 |   40 | `		pSet->pBase = pNew;` |
|   4142632 |   41 | `		pSet->nSize <<= 1;` |
|   2071315 |   42 | `	}` |
|  27242776 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 203083542 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  27242776 |   45 | `	pSet->nUsed++;` |
|  27242776 |   46 | `	return SXRET_OK;` |
|  13621411 |   47 |  |
|   1076810 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|   1076812 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1076812 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1076812 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1076812 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1076812 |   60 | `	pSet->nSize = nItem;` |
|   1076812 |   61 | `	return SXRET_OK;` |
|    538407 |   62 |  |
|   1574380 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1574382 |   65 | `	pSet->nUsed   = 0;` |
|   1574382 |   66 | `	pSet->nCursor = 0;` |
|   1574382 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     52008 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     52010 |   71 | `	pSet->nCursor = 0;` |
|     52010 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     56090 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     56092 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     21402 |   79 | `		pSet->nCursor = 0;` |
|     21402 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     34692 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     34692 |   83 | `	if( ppEntry ){` |
|     34692 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     17345 |   85 | `	}` |
|     34692 |   86 | `	pSet->nCursor++;` |
|     34692 |   87 | `	return SXRET_OK;` |
|     28047 |   88 |  |
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
|    187040 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    187042 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       110 |  103 | `		pSet->nUsed = nNewSize;` |
|        54 |  104 | `	}` |
|    187042 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   9141526 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   9141528 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9141528 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4619116 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2309557 |  112 | `	}` |
|   9141528 |  113 | `	pSet->pBase = 0;` |
|   9141528 |  114 | `	pSet->nUsed = 0;` |
|   9141528 |  115 | `	pSet->nCursor = 0;` |
|   9141528 |  116 | `	return rc;` |
|         2 |  117 |  |
|   5145214 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5145216 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       108 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5145110 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5145110 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2572609 |  126 |  |
|   3396572 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3396574 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2149320 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1247256 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1247256 |  135 | `	pSet->nUsed--;` |
|   1247256 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1247256 |  137 | `	return pData;` |
|   1698288 |  138 |  |
|  12022836 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  12022838 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  12022838 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  12022838 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6011554 |  148 |  |
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
|    301476 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    301478 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    301478 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    301478 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    301478 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    301478 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    301478 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    301478 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    301478 |  180 | `	pHash->nEntry = 0;` |
|    301478 |  181 | `	pHash->apBucket = apNew;` |
|    301478 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    301478 |  183 | `	return SXRET_OK;` |
|    150740 |  184 |  |
|     88758 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     88760 |  193 | `	pEntry = pHash->pList;` |
|     47191 |  194 | `	for(;;){` |
|     94384 |  195 | `		if( pHash->nEntry == 0 ){` |
|     88760 |  196 | `			break;` |
|         - |  197 | `		}` |
|      5626 |  198 | `		pNext = pEntry->pNext;` |
|      5626 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      5626 |  200 | `		pEntry = pNext;` |
|      5626 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     88760 |  203 | `	if( pHash->apBucket ){` |
|     88760 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     44379 |  205 | `	}` |
|     88760 |  206 | `	pHash->apBucket = 0;` |
|     88760 |  207 | `	pHash->nBucketSize = 0;` |
|     88760 |  208 | `	pHash->pAllocator = 0;` |
|     88760 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  13532708 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  13532710 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  13532710 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  12155329 |  218 | `	for(;;){` |
|  24314402 |  219 | `		if( pEntry == 0 ){` |
|   7388154 |  220 | `			break;` |
|         - |  221 | `		}` |
|  19998398 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   6144560 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   6144558 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  10781694 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   7388154 |  229 | `	return 0;` |
|   6766620 |  230 |  |
|  14102846 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  14102848 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    570288 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  13532562 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  13532562 |  244 | `	if( pEntry == 0 ){` |
|   7388154 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   6144410 |  247 | `	return (SyHashEntry *)pEntry;` |
|   7051689 |  248 |  |
|    107552 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    107554 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     82310 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     41156 |  254 | `	}else{` |
|     25246 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    107554 |  257 | `	if( pEntry->pNextCollide ){` |
|      4749 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2374 |  259 | `	}` |
|    107554 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    107554 |  261 | `	pHash->nEntry--;` |
|    107554 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    107554 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    107554 |  268 | `	return rc;` |
|         2 |  269 |  |
|       148 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       150 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       150 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       150 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       150 |  284 | `	return rc;` |
|        76 |  285 |  |
|    107404 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|    107406 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    107406 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    107406 |  296 | `	return rc;` |
|         2 |  297 |  |
|    376770 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    376772 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    376772 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2961948 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2961950 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    376336 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    376336 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2585616 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2585616 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2585616 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1480976 |  325 |  |
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
|      1801 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1791 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1791 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1791 |  344 | `		pEntry = pEntry->pNext;` |
|       896 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     24860 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     24862 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     24862 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     24862 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     24862 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3157054 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3132194 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3132194 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3132194 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3132194 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1497171 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    748551 |  371 | `		}` |
|   3132194 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3132194 |  374 | `		pEntry = pEntry->pNext;` |
|   1566098 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     24862 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     24862 |  378 | `	pHash->apBucket = apNew;` |
|     24862 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     24862 |  380 | `	return SXRET_OK;` |
|     12432 |  381 |  |
|   3292166 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   3292168 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   3292168 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   3292168 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2120930 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1060505 |  389 | `	}` |
|   3292168 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   3292168 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   3292168 |  393 | `	if( pHash->nEntry == 0 ){` |
|    149524 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     74761 |  395 | `	}` |
|   3292168 |  396 | `	pHash->nEntry++;` |
|   3292168 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   3292166 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   3292168 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     24862 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     24862 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     12430 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   3292168 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   3292168 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   3292168 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   3292168 |  421 | `	pEntry->pHash = pHash;` |
|   3292168 |  422 | `	pEntry->pKey = pKey;` |
|   3292168 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   3292168 |  424 | `	pEntry->pUserData = pUserData;` |
|   3292168 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   3292168 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   3292168 |  428 | `	return rc;` |
|   1646085 |  429 |  |
|    135338 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    135340 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
