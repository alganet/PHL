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
|  15406446 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  15406448 |   16 | `	pSet->nSize = 0 ;` |
|  15406448 |   17 | `	pSet->nUsed = 0;` |
|  15406448 |   18 | `	pSet->nCursor = 0;` |
|  15406448 |   19 | `	pSet->eSize = ElemSize;` |
|  15406448 |   20 | `	pSet->pAllocator = pAllocator;` |
|  15406448 |   21 | `	pSet->pBase =  0;` |
|  15406448 |   22 | `	pSet->pUserData = 0;` |
|  15406448 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  25061018 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  25061020 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4011014 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4011014 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3892638 |   34 | `			pSet->nSize = 4;` |
|   1946318 |   35 | `		}` |
|   4011014 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4011014 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4011014 |   40 | `		pSet->pBase = pNew;` |
|   4011014 |   41 | `		pSet->nSize <<= 1;` |
|   2005506 |   42 | `	}` |
|  25061020 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 186969922 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  25061020 |   45 | `	pSet->nUsed++;` |
|  25061020 |   46 | `	return SXRET_OK;` |
|  12530533 |   47 |  |
|    933112 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    933114 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    933114 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    933114 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    933114 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    933114 |   60 | `	pSet->nSize = nItem;` |
|    933114 |   61 | `	return SXRET_OK;` |
|    466558 |   62 |  |
|   1454434 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1454436 |   65 | `	pSet->nUsed   = 0;` |
|   1454436 |   66 | `	pSet->nCursor = 0;` |
|   1454436 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     49488 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     49490 |   71 | `	pSet->nCursor = 0;` |
|     49490 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     53570 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     53572 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     20354 |   79 | `		pSet->nCursor = 0;` |
|     20354 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     33220 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     33220 |   83 | `	if( ppEntry ){` |
|     33220 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     16609 |   85 | `	}` |
|     33220 |   86 | `	pSet->nCursor++;` |
|     33220 |   87 | `	return SXRET_OK;` |
|     26787 |   88 |  |
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
|    155238 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    155240 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       110 |  103 | `		pSet->nUsed = nNewSize;` |
|        54 |  104 | `	}` |
|    155240 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8745714 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8745716 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8745716 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4446682 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2223340 |  112 | `	}` |
|   8745716 |  113 | `	pSet->pBase = 0;` |
|   8745716 |  114 | `	pSet->nUsed = 0;` |
|   8745716 |  115 | `	pSet->nCursor = 0;` |
|   8745716 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4798068 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4798070 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       108 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4797964 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4797964 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2399036 |  126 |  |
|   3343318 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3343320 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2147482 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1195840 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1195840 |  135 | `	pSet->nUsed--;` |
|   1195840 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1195840 |  137 | `	return pData;` |
|   1671661 |  138 |  |
|  11584091 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  11584093 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  11584093 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  11584093 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5792227 |  148 |  |
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
|    281498 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    281500 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    281500 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    281500 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    281500 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    281500 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    281500 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    281500 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    281500 |  180 | `	pHash->nEntry = 0;` |
|    281500 |  181 | `	pHash->apBucket = apNew;` |
|    281500 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    281500 |  183 | `	return SXRET_OK;` |
|    140751 |  184 |  |
|     84078 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     84080 |  193 | `	pEntry = pHash->pList;` |
|     44302 |  194 | `	for(;;){` |
|     88606 |  195 | `		if( pHash->nEntry == 0 ){` |
|     84080 |  196 | `			break;` |
|         - |  197 | `		}` |
|      4528 |  198 | `		pNext = pEntry->pNext;` |
|      4528 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      4528 |  200 | `		pEntry = pNext;` |
|      4528 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     84080 |  203 | `	if( pHash->apBucket ){` |
|     84080 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     42039 |  205 | `	}` |
|     84080 |  206 | `	pHash->apBucket = 0;` |
|     84080 |  207 | `	pHash->nBucketSize = 0;` |
|     84080 |  208 | `	pHash->pAllocator = 0;` |
|     84080 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  12767176 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  12767178 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  12767178 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  11565296 |  218 | `	for(;;){` |
|  23118229 |  219 | `		if( pEntry == 0 ){` |
|   7056282 |  220 | `			break;` |
|         - |  221 | `		}` |
|  18917267 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5710900 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5710898 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  10351053 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   7056282 |  229 | `	return 0;` |
|   6383854 |  230 |  |
|  13300218 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  13300220 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    533186 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  12767036 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  12767036 |  244 | `	if( pEntry == 0 ){` |
|   7056282 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5710756 |  247 | `	return (SyHashEntry *)pEntry;` |
|   6650375 |  248 |  |
|     99406 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     99408 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     75800 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     37901 |  254 | `	}else{` |
|     23610 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     99408 |  257 | `	if( pEntry->pNextCollide ){` |
|      4931 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2465 |  259 | `	}` |
|     99408 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     99408 |  261 | `	pHash->nEntry--;` |
|     99408 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     99408 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     99408 |  268 | `	return rc;` |
|         2 |  269 |  |
|       142 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       144 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       144 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       144 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       144 |  284 | `	return rc;` |
|        73 |  285 |  |
|     99264 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     99266 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     99266 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     99266 |  296 | `	return rc;` |
|         2 |  297 |  |
|    336418 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    336420 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    336420 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2709262 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2709264 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    335984 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    335984 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2373282 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2373282 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2373282 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1354633 |  325 |  |
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
|     24212 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     24214 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     24214 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     24214 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     24214 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3074422 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3050210 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3050210 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3050210 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3050210 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1457717 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    728804 |  371 | `		}` |
|   3050210 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3050210 |  374 | `		pEntry = pEntry->pNext;` |
|   1525106 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     24214 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     24214 |  378 | `	pHash->apBucket = apNew;` |
|     24214 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     24214 |  380 | `	return SXRET_OK;` |
|     12108 |  381 |  |
|   3132032 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   3132034 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   3132034 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   3132034 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2025950 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1012955 |  389 | `	}` |
|   3132034 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   3132034 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   3132034 |  393 | `	if( pHash->nEntry == 0 ){` |
|    140794 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     70396 |  395 | `	}` |
|   3132034 |  396 | `	pHash->nEntry++;` |
|   3132034 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   3132032 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   3132034 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     24214 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     24214 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     12106 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   3132034 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   3132034 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   3132034 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   3132034 |  421 | `	pEntry->pHash = pHash;` |
|   3132034 |  422 | `	pEntry->pKey = pKey;` |
|   3132034 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   3132034 |  424 | `	pEntry->pUserData = pUserData;` |
|   3132034 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   3132034 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   3132034 |  428 | `	return rc;` |
|   1566018 |  429 |  |
|    126434 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    126436 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
