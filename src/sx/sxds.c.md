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
|  169271492 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  169271497 |   16 | `	pSet->nSize = 0 ;` |
|  169271497 |   17 | `	pSet->nUsed = 0;` |
|  169271497 |   18 | `	pSet->nCursor = 0;` |
|  169271497 |   19 | `	pSet->eSize = ElemSize;` |
|  169271497 |   20 | `	pSet->pAllocator = pAllocator;` |
|  169271497 |   21 | `	pSet->pBase =  0;` |
|  169271497 |   22 | `	pSet->pUserData = 0;` |
|  169271497 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  384483293 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  384483298 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   22158939 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   22158939 |   33 | `		if( pSet->nSize <= 0 ){` |
|   18885367 |   34 | `			pSet->nSize = 4;` |
|    9442681 |   35 | `		}` |
|   22158939 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   22158939 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   22158939 |   40 | `		pSet->pBase = pNew;` |
|   22158939 |   41 | `		pSet->nSize <<= 1;` |
|   11079467 |   42 | `	}` |
|  384483298 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3032976594 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  384483298 |   45 | `	pSet->nUsed++;` |
|  384483298 |   46 | `	return SXRET_OK;` |
|  192241675 |   47 | `}` |
|   19124370 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   19124375 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   19124375 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   19124375 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   19124375 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   19124375 |   60 | `	pSet->nSize = nItem;` |
|   19124375 |   61 | `	return SXRET_OK;` |
|    9562190 |   62 | `}` |
|   28453471 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   28453476 |   65 | `	pSet->nUsed   = 0;` |
|   28453476 |   66 | `	pSet->nCursor = 0;` |
|   28453476 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      70828 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      70833 |   71 | `	pSet->nCursor = 0;` |
|      70833 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      75014 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      75019 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30649 |   79 | `		pSet->nCursor = 0;` |
|      30649 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      44375 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      44375 |   83 | `	if( ppEntry ){` |
|      44375 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      22185 |   85 | `	}` |
|      44375 |   86 | `	pSet->nCursor++;` |
|      44375 |   87 | `	return SXRET_OK;` |
|      37512 |   88 | `}` |
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
|    2965756 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2965761 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1199 |  103 | `		pSet->nUsed = nNewSize;` |
|        597 |  104 | `	}` |
|    2965761 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   58460890 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   58460895 |  109 | `	sxi32 rc = SXRET_OK;` |
|   58460895 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   31612303 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   15806149 |  112 | `	}` |
|   58460895 |  113 | `	pSet->pBase = 0;` |
|   58460895 |  114 | `	pSet->nUsed = 0;` |
|   58460895 |  115 | `	pSet->nCursor = 0;` |
|   58460895 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   68500976 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   68500981 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19417 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   68481569 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   68481569 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   34250493 |  126 | `}` |
|    9555298 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9555303 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2234183 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7321125 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7321125 |  135 | `	pSet->nUsed--;` |
|    7321125 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7321125 |  137 | `	return pData;` |
|    4777654 |  138 | `}` |
|   35088789 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   35088794 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   35088742 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   35088742 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   17544320 |  148 | `}` |
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
|    1857900 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1857905 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1857905 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1857905 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1857905 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1857905 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1857905 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1857905 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1857905 |  180 | `	pHash->nEntry = 0;` |
|    1857905 |  181 | `	pHash->apBucket = apNew;` |
|    1857905 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1857905 |  183 | `	return SXRET_OK;` |
|     928955 |  184 | `}` |
|     416660 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     416665 |  193 | `	pEntry = pHash->pList;` |
|     222902 |  194 | `	for(;;){` |
|     445809 |  195 | `		if( pHash->nEntry == 0 ){` |
|     416665 |  196 | `			break;` |
|          - |  197 | `		}` |
|      29149 |  198 | `		pNext = pEntry->pNext;` |
|      29149 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      29149 |  200 | `		pEntry = pNext;` |
|      29149 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     416665 |  203 | `	if( pHash->apBucket ){` |
|     416665 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     208330 |  205 | `	}` |
|     416665 |  206 | `	pHash->apBucket = 0;` |
|     416665 |  207 | `	pHash->nBucketSize = 0;` |
|     416665 |  208 | `	pHash->pAllocator = 0;` |
|     416665 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   70585049 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   70585054 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   70585054 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   63325562 |  218 | `	for(;;){` |
|  126808470 |  219 | `		if( pEntry == 0 ){` |
|   25568464 |  220 | `			break;` |
|          - |  221 | `		}` |
|  123750186 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   45020632 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   45016595 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   56223421 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   25568464 |  229 | `	return 0;` |
|   35292813 |  230 | `}` |
|   77904317 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   77904322 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    7319705 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   70584622 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   70584622 |  244 | `	if( pEntry == 0 ){` |
|   25568446 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   45016181 |  247 | `	return (SyHashEntry *)pEntry;` |
|   38952447 |  248 | `}` |
|     439372 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     439377 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     358585 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     179295 |  254 | `	}else{` |
|      80797 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     439377 |  257 | `	if( pEntry->pNextCollide ){` |
|       4618 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2307 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     439377 |  261 | `	if( pHash->pLast == pEntry ){` |
|     429387 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     214691 |  263 | `	}` |
|     439377 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     439377 |  265 | `	pHash->nEntry--;` |
|     439377 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     439377 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     439377 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        432 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        437 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        437 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        419 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        419 |  288 | `	return rc;` |
|        221 |  289 | `}` |
|     438958 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     438963 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     438963 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     438963 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3066260 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3066265 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3066265 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   23620510 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   23620515 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3065999 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3065999 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   20554521 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   20554521 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   20554521 |  328 | `	return (SyHashEntry *)pEntry;` |
|   11810260 |  329 | `}` |
|         14 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|          1 |  331 | `{` |
|          - |  332 | `	SyHashEntry_Pr *pEntry;` |
|          - |  333 | `	sxi32 rc;` |
|          - |  334 | `	sxu32 n;` |
|          - |  335 | `#if defined(UNTRUST)` |
|          - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|          - |  337 | `		return 0;` |
|          - |  338 | `	}` |
|          - |  339 | `#endif` |
|         15 |  340 | `	pEntry = pHash->pList;` |
|       4223 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4209 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4209 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4209 |  348 | `		pEntry = pEntry->pNext;` |
|       2105 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|      93136 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      93141 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      93141 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      93141 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      93141 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14780565 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14687429 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14687429 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14687429 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14687429 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    7037980 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3519261 |  375 | `		}` |
|   14687429 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14687429 |  378 | `		pEntry = pEntry->pNext;` |
|    7343717 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      93141 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      93141 |  382 | `	pHash->apBucket = apNew;` |
|      93141 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      93141 |  384 | `	return SXRET_OK;` |
|      46573 |  385 | `}` |
|   19731492 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   19731497 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   19731497 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   19731497 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   12471289 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    6235767 |  393 | `	}` |
|   19731497 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   19731497 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   19731445 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   19731497 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1044199 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1044199 |  408 | `		pHash->pLast = pEntry;` |
|     522097 |  409 | `	}` |
|   19731497 |  410 | `	pHash->nEntry++;` |
|   19731497 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   19731492 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   19731497 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      93141 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      93141 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      46568 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   19731497 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   19731497 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   19731497 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   19731497 |  435 | `	pEntry->pHash = pHash;` |
|   19731497 |  436 | `	pEntry->pKey = pKey;` |
|   19731497 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   19731497 |  438 | `	pEntry->pUserData = pUserData;` |
|   19731497 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   19731497 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   19731497 |  442 | `	return rc;` |
|    9865751 |  443 | `}` |
|   19731358 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   19731363 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     479110 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     479115 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
