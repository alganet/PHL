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
|  160526176 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  160526181 |   16 | `	pSet->nSize = 0 ;` |
|  160526181 |   17 | `	pSet->nUsed = 0;` |
|  160526181 |   18 | `	pSet->nCursor = 0;` |
|  160526181 |   19 | `	pSet->eSize = ElemSize;` |
|  160526181 |   20 | `	pSet->pAllocator = pAllocator;` |
|  160526181 |   21 | `	pSet->pBase =  0;` |
|  160526181 |   22 | `	pSet->pUserData = 0;` |
|  160526181 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  366389629 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  366389634 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   21030759 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   21030759 |   33 | `		if( pSet->nSize <= 0 ){` |
|   17889959 |   34 | `			pSet->nSize = 4;` |
|    8944977 |   35 | `		}` |
|   21030759 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   21030759 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   21030759 |   40 | `		pSet->pBase = pNew;` |
|   21030759 |   41 | `		pSet->nSize <<= 1;` |
|   10515377 |   42 | `	}` |
|  366389634 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2893790694 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  366389634 |   45 | `	pSet->nUsed++;` |
|  366389634 |   46 | `	return SXRET_OK;` |
|  183194843 |   47 | `}` |
|   18206800 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   18206805 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   18206805 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   18206805 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   18206805 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   18206805 |   60 | `	pSet->nSize = nItem;` |
|   18206805 |   61 | `	return SXRET_OK;` |
|    9103405 |   62 | `}` |
|   27038705 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   27038710 |   65 | `	pSet->nUsed   = 0;` |
|   27038710 |   66 | `	pSet->nCursor = 0;` |
|   27038710 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69928 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69933 |   71 | `	pSet->nCursor = 0;` |
|      69933 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      74096 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      74101 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30253 |   79 | `		pSet->nCursor = 0;` |
|      30253 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43853 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43853 |   83 | `	if( ppEntry ){` |
|      43853 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21924 |   85 | `	}` |
|      43853 |   86 | `	pSet->nCursor++;` |
|      43853 |   87 | `	return SXRET_OK;` |
|      37053 |   88 | `}` |
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
|    2744798 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2744803 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1183 |  103 | `		pSet->nUsed = nNewSize;` |
|        589 |  104 | `	}` |
|    2744803 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   55206720 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   55206725 |  109 | `	sxi32 rc = SXRET_OK;` |
|   55206725 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   30089923 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   15044959 |  112 | `	}` |
|   55206725 |  113 | `	pSet->pBase = 0;` |
|   55206725 |  114 | `	pSet->nUsed = 0;` |
|   55206725 |  115 | `	pSet->nCursor = 0;` |
|   55206725 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   65224392 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   65224397 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19197 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   65205205 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   65205205 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   32612201 |  126 | `}` |
|    8944254 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    8944259 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2232327 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    6711937 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    6711937 |  135 | `	pSet->nUsed--;` |
|    6711937 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    6711937 |  137 | `	return pData;` |
|    4472132 |  138 | `}` |
|   33418795 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   33418800 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   33418778 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   33418778 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   16709541 |  148 | `}` |
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
|    1767620 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1767625 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1767625 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1767625 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1767625 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1767625 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1767625 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1767625 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1767625 |  180 | `	pHash->nEntry = 0;` |
|    1767625 |  181 | `	pHash->apBucket = apNew;` |
|    1767625 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1767625 |  183 | `	return SXRET_OK;` |
|     883815 |  184 | `}` |
|     411658 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     411663 |  193 | `	pEntry = pHash->pList;` |
|     220202 |  194 | `	for(;;){` |
|     440409 |  195 | `		if( pHash->nEntry == 0 ){` |
|     411663 |  196 | `			break;` |
|          - |  197 | `		}` |
|      28751 |  198 | `		pNext = pEntry->pNext;` |
|      28751 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      28751 |  200 | `		pEntry = pNext;` |
|      28751 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     411663 |  203 | `	if( pHash->apBucket ){` |
|     411663 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     205829 |  205 | `	}` |
|     411663 |  206 | `	pHash->apBucket = 0;` |
|     411663 |  207 | `	pHash->nBucketSize = 0;` |
|     411663 |  208 | `	pHash->pAllocator = 0;` |
|     411663 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   67008351 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   67008356 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   67008356 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   60750061 |  218 | `	for(;;){` |
|  121263890 |  219 | `		if( pEntry == 0 ){` |
|   24261474 |  220 | `			break;` |
|          - |  221 | `		}` |
|  118377707 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   42750854 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   42746887 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   54255539 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   24261474 |  229 | `	return 0;` |
|   33504464 |  230 | `}` |
|   73980063 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   73980068 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    6972169 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   67007904 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   67007904 |  244 | `	if( pEntry == 0 ){` |
|   24261456 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   42746453 |  247 | `	return (SyHashEntry *)pEntry;` |
|   36990320 |  248 | `}` |
|     432690 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     432695 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     356217 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     178111 |  254 | `	}else{` |
|      76483 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     432695 |  257 | `	if( pEntry->pNextCollide ){` |
|       4222 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2110 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     432695 |  261 | `	if( pHash->pLast == pEntry ){` |
|     422821 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     211408 |  263 | `	}` |
|     432695 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     432695 |  265 | `	pHash->nEntry--;` |
|     432695 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     432695 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     432695 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        452 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        457 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        457 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        439 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        439 |  288 | `	return rc;` |
|        231 |  289 | `}` |
|     432256 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     432261 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     432261 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     432261 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2827570 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2827575 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2827575 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   21125300 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   21125305 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2827309 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2827309 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18298001 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18298001 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18298001 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10562655 |  329 | `}` |
|         10 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|          1 |  331 | `{` |
|          - |  332 | `	SyHashEntry_Pr *pEntry;` |
|          - |  333 | `	sxi32 rc;` |
|          - |  334 | `	sxu32 n;` |
|          - |  335 | `#if defined(UNTRUST)` |
|          - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|          - |  337 | `		return 0;` |
|          - |  338 | `	}` |
|          - |  339 | `#endif` |
|         11 |  340 | `	pEntry = pHash->pList;` |
|       4065 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4055 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4055 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4055 |  348 | `		pEntry = pEntry->pNext;` |
|       2028 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      91776 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      91781 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      91781 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      91781 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      91781 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14407685 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14315909 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14315909 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14315909 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14315909 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    6870554 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3435385 |  375 | `		}` |
|   14315909 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14315909 |  378 | `		pEntry = pEntry->pNext;` |
|    7157957 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      91781 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      91781 |  382 | `	pHash->apBucket = apNew;` |
|      91781 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      91781 |  384 | `	return SXRET_OK;` |
|      45893 |  385 | `}` |
|   18429072 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   18429077 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   18429077 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   18429077 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11669453 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5834516 |  393 | `	}` |
|   18429077 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   18429077 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   18429025 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   18429077 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     974895 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     974895 |  408 | `		pHash->pLast = pEntry;` |
|     487445 |  409 | `	}` |
|   18429077 |  410 | `	pHash->nEntry++;` |
|   18429077 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   18429072 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   18429077 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      91781 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      91781 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45888 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   18429077 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   18429077 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   18429077 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   18429077 |  435 | `	pEntry->pHash = pHash;` |
|   18429077 |  436 | `	pEntry->pKey = pKey;` |
|   18429077 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   18429077 |  438 | `	pEntry->pUserData = pUserData;` |
|   18429077 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   18429077 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   18429077 |  442 | `	return rc;` |
|    9214541 |  443 | `}` |
|   18428938 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   18428943 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     471926 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     471931 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
