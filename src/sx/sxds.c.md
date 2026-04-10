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
|  14254936 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  14254938 |   16 | `	pSet->nSize = 0 ;` |
|  14254938 |   17 | `	pSet->nUsed = 0;` |
|  14254938 |   18 | `	pSet->nCursor = 0;` |
|  14254938 |   19 | `	pSet->eSize = ElemSize;` |
|  14254938 |   20 | `	pSet->pAllocator = pAllocator;` |
|  14254938 |   21 | `	pSet->pBase =  0;` |
|  14254938 |   22 | `	pSet->pUserData = 0;` |
|  14254938 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  23574772 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  23574774 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3896136 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3896136 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3787764 |   34 | `			pSet->nSize = 4;` |
|   1893881 |   35 | `		}` |
|   3896136 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3896136 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3896136 |   40 | `		pSet->pBase = pNew;` |
|   3896136 |   41 | `		pSet->nSize <<= 1;` |
|   1948067 |   42 | `	}` |
|  23574774 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 175050234 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  23574774 |   45 | `	pSet->nUsed++;` |
|  23574774 |   46 | `	return SXRET_OK;` |
|  11787410 |   47 |  |
|    853892 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    853894 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    853894 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    853894 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    853894 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    853894 |   60 | `	pSet->nSize = nItem;` |
|    853894 |   61 | `	return SXRET_OK;` |
|    426948 |   62 |  |
|   1313780 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1313782 |   65 | `	pSet->nUsed   = 0;` |
|   1313782 |   66 | `	pSet->nCursor = 0;` |
|   1313782 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     45610 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     45612 |   71 | `	pSet->nCursor = 0;` |
|     45612 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     49692 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     49694 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     18702 |   79 | `		pSet->nCursor = 0;` |
|     18702 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     30994 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     30994 |   83 | `	if( ppEntry ){` |
|     30994 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     15496 |   85 | `	}` |
|     30994 |   86 | `	pSet->nCursor++;` |
|     30994 |   87 | `	return SXRET_OK;` |
|     24848 |   88 |  |
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
|    141902 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    141904 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|    141904 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8419358 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8419360 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8419360 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4295958 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2147978 |  112 | `	}` |
|   8419360 |  113 | `	pSet->pBase = 0;` |
|   8419360 |  114 | `	pSet->nUsed = 0;` |
|   8419360 |  115 | `	pSet->nCursor = 0;` |
|   8419360 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4567700 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4567702 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       106 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4567598 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4567598 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2283852 |  126 |  |
|   3287046 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3287048 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2143306 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1143744 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1143744 |  135 | `	pSet->nUsed--;` |
|   1143744 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1143744 |  137 | `	return pData;` |
|   1643525 |  138 |  |
|  10925952 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  10925954 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  10925954 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  10925954 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5463112 |  148 |  |
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
|    254092 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    254094 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    254094 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    254094 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    254094 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    254094 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    254094 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    254094 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    254094 |  180 | `	pHash->nEntry = 0;` |
|    254094 |  181 | `	pHash->apBucket = apNew;` |
|    254094 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    254094 |  183 | `	return SXRET_OK;` |
|    127048 |  184 |  |
|     76648 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     76650 |  193 | `	pEntry = pHash->pList;` |
|     40215 |  194 | `	for(;;){` |
|     80432 |  195 | `		if( pHash->nEntry == 0 ){` |
|     76650 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3784 |  198 | `		pNext = pEntry->pNext;` |
|      3784 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3784 |  200 | `		pEntry = pNext;` |
|      3784 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     76650 |  203 | `	if( pHash->apBucket ){` |
|     76650 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     38324 |  205 | `	}` |
|     76650 |  206 | `	pHash->apBucket = 0;` |
|     76650 |  207 | `	pHash->nBucketSize = 0;` |
|     76650 |  208 | `	pHash->pAllocator = 0;` |
|     76650 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  11834394 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  11834396 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  11834396 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  10757490 |  218 | `	for(;;){` |
|  21486786 |  219 | `		if( pEntry == 0 ){` |
|   6523116 |  220 | `			break;` |
|         - |  221 | `		}` |
|  17619182 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5311284 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5311282 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   9652392 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   6523116 |  229 | `	return 0;` |
|   5917463 |  230 |  |
|  12294422 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  12294424 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    460052 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  11834374 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  11834374 |  244 | `	if( pEntry == 0 ){` |
|   6523116 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5311260 |  247 | `	return (SyHashEntry *)pEntry;` |
|   6147477 |  248 |  |
|     87940 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     87942 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     66736 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     33369 |  254 | `	}else{` |
|     21208 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     87942 |  257 | `	if( pEntry->pNextCollide ){` |
|      4537 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2268 |  259 | `	}` |
|     87942 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     87942 |  261 | `	pHash->nEntry--;` |
|     87942 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     87942 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     87942 |  268 | `	return rc;` |
|         2 |  269 |  |
|        22 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|        24 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        24 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|        24 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        24 |  284 | `	return rc;` |
|        13 |  285 |  |
|     87918 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     87920 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     87920 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     87920 |  296 | `	return rc;` |
|         2 |  297 |  |
|    306322 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    306324 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    306324 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2402924 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2402926 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    305890 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    305890 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2097038 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2097038 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2097038 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1201464 |  325 |  |
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
|     22214 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     22216 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     22216 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     22216 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     22216 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2821288 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2799074 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2799074 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2799074 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2799074 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1337784 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    668906 |  371 | `		}` |
|   2799074 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2799074 |  374 | `		pEntry = pEntry->pNext;` |
|   1399538 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     22216 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     22216 |  378 | `	pHash->apBucket = apNew;` |
|     22216 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     22216 |  380 | `	return SXRET_OK;` |
|     11109 |  381 |  |
|   2867058 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2867060 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2867060 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2867060 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1857336 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    928706 |  389 | `	}` |
|   2867060 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2867060 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2867060 |  393 | `	if( pHash->nEntry == 0 ){` |
|    127684 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     63841 |  395 | `	}` |
|   2867060 |  396 | `	pHash->nEntry++;` |
|   2867060 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2867058 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2867060 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     22216 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     22216 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     11107 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2867060 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2867060 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2867060 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2867060 |  421 | `	pEntry->pHash = pHash;` |
|   2867060 |  422 | `	pEntry->pKey = pKey;` |
|   2867060 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2867060 |  424 | `	pEntry->pUserData = pUserData;` |
|   2867060 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2867060 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2867060 |  428 | `	return rc;` |
|   1433531 |  429 |  |
|    112862 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    112864 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
