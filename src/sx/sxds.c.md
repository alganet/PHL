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
|  182293432 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  182293437 |   16 | `	pSet->nSize = 0 ;` |
|  182293437 |   17 | `	pSet->nUsed = 0;` |
|  182293437 |   18 | `	pSet->nCursor = 0;` |
|  182293437 |   19 | `	pSet->eSize = ElemSize;` |
|  182293437 |   20 | `	pSet->pAllocator = pAllocator;` |
|  182293437 |   21 | `	pSet->pBase =  0;` |
|  182293437 |   22 | `	pSet->pUserData = 0;` |
|  182293437 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  412316244 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  412316249 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   23728492 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   23728492 |   33 | `		if( pSet->nSize <= 0 ){` |
|   20181938 |   34 | `			pSet->nSize = 4;` |
|   10091641 |   35 | `		}` |
|   23728492 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   23728492 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   23728492 |   40 | `		pSet->pBase = pNew;` |
|   23728492 |   41 | `		pSet->nSize <<= 1;` |
|   11864918 |   42 | `	}` |
|  412316249 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3256896045 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  412316249 |   45 | `	pSet->nUsed++;` |
|  412316249 |   46 | `	return SXRET_OK;` |
|  206160587 |   47 | `}` |
|   20479598 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   20479603 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   20479603 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   20479603 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   20479603 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   20479603 |   60 | `	pSet->nSize = nItem;` |
|   20479603 |   61 | `	return SXRET_OK;` |
|   10239804 |   62 | `}` |
|   30420529 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   30420534 |   65 | `	pSet->nUsed   = 0;` |
|   30420534 |   66 | `	pSet->nCursor = 0;` |
|   30420534 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      71640 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      71645 |   71 | `	pSet->nCursor = 0;` |
|      71645 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      72062 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      72067 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      31011 |   79 | `		pSet->nCursor = 0;` |
|      31011 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      41061 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      41061 |   83 | `	if( ppEntry ){` |
|      41061 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      20528 |   85 | `	}` |
|      41061 |   86 | `	pSet->nCursor++;` |
|      41061 |   87 | `	return SXRET_OK;` |
|      36036 |   88 | `}` |
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
|    3209544 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3209549 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1213 |  103 | `		pSet->nUsed = nNewSize;` |
|        604 |  104 | `	}` |
|    3209549 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   62235266 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   62235271 |  109 | `	sxi32 rc = SXRET_OK;` |
|   62235271 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   33632600 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   16816972 |  112 | `	}` |
|   62235271 |  113 | `	pSet->pBase = 0;` |
|   62235271 |  114 | `	pSet->nUsed = 0;` |
|   62235271 |  115 | `	pSet->nCursor = 0;` |
|   62235271 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   73413098 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   73413103 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19539 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   73393569 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   73393569 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   36706554 |  126 | `}` |
|    9866851 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9866856 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2236865 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7629996 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7629996 |  135 | `	pSet->nUsed--;` |
|    7629996 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7629996 |  137 | `	return pData;` |
|    4933880 |  138 | `}` |
|   37287130 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   37287135 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   37287083 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   37287083 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   18647713 |  148 | `}` |
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
|    2108138 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    2108143 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2108143 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    2108143 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2108143 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    2108143 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    2108143 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    2108143 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    2108143 |  180 | `	pHash->nEntry = 0;` |
|    2108143 |  181 | `	pHash->apBucket = apNew;` |
|    2108143 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    2108143 |  183 | `	return SXRET_OK;` |
|    1054149 |  184 | `}` |
|     506190 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     506195 |  193 | `	pEntry = pHash->pList;` |
|     268019 |  194 | `	for(;;){` |
|     535893 |  195 | `		if( pHash->nEntry == 0 ){` |
|     506195 |  196 | `			break;` |
|          - |  197 | `		}` |
|      29703 |  198 | `		pNext = pEntry->pNext;` |
|      29703 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      29703 |  200 | `		pEntry = pNext;` |
|      29703 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     506195 |  203 | `	if( pHash->apBucket ){` |
|     506195 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     253170 |  205 | `	}` |
|     506195 |  206 | `	pHash->apBucket = 0;` |
|     506195 |  207 | `	pHash->nBucketSize = 0;` |
|     506195 |  208 | `	pHash->pAllocator = 0;` |
|     506195 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   76491228 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   76491233 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   76491233 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   68873006 |  218 | `	for(;;){` |
|  137985415 |  219 | `		if( pEntry == 0 ){` |
|   28359611 |  220 | `			break;` |
|          - |  221 | `		}` |
|  133690731 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   48135670 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   48131627 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   61494187 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   28359611 |  229 | `	return 0;` |
|   38252258 |  230 | `}` |
|   84509682 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   84509687 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    8018899 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   76490793 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   76490793 |  244 | `	if( pEntry == 0 ){` |
|   28359593 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   48131205 |  247 | `	return (SyHashEntry *)pEntry;` |
|   42261560 |  248 | `}` |
|     491900 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     491905 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     402502 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     201628 |  254 | `	}else{` |
|      89408 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     491905 |  257 | `	if( pEntry->pNextCollide ){` |
|       4850 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2423 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     491905 |  261 | `	if( pHash->pLast == pEntry ){` |
|     481821 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     241358 |  263 | `	}` |
|     491905 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     491905 |  265 | `	pHash->nEntry--;` |
|     491905 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     491905 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     491905 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        440 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        445 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        445 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        427 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        427 |  288 | `	return rc;` |
|        225 |  289 | `}` |
|     491478 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     491483 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     491483 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     491483 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3351230 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3351235 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3351235 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   26049558 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   26049563 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3350969 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3350969 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   22698599 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   22698599 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   22698599 |  328 | `	return (SyHashEntry *)pEntry;` |
|   13024784 |  329 | `}` |
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
|       4867 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4853 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4853 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4853 |  348 | `		pEntry = pEntry->pNext;` |
|       2427 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|     100788 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100793 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100793 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100793 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100793 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18675833 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18575045 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18575045 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18575045 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18575045 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8949512 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4474878 |  375 | `		}` |
|   18575045 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18575045 |  378 | `		pEntry = pEntry->pNext;` |
|    9287525 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100793 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100793 |  382 | `	pHash->apBucket = apNew;` |
|     100793 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100793 |  384 | `	return SXRET_OK;` |
|      50399 |  385 | `}` |
|   22234420 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   22234425 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   22234425 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   22234425 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   14077545 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    7038754 |  393 | `	}` |
|   22234425 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   22234425 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         63 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         63 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         63 |  401 | `		pHash->pLast = pEntry;` |
|         33 |  402 | `	}else{` |
|   22234365 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   22234425 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1168693 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1168693 |  408 | `		pHash->pLast = pEntry;` |
|     584419 |  409 | `	}` |
|   22234425 |  410 | `	pHash->nEntry++;` |
|   22234425 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   22234420 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   22234425 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100793 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100793 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50394 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   22234425 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   22234425 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   22234425 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   22234425 |  435 | `	pEntry->pHash = pHash;` |
|   22234425 |  436 | `	pEntry->pKey = pKey;` |
|   22234425 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   22234425 |  438 | `	pEntry->pUserData = pUserData;` |
|   22234425 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   22234425 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   22234425 |  442 | `	return rc;` |
|   11117665 |  443 | `}` |
|   22234268 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   22234273 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        152 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          4 |  455 | `{` |
|        156 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          4 |  457 | `}` |
|     531980 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     531985 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
