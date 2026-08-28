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
|  144248094 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  144248099 |   16 | `	pSet->nSize = 0 ;` |
|  144248099 |   17 | `	pSet->nUsed = 0;` |
|  144248099 |   18 | `	pSet->nCursor = 0;` |
|  144248099 |   19 | `	pSet->eSize = ElemSize;` |
|  144248099 |   20 | `	pSet->pAllocator = pAllocator;` |
|  144248099 |   21 | `	pSet->pBase =  0;` |
|  144248099 |   22 | `	pSet->pUserData = 0;` |
|  144248099 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  323084455 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  323084460 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   18817721 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   18817721 |   33 | `		if( pSet->nSize <= 0 ){` |
|   16071035 |   34 | `			pSet->nSize = 4;` |
|    8035515 |   35 | `		}` |
|   18817721 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   18817721 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   18817721 |   40 | `		pSet->pBase = pNew;` |
|   18817721 |   41 | `		pSet->nSize <<= 1;` |
|    9408858 |   42 | `	}` |
|  323084460 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2395898588 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  323084460 |   45 | `	pSet->nUsed++;` |
|  323084460 |   46 | `	return SXRET_OK;` |
|  161542276 |   47 | `}` |
|   15940046 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   15940051 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   15940051 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   15940051 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   15940051 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   15940051 |   60 | `	pSet->nSize = nItem;` |
|   15940051 |   61 | `	return SXRET_OK;` |
|    7970028 |   62 | `}` |
|   22881339 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   22881344 |   65 | `	pSet->nUsed   = 0;` |
|   22881344 |   66 | `	pSet->nCursor = 0;` |
|   22881344 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69590 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69595 |   71 | `	pSet->nCursor = 0;` |
|      69595 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73844 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73849 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29965 |   79 | `		pSet->nCursor = 0;` |
|      29965 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43889 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43889 |   83 | `	if( ppEntry ){` |
|      43889 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21942 |   85 | `	}` |
|      43889 |   86 | `	pSet->nCursor++;` |
|      43889 |   87 | `	return SXRET_OK;` |
|      36927 |   88 | `}` |
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
|    2619410 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2619415 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1179 |  103 | `		pSet->nUsed = nNewSize;` |
|        587 |  104 | `	}` |
|    2619415 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   49515314 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   49515319 |  109 | `	sxi32 rc = SXRET_OK;` |
|   49515319 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   26456639 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   13228317 |  112 | `	}` |
|   49515319 |  113 | `	pSet->pBase = 0;` |
|   49515319 |  114 | `	pSet->nUsed = 0;` |
|   49515319 |  115 | `	pSet->nCursor = 0;` |
|   49515319 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   58351184 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   58351189 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15821 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   58335373 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   58335373 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   29175597 |  126 | `}` |
|    7947536 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    7947541 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2223051 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    5724495 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    5724495 |  135 | `	pSet->nUsed--;` |
|    5724495 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    5724495 |  137 | `	return pData;` |
|    3973773 |  138 | `}` |
|   28676532 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   28676537 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   28676515 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   28676515 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   14338595 |  148 | `}` |
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
|    1779268 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1779273 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1779273 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1779273 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1779273 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1779273 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1779273 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1779273 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1779273 |  180 | `	pHash->nEntry = 0;` |
|    1779273 |  181 | `	pHash->apBucket = apNew;` |
|    1779273 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1779273 |  183 | `	return SXRET_OK;` |
|     889639 |  184 | `}` |
|     384962 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     384967 |  193 | `	pEntry = pHash->pList;` |
|     205181 |  194 | `	for(;;){` |
|     410367 |  195 | `		if( pHash->nEntry == 0 ){` |
|     384967 |  196 | `			break;` |
|          - |  197 | `		}` |
|      25405 |  198 | `		pNext = pEntry->pNext;` |
|      25405 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      25405 |  200 | `		pEntry = pNext;` |
|      25405 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     384967 |  203 | `	if( pHash->apBucket ){` |
|     384967 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     192481 |  205 | `	}` |
|     384967 |  206 | `	pHash->apBucket = 0;` |
|     384967 |  207 | `	pHash->nBucketSize = 0;` |
|     384967 |  208 | `	pHash->pAllocator = 0;` |
|     384967 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   60822941 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   60822946 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   60822946 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   56264117 |  218 | `	for(;;){` |
|  112503237 |  219 | `		if( pEntry == 0 ){` |
|   22386176 |  220 | `			break;` |
|          - |  221 | `		}` |
|  109335242 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   38436874 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   38436775 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   51680296 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   22386176 |  229 | `	return 0;` |
|   30411999 |  230 | `}` |
|   67148839 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   67148844 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    6326255 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   60822594 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   60822594 |  244 | `	if( pEntry == 0 ){` |
|   22386158 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   38436441 |  247 | `	return (SyHashEntry *)pEntry;` |
|   33574948 |  248 | `}` |
|     232532 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     232537 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     187909 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      93957 |  254 | `	}else{` |
|      44633 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     232537 |  257 | `	if( pEntry->pNextCollide ){` |
|       4396 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2197 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     232537 |  261 | `	if( pHash->pLast == pEntry ){` |
|     225477 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     112736 |  263 | `	}` |
|     232537 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     232537 |  265 | `	pHash->nEntry--;` |
|     232537 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     232537 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     232537 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        352 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        357 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        357 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        339 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        339 |  288 | `	return rc;` |
|        181 |  289 | `}` |
|     232198 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     232203 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     232203 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     232203 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2920082 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2920087 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2920087 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   21719722 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   21719727 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2919821 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2919821 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18799911 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18799911 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18799911 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10859866 |  329 | `}` |
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
|       3817 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3807 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3807 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3807 |  348 | `		pEntry = pEntry->pNext;` |
|       1904 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      94678 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      94683 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      94683 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      94683 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      94683 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14913051 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14818373 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14818373 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14818373 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14818373 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    7089733 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3544956 |  375 | `		}` |
|   14818373 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14818373 |  378 | `		pEntry = pEntry->pNext;` |
|    7409189 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      94683 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      94683 |  382 | `	pHash->apBucket = apNew;` |
|      94683 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      94683 |  384 | `	return SXRET_OK;` |
|      47344 |  385 | `}` |
|   17889642 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   17889647 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   17889647 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   17889647 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11229029 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5614381 |  393 | `	}` |
|   17889647 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   17889647 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   17889595 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   17889647 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     964871 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     964871 |  408 | `		pHash->pLast = pEntry;` |
|     482433 |  409 | `	}` |
|   17889647 |  410 | `	pHash->nEntry++;` |
|   17889647 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   17889642 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   17889647 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      94683 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      94683 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      47339 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   17889647 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   17889647 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   17889647 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   17889647 |  435 | `	pEntry->pHash = pHash;` |
|   17889647 |  436 | `	pEntry->pKey = pKey;` |
|   17889647 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   17889647 |  438 | `	pEntry->pUserData = pUserData;` |
|   17889647 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   17889647 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   17889647 |  442 | `	return rc;` |
|    8944826 |  443 | `}` |
|   17889510 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   17889515 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        132 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          2 |  455 | `{` |
|        134 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          2 |  457 | `}` |
|     273766 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     273771 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
