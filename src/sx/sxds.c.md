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
|  184109794 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  184109799 |   16 | `	pSet->nSize = 0 ;` |
|  184109799 |   17 | `	pSet->nUsed = 0;` |
|  184109799 |   18 | `	pSet->nCursor = 0;` |
|  184109799 |   19 | `	pSet->eSize = ElemSize;` |
|  184109799 |   20 | `	pSet->pAllocator = pAllocator;` |
|  184109799 |   21 | `	pSet->pBase =  0;` |
|  184109799 |   22 | `	pSet->pUserData = 0;` |
|  184109799 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  417193235 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  417193240 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   24107552 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   24107552 |   33 | `		if( pSet->nSize <= 0 ){` |
|   20549390 |   34 | `			pSet->nSize = 4;` |
|   10275619 |   35 | `		}` |
|   24107552 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   24107552 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   24107552 |   40 | `		pSet->pBase = pNew;` |
|   24107552 |   41 | `		pSet->nSize <<= 1;` |
|   12054700 |   42 | `	}` |
|  417193240 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3293766894 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  417193240 |   45 | `	pSet->nUsed++;` |
|  417193240 |   46 | `	return SXRET_OK;` |
|  208599768 |   47 | `}` |
|   20735466 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   20735471 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   20735471 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   20735471 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   20735471 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   20735471 |   60 | `	pSet->nSize = nItem;` |
|   20735471 |   61 | `	return SXRET_OK;` |
|   10367738 |   62 | `}` |
|   30705032 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   30705037 |   65 | `	pSet->nUsed   = 0;` |
|   30705037 |   66 | `	pSet->nCursor = 0;` |
|   30705037 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69086 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69091 |   71 | `	pSet->nCursor = 0;` |
|      69091 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      69344 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      69349 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30045 |   79 | `		pSet->nCursor = 0;` |
|      30045 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      39309 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      39309 |   83 | `	if( ppEntry ){` |
|      39309 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      19652 |   85 | `	}` |
|      39309 |   86 | `	pSet->nCursor++;` |
|      39309 |   87 | `	return SXRET_OK;` |
|      34677 |   88 | `}` |
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
|    3276402 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3276407 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1213 |  103 | `		pSet->nUsed = nNewSize;` |
|        604 |  104 | `	}` |
|    3276407 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   62975784 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   62975789 |  109 | `	sxi32 rc = SXRET_OK;` |
|   62975789 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   34091352 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   17046600 |  112 | `	}` |
|   62975789 |  113 | `	pSet->pBase = 0;` |
|   62975789 |  114 | `	pSet->nUsed = 0;` |
|   62975789 |  115 | `	pSet->nCursor = 0;` |
|   62975789 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   74149416 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   74149421 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       3999 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   74145427 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   74145427 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   37074713 |  126 | `}` |
|    9834997 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9835002 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2237577 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7597430 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7597430 |  135 | `	pSet->nUsed--;` |
|    7597430 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7597430 |  137 | `	return pData;` |
|    4918121 |  138 | `}` |
|   37000959 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   37000964 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   37000912 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   37000912 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   18504609 |  148 | `}` |
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
|    2176780 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    2176785 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2176785 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    2176785 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2176785 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    2176785 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    2176785 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    2176785 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    2176785 |  180 | `	pHash->nEntry = 0;` |
|    2176785 |  181 | `	pHash->apBucket = apNew;` |
|    2176785 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    2176785 |  183 | `	return SXRET_OK;` |
|    1088498 |  184 | `}` |
|     519776 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     519781 |  193 | `	pEntry = pHash->pList;` |
|     275342 |  194 | `	for(;;){` |
|     550483 |  195 | `		if( pHash->nEntry == 0 ){` |
|     519781 |  196 | `			break;` |
|          - |  197 | `		}` |
|      30707 |  198 | `		pNext = pEntry->pNext;` |
|      30707 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      30707 |  200 | `		pEntry = pNext;` |
|      30707 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     519781 |  203 | `	if( pHash->apBucket ){` |
|     519781 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     259991 |  205 | `	}` |
|     519781 |  206 | `	pHash->apBucket = 0;` |
|     519781 |  207 | `	pHash->nBucketSize = 0;` |
|     519781 |  208 | `	pHash->pAllocator = 0;` |
|     519781 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   74951031 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   74951036 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   74951036 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   67666034 |  218 | `	for(;;){` |
|  135641540 |  219 | `		if( pEntry == 0 ){` |
|   27225610 |  220 | `			break;` |
|          - |  221 | `		}` |
|  132278105 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   47729424 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   47725431 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   60690509 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   27225610 |  229 | `	return 0;` |
|   37481645 |  230 | `}` |
|   83930169 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   83930174 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    8979601 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   74950578 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   74950578 |  244 | `	if( pEntry == 0 ){` |
|   27225592 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   47724991 |  247 | `	return (SyHashEntry *)pEntry;` |
|   41971317 |  248 | `}` |
|     496868 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     496873 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     407934 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     204485 |  254 | `	}else{` |
|      88944 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     496873 |  257 | `	if( pEntry->pNextCollide ){` |
|       4297 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2150 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     496873 |  261 | `	if( pHash->pLast == pEntry ){` |
|     487343 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     244287 |  263 | `	}` |
|     496873 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     496873 |  265 | `	pHash->nEntry--;` |
|     496873 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     496873 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     496873 |  272 | `	return rc;` |
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
|     496428 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     496433 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     496433 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     496433 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3422798 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3422803 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3422803 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   26379606 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   26379611 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3422537 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3422537 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   22957079 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   22957079 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   22957079 |  328 | `	return (SyHashEntry *)pEntry;` |
|   13189808 |  329 | `}` |
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
|       4833 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4819 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4819 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4819 |  348 | `		pEntry = pEntry->pNext;` |
|       2410 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|     100464 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100469 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100469 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100469 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100469 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18603125 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18502661 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18502661 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18502661 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18502661 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8928997 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4464636 |  375 | `		}` |
|   18502661 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18502661 |  378 | `		pEntry = pEntry->pNext;` |
|    9251333 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100469 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100469 |  382 | `	pHash->apBucket = apNew;` |
|     100469 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100469 |  384 | `	return SXRET_OK;` |
|      50237 |  385 | `}` |
|   22525522 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   22525527 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   22525527 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   22525527 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   14151831 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    7076020 |  393 | `	}` |
|   22525527 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   22525527 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|     878735 |  399 | `		pHash->pLast->pNext = pEntry;` |
|     878735 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|     878735 |  401 | `		pHash->pLast = pEntry;` |
|     439370 |  402 | `	}else{` |
|   21646797 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   22525527 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1210497 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1210497 |  408 | `		pHash->pLast = pEntry;` |
|     605349 |  409 | `	}` |
|   22525527 |  410 | `	pHash->nEntry++;` |
|   22525527 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   22525522 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   22525527 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100469 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100469 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50232 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   22525527 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   22525527 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   22525527 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   22525527 |  435 | `	pEntry->pHash = pHash;` |
|   22525527 |  436 | `	pEntry->pKey = pKey;` |
|   22525527 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   22525527 |  438 | `	pEntry->pUserData = pUserData;` |
|   22525527 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   22525527 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   22525527 |  442 | `	return rc;` |
|   11263384 |  443 | `}` |
|   21385902 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   21385907 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|    1139620 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  455 | `{` |
|    1139625 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          5 |  457 | `}` |
|     536798 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     536803 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
