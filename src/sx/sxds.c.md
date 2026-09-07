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
|  140224330 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  140224335 |   16 | `	pSet->nSize = 0 ;` |
|  140224335 |   17 | `	pSet->nUsed = 0;` |
|  140224335 |   18 | `	pSet->nCursor = 0;` |
|  140224335 |   19 | `	pSet->eSize = ElemSize;` |
|  140224335 |   20 | `	pSet->pAllocator = pAllocator;` |
|  140224335 |   21 | `	pSet->pBase =  0;` |
|  140224335 |   22 | `	pSet->pUserData = 0;` |
|  140224335 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  313856544 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  313856549 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   18545975 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   18545975 |   33 | `		if( pSet->nSize <= 0 ){` |
|   15870365 |   34 | `			pSet->nSize = 4;` |
|    7935180 |   35 | `		}` |
|   18545975 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   18545975 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   18545975 |   40 | `		pSet->pBase = pNew;` |
|   18545975 |   41 | `		pSet->nSize <<= 1;` |
|    9272985 |   42 | `	}` |
|  313856549 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2323119653 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  313856549 |   45 | `	pSet->nUsed++;` |
|  313856549 |   46 | `	return SXRET_OK;` |
|  156928299 |   47 | `}` |
|   15414226 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   15414231 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   15414231 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   15414231 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   15414231 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   15414231 |   60 | `	pSet->nSize = nItem;` |
|   15414231 |   61 | `	return SXRET_OK;` |
|    7707118 |   62 | `}` |
|   22218160 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   22218165 |   65 | `	pSet->nUsed   = 0;` |
|   22218165 |   66 | `	pSet->nCursor = 0;` |
|   22218165 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69010 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69015 |   71 | `	pSet->nCursor = 0;` |
|      69015 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73150 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73155 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29839 |   79 | `		pSet->nCursor = 0;` |
|      29839 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43321 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43321 |   83 | `	if( ppEntry ){` |
|      43321 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21658 |   85 | `	}` |
|      43321 |   86 | `	pSet->nCursor++;` |
|      43321 |   87 | `	return SXRET_OK;` |
|      36580 |   88 | `}` |
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
|    2532854 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2532859 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1181 |  103 | `		pSet->nUsed = nNewSize;` |
|        588 |  104 | `	}` |
|    2532859 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   48464370 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   48464375 |  109 | `	sxi32 rc = SXRET_OK;` |
|   48464375 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   25911315 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   12955655 |  112 | `	}` |
|   48464375 |  113 | `	pSet->pBase = 0;` |
|   48464375 |  114 | `	pSet->nUsed = 0;` |
|   48464375 |  115 | `	pSet->nCursor = 0;` |
|   48464375 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   56500688 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   56500693 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15301 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   56485397 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   56485397 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   28250349 |  126 | `}` |
|    7939464 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    7939469 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2218193 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    5721281 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    5721281 |  135 | `	pSet->nUsed--;` |
|    5721281 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    5721281 |  137 | `	return pData;` |
|    3969737 |  138 | `}` |
|   29516023 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   29516028 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   29516006 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   29516006 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   14758119 |  148 | `}` |
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
|    1745118 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1745123 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1745123 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1745123 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1745123 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1745123 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1745123 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1745123 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1745123 |  180 | `	pHash->nEntry = 0;` |
|    1745123 |  181 | `	pHash->apBucket = apNew;` |
|    1745123 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1745123 |  183 | `	return SXRET_OK;` |
|     872564 |  184 | `}` |
|     396676 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     396681 |  193 | `	pEntry = pHash->pList;` |
|     211135 |  194 | `	for(;;){` |
|     422275 |  195 | `		if( pHash->nEntry == 0 ){` |
|     396681 |  196 | `			break;` |
|          - |  197 | `		}` |
|      25599 |  198 | `		pNext = pEntry->pNext;` |
|      25599 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      25599 |  200 | `		pEntry = pNext;` |
|      25599 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     396681 |  203 | `	if( pHash->apBucket ){` |
|     396681 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     198338 |  205 | `	}` |
|     396681 |  206 | `	pHash->apBucket = 0;` |
|     396681 |  207 | `	pHash->nBucketSize = 0;` |
|     396681 |  208 | `	pHash->pAllocator = 0;` |
|     396681 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   61028501 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   61028506 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   61028506 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   56133214 |  218 | `	for(;;){` |
|  112526229 |  219 | `		if( pEntry == 0 ){` |
|   22884204 |  220 | `			break;` |
|          - |  221 | `		}` |
|  108714101 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   38144406 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   38144307 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   51497728 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   22884204 |  229 | `	return 0;` |
|   30514521 |  230 | `}` |
|   67170491 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   67170496 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    6142347 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   61028154 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   61028154 |  244 | `	if( pEntry == 0 ){` |
|   22884186 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   38143973 |  247 | `	return (SyHashEntry *)pEntry;` |
|   33585516 |  248 | `}` |
|     368692 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     368697 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     301968 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     150987 |  254 | `	}else{` |
|      66734 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     368697 |  257 | `	if( pEntry->pNextCollide ){` |
|       4384 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2190 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     368697 |  261 | `	if( pHash->pLast == pEntry ){` |
|     361655 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     180825 |  263 | `	}` |
|     368697 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     368697 |  265 | `	pHash->nEntry--;` |
|     368697 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     368697 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     368697 |  272 | `	return rc;` |
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
|     368358 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     368363 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     368363 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     368363 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2808394 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2808399 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2808399 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   20863568 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   20863573 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2808133 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2808133 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18055445 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18055445 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18055445 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10431789 |  329 | `}` |
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
|       3833 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3823 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3823 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3823 |  348 | `		pEntry = pEntry->pNext;` |
|       1912 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      90834 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      90839 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      90839 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      90839 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      90839 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14286647 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14195813 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14195813 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14195813 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14195813 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    6791865 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3395839 |  375 | `		}` |
|   14195813 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14195813 |  378 | `		pEntry = pEntry->pNext;` |
|    7097909 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      90839 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      90839 |  382 | `	pHash->apBucket = apNew;` |
|      90839 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      90839 |  384 | `	return SXRET_OK;` |
|      45422 |  385 | `}` |
|   17356422 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   17356427 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   17356427 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   17356427 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   10810187 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5405413 |  393 | `	}` |
|   17356427 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   17356427 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   17356375 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   17356427 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     957365 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     957365 |  408 | `		pHash->pLast = pEntry;` |
|     478680 |  409 | `	}` |
|   17356427 |  410 | `	pHash->nEntry++;` |
|   17356427 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   17356422 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   17356427 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      90839 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      90839 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45417 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   17356427 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   17356427 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   17356427 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   17356427 |  435 | `	pEntry->pHash = pHash;` |
|   17356427 |  436 | `	pEntry->pKey = pKey;` |
|   17356427 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   17356427 |  438 | `	pEntry->pUserData = pUserData;` |
|   17356427 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   17356427 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   17356427 |  442 | `	return rc;` |
|    8678216 |  443 | `}` |
|   17356290 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   17356295 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     407794 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     407799 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
