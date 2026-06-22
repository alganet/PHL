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
|  17969294 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 |  |
|  17969299 |   16 | `	pSet->nSize = 0 ;` |
|  17969299 |   17 | `	pSet->nUsed = 0;` |
|  17969299 |   18 | `	pSet->nCursor = 0;` |
|  17969299 |   19 | `	pSet->eSize = ElemSize;` |
|  17969299 |   20 | `	pSet->pAllocator = pAllocator;` |
|  17969299 |   21 | `	pSet->pBase =  0;` |
|  17969299 |   22 | `	pSet->pUserData = 0;` |
|  17969299 |   23 | `	return SXRET_OK;` |
|         5 |   24 |  |
|  29458507 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  29458512 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4386703 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4386703 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4242911 |   34 | `			pSet->nSize = 4;` |
|   2121453 |   35 | `		}` |
|   4386703 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4386703 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4386703 |   40 | `		pSet->pBase = pNew;` |
|   4386703 |   41 | `		pSet->nSize <<= 1;` |
|   2193349 |   42 | `	}` |
|  29458512 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 220527020 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  29458512 |   45 | `	pSet->nUsed++;` |
|  29458512 |   46 | `	return SXRET_OK;` |
|  14729301 |   47 |  |
|   1186790 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 |  |
|   1186795 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1186795 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1186795 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1186795 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1186795 |   60 | `	pSet->nSize = nItem;` |
|   1186795 |   61 | `	return SXRET_OK;` |
|    593400 |   62 |  |
|   1676243 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 |  |
|   1676248 |   65 | `	pSet->nUsed   = 0;` |
|   1676248 |   66 | `	pSet->nCursor = 0;` |
|   1676248 |   67 | `	return SXRET_OK;` |
|         5 |   68 |  |
|     54964 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 |  |
|     54969 |   71 | `	pSet->nCursor = 0;` |
|     54969 |   72 | `	return SXRET_OK;` |
|         5 |   73 |  |
|     59148 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     59153 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     22661 |   79 | `		pSet->nCursor = 0;` |
|     22661 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     36497 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     36497 |   83 | `	if( ppEntry ){` |
|     36497 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     18246 |   85 | `	}` |
|     36497 |   86 | `	pSet->nCursor++;` |
|     36497 |   87 | `	return SXRET_OK;` |
|     29579 |   88 |  |
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
|    200620 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 |  |
|    200625 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       118 |  103 | `		pSet->nUsed = nNewSize;` |
|        57 |  104 | `	}` |
|    200625 |  105 | `	return SXRET_OK;` |
|         5 |  106 |  |
|   9555860 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 |  |
|   9555865 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9555865 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4784141 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2392068 |  112 | `	}` |
|   9555865 |  113 | `	pSet->pBase = 0;` |
|   9555865 |  114 | `	pSet->nUsed = 0;` |
|   9555865 |  115 | `	pSet->nCursor = 0;` |
|   9555865 |  116 | `	return rc;` |
|         5 |  117 |  |
|   5422976 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5422981 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       121 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5422865 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5422865 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2711493 |  126 |  |
|   3497196 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3497201 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2172987 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1324219 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1324219 |  135 | `	pSet->nUsed--;` |
|   1324219 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1324219 |  137 | `	return pData;` |
|   1748603 |  138 |  |
|  12786664 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  12786669 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  12786669 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  12786669 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6393587 |  148 |  |
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
|    516060 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    516065 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    516065 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    516065 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    516065 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    516065 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    516065 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    516065 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    516065 |  180 | `	pHash->nEntry = 0;` |
|    516065 |  181 | `	pHash->apBucket = apNew;` |
|    516065 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    516065 |  183 | `	return SXRET_OK;` |
|    258035 |  184 |  |
|     95630 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     95635 |  193 | `	pEntry = pHash->pList;` |
|     51025 |  194 | `	for(;;){` |
|    102055 |  195 | `		if( pHash->nEntry == 0 ){` |
|     95635 |  196 | `			break;` |
|         - |  197 | `		}` |
|      6425 |  198 | `		pNext = pEntry->pNext;` |
|      6425 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      6425 |  200 | `		pEntry = pNext;` |
|      6425 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|     95635 |  203 | `	if( pHash->apBucket ){` |
|     95635 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     47815 |  205 | `	}` |
|     95635 |  206 | `	pHash->apBucket = 0;` |
|     95635 |  207 | `	pHash->nBucketSize = 0;` |
|     95635 |  208 | `	pHash->pAllocator = 0;` |
|     95635 |  209 | `	return SXRET_OK;` |
|         5 |  210 |  |
|  16170912 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  16170917 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  16170917 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  14281076 |  218 | `	for(;;){` |
|  28581604 |  219 | `		if( pEntry == 0 ){` |
|   8508063 |  220 | `			break;` |
|         - |  221 | `		}` |
|  23904720 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   7662858 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   7662859 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  12410692 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   8508063 |  229 | `	return 0;` |
|   8085971 |  230 |  |
|  16930546 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  16930551 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    759805 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  16170751 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  16170751 |  244 | `	if( pEntry == 0 ){` |
|   8508063 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   7662693 |  247 | `	return (SyHashEntry *)pEntry;` |
|   8465788 |  248 |  |
|    114618 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    114623 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     87903 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     43954 |  254 | `	}else{` |
|     26725 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    114623 |  257 | `	if( pEntry->pNextCollide ){` |
|      4929 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2464 |  259 | `	}` |
|    114623 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    114623 |  261 | `	pHash->nEntry--;` |
|    114623 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    114623 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    114623 |  268 | `	return rc;` |
|         5 |  269 |  |
|       166 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       171 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       171 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       171 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       171 |  284 | `	return rc;` |
|        88 |  285 |  |
|    114452 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  287 |  |
|    114457 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    114457 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    114457 |  296 | `	return rc;` |
|         5 |  297 |  |
|   1050584 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|   1050589 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   1050589 |  306 | `	return SXRET_OK;` |
|         5 |  307 |  |
|   6576048 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   6576053 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1050145 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   1050145 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   5525913 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   5525913 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   5525913 |  324 | `	return (SyHashEntry *)pEntry;` |
|   3288029 |  325 |  |
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
|      1911 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1901 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1901 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1901 |  344 | `		pEntry = pEntry->pNext;` |
|       951 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     26744 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  349 |  |
|     26749 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     26749 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     26749 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     26749 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3398557 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3371813 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3371813 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3371813 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3371813 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1606039 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    803052 |  371 | `		}` |
|   3371813 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3371813 |  374 | `		pEntry = pEntry->pNext;` |
|   1685909 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     26749 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     26749 |  378 | `	pHash->apBucket = apNew;` |
|     26749 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     26749 |  380 | `	return SXRET_OK;` |
|     13377 |  381 |  |
|   4374082 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         5 |  383 |  |
|   4374087 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   4374087 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4374087 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2441620 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1220707 |  389 | `	}` |
|   4374087 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   4374087 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   4374087 |  393 | `	if( pHash->nEntry == 0 ){` |
|    281135 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    140565 |  395 | `	}` |
|   4374087 |  396 | `	pHash->nEntry++;` |
|   4374087 |  397 | `	return SXRET_OK;` |
|         5 |  398 |  |
|   4374082 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   4374087 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     26749 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     26749 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     13372 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   4374087 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4374087 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   4374087 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4374087 |  421 | `	pEntry->pHash = pHash;` |
|   4374087 |  422 | `	pEntry->pKey = pKey;` |
|   4374087 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   4374087 |  424 | `	pEntry->pUserData = pUserData;` |
|   4374087 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4374087 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   4374087 |  428 | `	return rc;` |
|   2187046 |  429 |  |
|    147574 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    147579 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  439 |  |
|         - |  440 |  |
