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
|  15382736 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  15382738 |   16 | `	pSet->nSize = 0 ;` |
|  15382738 |   17 | `	pSet->nUsed = 0;` |
|  15382738 |   18 | `	pSet->nCursor = 0;` |
|  15382738 |   19 | `	pSet->eSize = ElemSize;` |
|  15382738 |   20 | `	pSet->pAllocator = pAllocator;` |
|  15382738 |   21 | `	pSet->pBase =  0;` |
|  15382738 |   22 | `	pSet->pUserData = 0;` |
|  15382738 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  25025382 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  25025384 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4006676 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4006676 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3888510 |   34 | `			pSet->nSize = 4;` |
|   1944254 |   35 | `		}` |
|   4006676 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4006676 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4006676 |   40 | `		pSet->pBase = pNew;` |
|   4006676 |   41 | `		pSet->nSize <<= 1;` |
|   2003337 |   42 | `	}` |
|  25025384 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 186738658 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  25025384 |   45 | `	pSet->nUsed++;` |
|  25025384 |   46 | `	return SXRET_OK;` |
|  12512715 |   47 |  |
|    931552 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    931554 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    931554 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    931554 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    931554 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    931554 |   60 | `	pSet->nSize = nItem;` |
|    931554 |   61 | `	return SXRET_OK;` |
|    465778 |   62 |  |
|   1450566 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1450568 |   65 | `	pSet->nUsed   = 0;` |
|   1450568 |   66 | `	pSet->nCursor = 0;` |
|   1450568 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     49272 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     49274 |   71 | `	pSet->nCursor = 0;` |
|     49274 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     53354 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     53356 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     20266 |   79 | `		pSet->nCursor = 0;` |
|     20266 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     33092 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     33092 |   83 | `	if( ppEntry ){` |
|     33092 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     16545 |   85 | `	}` |
|     33092 |   86 | `	pSet->nCursor++;` |
|     33092 |   87 | `	return SXRET_OK;` |
|     26679 |   88 |  |
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
|    155016 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    155018 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       110 |  103 | `		pSet->nUsed = nNewSize;` |
|        54 |  104 | `	}` |
|    155018 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8735372 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8735374 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8735374 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4441726 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2220862 |  112 | `	}` |
|   8735374 |  113 | `	pSet->pBase = 0;` |
|   8735374 |  114 | `	pSet->nUsed = 0;` |
|   8735374 |  115 | `	pSet->nCursor = 0;` |
|   8735374 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4793530 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4793532 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       108 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4793426 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4793426 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2396767 |  126 |  |
|   3340364 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3340366 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2147372 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1192996 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1192996 |  135 | `	pSet->nUsed--;` |
|   1192996 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1192996 |  137 | `	return pData;` |
|   1670184 |  138 |  |
|  11559124 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  11559126 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  11559126 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  11559126 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5779726 |  148 |  |
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
|    280760 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    280762 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    280762 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    280762 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    280762 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    280762 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    280762 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    280762 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    280762 |  180 | `	pHash->nEntry = 0;` |
|    280762 |  181 | `	pHash->apBucket = apNew;` |
|    280762 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    280762 |  183 | `	return SXRET_OK;` |
|    140382 |  184 |  |
|     83632 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     83634 |  193 | `	pEntry = pHash->pList;` |
|     44006 |  194 | `	for(;;){` |
|     88014 |  195 | `		if( pHash->nEntry == 0 ){` |
|     83634 |  196 | `			break;` |
|         - |  197 | `		}` |
|      4382 |  198 | `		pNext = pEntry->pNext;` |
|      4382 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      4382 |  200 | `		pEntry = pNext;` |
|      4382 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     83634 |  203 | `	if( pHash->apBucket ){` |
|     83634 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     41816 |  205 | `	}` |
|     83634 |  206 | `	pHash->apBucket = 0;` |
|     83634 |  207 | `	pHash->nBucketSize = 0;` |
|     83634 |  208 | `	pHash->pAllocator = 0;` |
|     83634 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  12736990 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  12736992 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  12736992 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  11494254 |  218 | `	for(;;){` |
|  23126493 |  219 | `		if( pEntry == 0 ){` |
|   7039844 |  220 | `			break;` |
|         - |  221 | `		}` |
|  18935095 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5697152 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5697150 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  10389503 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   7039844 |  229 | `	return 0;` |
|   6368761 |  230 |  |
|  13269164 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  13269166 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    532316 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  12736852 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  12736852 |  244 | `	if( pEntry == 0 ){` |
|   7039844 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5697010 |  247 | `	return (SyHashEntry *)pEntry;` |
|   6634848 |  248 |  |
|     98720 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     98722 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     75248 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     37625 |  254 | `	}else{` |
|     23476 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     98722 |  257 | `	if( pEntry->pNextCollide ){` |
|      4873 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2436 |  259 | `	}` |
|     98722 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     98722 |  261 | `	pHash->nEntry--;` |
|     98722 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     98722 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     98722 |  268 | `	return rc;` |
|         2 |  269 |  |
|       140 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       142 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       142 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       142 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       142 |  284 | `	return rc;` |
|        72 |  285 |  |
|     98580 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     98582 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     98582 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     98582 |  296 | `	return rc;` |
|         2 |  297 |  |
|    335622 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    335624 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    335624 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2698594 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2698596 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    335188 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    335188 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2363410 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2363410 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2363410 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1349299 |  325 |  |
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
|     24176 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     24178 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     24178 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     24178 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     24178 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3069778 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3045602 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3045602 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3045602 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3045602 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1455444 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    727652 |  371 | `		}` |
|   3045602 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3045602 |  374 | `		pEntry = pEntry->pNext;` |
|   1522802 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     24178 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     24178 |  378 | `	pHash->apBucket = apNew;` |
|     24178 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     24178 |  380 | `	return SXRET_OK;` |
|     12090 |  381 |  |
|   3126648 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   3126650 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   3126650 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   3126650 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2022582 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1011295 |  389 | `	}` |
|   3126650 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   3126650 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   3126650 |  393 | `	if( pHash->nEntry == 0 ){` |
|    140450 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     70224 |  395 | `	}` |
|   3126650 |  396 | `	pHash->nEntry++;` |
|   3126650 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   3126648 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   3126650 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     24178 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     24178 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     12088 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   3126650 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   3126650 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   3126650 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   3126650 |  421 | `	pEntry->pHash = pHash;` |
|   3126650 |  422 | `	pEntry->pKey = pKey;` |
|   3126650 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   3126650 |  424 | `	pEntry->pUserData = pUserData;` |
|   3126650 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   3126650 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   3126650 |  428 | `	return rc;` |
|   1563326 |  429 |  |
|    125702 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    125704 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
