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
|   9795400 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|   9795402 |   16 | `	pSet->nSize = 0 ;` |
|   9795402 |   17 | `	pSet->nUsed = 0;` |
|   9795402 |   18 | `	pSet->nCursor = 0;` |
|   9795402 |   19 | `	pSet->eSize = ElemSize;` |
|   9795402 |   20 | `	pSet->pAllocator = pAllocator;` |
|   9795402 |   21 | `	pSet->pBase =  0;` |
|   9795402 |   22 | `	pSet->pUserData = 0;` |
|   9795402 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  15446822 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  15446824 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3213482 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3213482 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3155074 |   34 | `			pSet->nSize = 4;` |
|   1577536 |   35 | `		}` |
|   3213482 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3213482 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3213482 |   40 | `		pSet->pBase = pNew;` |
|   3213482 |   41 | `		pSet->nSize <<= 1;` |
|   1606740 |   42 | `	}` |
|  15446824 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 116865772 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  15446824 |   45 | `	pSet->nUsed++;` |
|  15446824 |   46 | `	return SXRET_OK;` |
|   7723435 |   47 |  |
|    418430 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    418432 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    418432 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    418432 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    418432 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    418432 |   60 | `	pSet->nSize = nItem;` |
|    418432 |   61 | `	return SXRET_OK;` |
|    209217 |   62 |  |
|    829984 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|    829986 |   65 | `	pSet->nUsed   = 0;` |
|    829986 |   66 | `	pSet->nCursor = 0;` |
|    829986 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     33882 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     33884 |   71 | `	pSet->nCursor = 0;` |
|     33884 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     37106 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     37108 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     13558 |   79 | `		pSet->nCursor = 0;` |
|     13558 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     23552 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     23552 |   83 | `	if( ppEntry ){` |
|     23552 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     11775 |   85 | `	}` |
|     23552 |   86 | `	pSet->nCursor++;` |
|     23552 |   87 | `	return SXRET_OK;` |
|     18555 |   88 |  |
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
|     52900 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     52902 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     52902 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   6657784 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   6657786 |  109 | `	sxi32 rc = SXRET_OK;` |
|   6657786 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3417688 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1708843 |  112 | `	}` |
|   6657786 |  113 | `	pSet->pBase = 0;` |
|   6657786 |  114 | `	pSet->nUsed = 0;` |
|   6657786 |  115 | `	pSet->nCursor = 0;` |
|   6657786 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3249972 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3249974 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3249884 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3249884 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1624988 |  126 |  |
|   2911518 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   2911520 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2120628 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    790894 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    790894 |  135 | `	pSet->nUsed--;` |
|    790894 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    790894 |  137 | `	return pData;` |
|   1455761 |  138 |  |
|   8284558 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   8284560 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   8284560 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   8284560 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4142493 |  148 |  |
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
|     75244 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     75246 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     75246 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     75246 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     75246 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     75246 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     75246 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     75246 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     75246 |  180 | `	pHash->nEntry = 0;` |
|     75246 |  181 | `	pHash->apBucket = apNew;` |
|     75246 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     75246 |  183 | `	return SXRET_OK;` |
|     37624 |  184 |  |
|      9606 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|      9608 |  193 | `	pEntry = pHash->pList;` |
|      5535 |  194 | `	for(;;){` |
|     11072 |  195 | `		if( pHash->nEntry == 0 ){` |
|      9608 |  196 | `			break;` |
|         - |  197 | `		}` |
|      1466 |  198 | `		pNext = pEntry->pNext;` |
|      1466 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      1466 |  200 | `		pEntry = pNext;` |
|      1466 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|      9608 |  203 | `	if( pHash->apBucket ){` |
|      9608 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      4803 |  205 | `	}` |
|      9608 |  206 | `	pHash->apBucket = 0;` |
|      9608 |  207 | `	pHash->nBucketSize = 0;` |
|      9608 |  208 | `	pHash->pAllocator = 0;` |
|      9608 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   7650278 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   7650280 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   7650280 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   6610599 |  218 | `	for(;;){` |
|  13188775 |  219 | `		if( pEntry == 0 ){` |
|   4139838 |  220 | `			break;` |
|         - |  221 | `		}` |
|  10804030 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   3510446 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   3510444 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   5538497 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   4139838 |  229 | `	return 0;` |
|   3825405 |  230 |  |
|   7692852 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   7692854 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     42582 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   7650274 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   7650274 |  244 | `	if( pEntry == 0 ){` |
|   4139838 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   3510438 |  247 | `	return (SyHashEntry *)pEntry;` |
|   3846692 |  248 |  |
|     61716 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     61718 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     46192 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     23097 |  254 | `	}else{` |
|     15528 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     61718 |  257 | `	if( pEntry->pNextCollide ){` |
|      3639 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      1819 |  259 | `	}` |
|     61718 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     61718 |  261 | `	pHash->nEntry--;` |
|     61718 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     61718 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     61718 |  268 | `	return rc;` |
|         2 |  269 |  |
|         6 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         1 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|         7 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|         7 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|         7 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|         7 |  284 | `	return rc;` |
|         4 |  285 |  |
|     61710 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     61712 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     61712 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     61712 |  296 | `	return rc;` |
|         2 |  297 |  |
|    110452 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    110454 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    110454 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    770344 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    770346 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    110020 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    110020 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    660328 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    660328 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    660328 |  324 | `	return (SyHashEntry *)pEntry;` |
|    385174 |  325 |  |
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
|      1579 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1569 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1569 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1569 |  344 | `		pEntry = pEntry->pNext;` |
|       785 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     10540 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     10542 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     10542 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     10542 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     10542 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1442382 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1431842 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1431842 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1431842 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1431842 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    687600 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    343798 |  371 | `		}` |
|   1431842 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1431842 |  374 | `		pEntry = pEntry->pNext;` |
|    715922 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     10542 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     10542 |  378 | `	pHash->apBucket = apNew;` |
|     10542 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     10542 |  380 | `	return SXRET_OK;` |
|      5272 |  381 |  |
|   1304372 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1304374 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1304374 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1304374 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    868185 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    434077 |  389 | `	}` |
|   1304374 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1304374 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1304374 |  393 | `	if( pHash->nEntry == 0 ){` |
|     53924 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     26961 |  395 | `	}` |
|   1304374 |  396 | `	pHash->nEntry++;` |
|   1304374 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1304372 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1304374 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     10542 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     10542 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      5270 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1304374 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1304374 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1304374 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1304374 |  421 | `	pEntry->pHash = pHash;` |
|   1304374 |  422 | `	pEntry->pKey = pKey;` |
|   1304374 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1304374 |  424 | `	pEntry->pUserData = pUserData;` |
|   1304374 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1304374 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1304374 |  428 | `	return rc;` |
|    652188 |  429 |  |
|     75002 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     75004 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
