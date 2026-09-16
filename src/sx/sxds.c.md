# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 286/304 lines (94.08%)

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
|  185039069 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  185039074 |   16 | `	pSet->nSize = 0 ;` |
|  185039074 |   17 | `	pSet->nUsed = 0;` |
|  185039074 |   18 | `	pSet->nCursor = 0;` |
|  185039074 |   19 | `	pSet->eSize = ElemSize;` |
|  185039074 |   20 | `	pSet->pAllocator = pAllocator;` |
|  185039074 |   21 | `	pSet->pBase =  0;` |
|  185039074 |   22 | `	pSet->pUserData = 0;` |
|  185039074 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  419344624 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  419344629 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   24226991 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   24226991 |   33 | `		if( pSet->nSize <= 0 ){` |
|   20646563 |   34 | `			pSet->nSize = 4;` |
|   10324201 |   35 | `		}` |
|   24226991 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   24226991 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   24226991 |   40 | `		pSet->pBase = pNew;` |
|   24226991 |   41 | `		pSet->nSize <<= 1;` |
|   12114415 |   42 | `	}` |
|  419344629 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3310633037 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  419344629 |   45 | `	pSet->nUsed++;` |
|  419344629 |   46 | `	return SXRET_OK;` |
|  209675446 |   47 | `}` |
|   20842550 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   20842555 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   20842555 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   20842555 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   20842555 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   20842555 |   60 | `	pSet->nSize = nItem;` |
|   20842555 |   61 | `	return SXRET_OK;` |
|   10421280 |   62 | `}` |
|   30860088 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   30860093 |   65 | `	pSet->nUsed   = 0;` |
|   30860093 |   66 | `	pSet->nCursor = 0;` |
|   30860093 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69222 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69227 |   71 | `	pSet->nCursor = 0;` |
|      69227 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      69482 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      69487 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30135 |   79 | `		pSet->nCursor = 0;` |
|      30135 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      39357 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      39357 |   83 | `	if( ppEntry ){` |
|      39357 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      19676 |   85 | `	}` |
|      39357 |   86 | `	pSet->nCursor++;` |
|      39357 |   87 | `	return SXRET_OK;` |
|      34746 |   88 | `}` |
|          - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        ! 0 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|        ! 0 |   91 | `{` |
|          - |   92 | `	register unsigned char *zSrc;` |
|        ! 0 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|        ! 0 |   94 | `		return 0;` |
|          - |   95 | `	}` |
|        ! 0 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|        ! 0 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|        ! 0 |   98 | `}` |
|          - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    3293354 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3293359 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1253 |  103 | `		pSet->nUsed = nNewSize;` |
|        624 |  104 | `	}` |
|    3293359 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   63289523 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   63289528 |  109 | `	sxi32 rc = SXRET_OK;` |
|   63289528 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   34258723 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   17130281 |  112 | `	}` |
|   63289528 |  113 | `	pSet->pBase = 0;` |
|   63289528 |  114 | `	pSet->nUsed = 0;` |
|   63289528 |  115 | `	pSet->nCursor = 0;` |
|   63289528 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   74522600 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   74522605 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       4019 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   74518591 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   74518591 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   37261305 |  126 | `}` |
|    9878037 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9878042 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2238117 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7639930 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7639930 |  135 | `	pSet->nUsed--;` |
|    7639930 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7639930 |  137 | `	return pData;` |
|    4939638 |  138 | `}` |
|   37276959 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   37276964 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   37276912 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   37276912 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   18642585 |  148 | `}` |
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
|    2191293 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    2191298 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2191298 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    2191298 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2191298 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    2191298 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    2191298 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    2191298 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    2191298 |  180 | `	pHash->nEntry = 0;` |
|    2191298 |  181 | `	pHash->apBucket = apNew;` |
|    2191298 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    2191298 |  183 | `	return SXRET_OK;` |
|    1095754 |  184 | `}` |
|     521831 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     521836 |  193 | `	pEntry = pHash->pList;` |
|     276549 |  194 | `	for(;;){` |
|     552898 |  195 | `		if( pHash->nEntry == 0 ){` |
|     521836 |  196 | `			break;` |
|          - |  197 | `		}` |
|      31067 |  198 | `		pNext = pEntry->pNext;` |
|      31067 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      31067 |  200 | `		pEntry = pNext;` |
|      31067 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     521836 |  203 | `	if( pHash->apBucket ){` |
|     521836 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     261018 |  205 | `	}` |
|     521836 |  206 | `	pHash->apBucket = 0;` |
|     521836 |  207 | `	pHash->nBucketSize = 0;` |
|     521836 |  208 | `	pHash->pAllocator = 0;` |
|     521836 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   75407712 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   75407717 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   75407717 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   68333466 |  218 | `	for(;;){` |
|  136451394 |  219 | `		if( pEntry == 0 ){` |
|   27407497 |  220 | `			break;` |
|          - |  221 | `		}` |
|  133043497 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   48004238 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   48000225 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   61043682 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   27407497 |  229 | `	return 0;` |
|   37709944 |  230 | `}` |
|   84432883 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   84432888 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    9025634 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   75407259 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   75407259 |  244 | `	if( pEntry == 0 ){` |
|   27407479 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   47999785 |  247 | `	return (SyHashEntry *)pEntry;` |
|   42222632 |  248 | `}` |
|     509062 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     509067 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     414326 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     207677 |  254 | `	}else{` |
|      94746 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     509067 |  257 | `	if( pEntry->pNextCollide ){` |
|       4392 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2190 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     509067 |  261 | `	if( pHash->pLast == pEntry ){` |
|     499523 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     250374 |  263 | `	}` |
|     509067 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     509067 |  265 | `	pHash->nEntry--;` |
|     509067 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     509067 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     509067 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        458 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        463 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        463 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        445 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        445 |  288 | `	return rc;` |
|        234 |  289 | `}` |
|     508622 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     508627 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     508627 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     508627 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3436186 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3436191 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3436191 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   26474658 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   26474663 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3435925 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3435925 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   23038743 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   23038743 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   23038743 |  328 | `	return (SyHashEntry *)pEntry;` |
|   13237334 |  329 | `}` |
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
|       4857 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4843 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4843 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4843 |  348 | `		pEntry = pEntry->pNext;` |
|       2422 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|     100916 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100921 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100921 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100921 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100921 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18679033 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18578117 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18578117 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18578117 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18578117 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8962609 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4480980 |  375 | `		}` |
|   18578117 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18578117 |  378 | `		pEntry = pEntry->pNext;` |
|    9289061 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100921 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100921 |  382 | `	pHash->apBucket = apNew;` |
|     100921 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100921 |  384 | `	return SXRET_OK;` |
|      50463 |  385 | `}` |
|   22642440 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   22642445 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   22642445 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   22642445 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   14221873 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    7111006 |  393 | `	}` |
|   22642445 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   22642445 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|     883477 |  399 | `		pHash->pLast->pNext = pEntry;` |
|     883477 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|     883477 |  401 | `		pHash->pLast = pEntry;` |
|     441741 |  402 | `	}else{` |
|   21758973 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   22642445 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1216424 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1216424 |  408 | `		pHash->pLast = pEntry;` |
|     608312 |  409 | `	}` |
|   22642445 |  410 | `	pHash->nEntry++;` |
|   22642445 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   22642440 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   22642445 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100921 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100921 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50458 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   22642445 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   22642445 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   22642445 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   22642445 |  435 | `	pEntry->pHash = pHash;` |
|   22642445 |  436 | `	pEntry->pKey = pKey;` |
|   22642445 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   22642445 |  438 | `	pEntry->pUserData = pUserData;` |
|   22642445 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   22642445 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   22642445 |  442 | `	return rc;` |
|   11321840 |  443 | `}` |
|   21496680 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   21496685 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|    1145760 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  455 | `{` |
|    1145765 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          5 |  457 | `}` |
|     549136 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     549141 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
