# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 290/304 lines (95.39%)

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
|  105582450 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  105582455 |   16 | `	pSet->nSize = 0 ;` |
|  105582455 |   17 | `	pSet->nUsed = 0;` |
|  105582455 |   18 | `	pSet->nCursor = 0;` |
|  105582455 |   19 | `	pSet->eSize = ElemSize;` |
|  105582455 |   20 | `	pSet->pAllocator = pAllocator;` |
|  105582455 |   21 | `	pSet->pBase =  0;` |
|  105582455 |   22 | `	pSet->pUserData = 0;` |
|  105582455 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  232602853 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  232602858 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   14623251 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   14623251 |   33 | `		if( pSet->nSize <= 0 ){` |
|   12709913 |   34 | `			pSet->nSize = 4;` |
|    6354954 |   35 | `		}` |
|   14623251 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   14623251 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   14623251 |   40 | `		pSet->pBase = pNew;` |
|   14623251 |   41 | `		pSet->nSize <<= 1;` |
|    7311623 |   42 | `	}` |
|  232602858 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 1724555158 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  232602858 |   45 | `	pSet->nUsed++;` |
|  232602858 |   46 | `	return SXRET_OK;` |
|  116301474 |   47 | `}` |
|   11266956 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   11266961 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   11266961 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   11266961 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   11266961 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   11266961 |   60 | `	pSet->nSize = nItem;` |
|   11266961 |   61 | `	return SXRET_OK;` |
|    5633483 |   62 | `}` |
|   16949735 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   16949740 |   65 | `	pSet->nUsed   = 0;` |
|   16949740 |   66 | `	pSet->nCursor = 0;` |
|   16949740 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69226 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69231 |   71 | `	pSet->nCursor = 0;` |
|      69231 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73464 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73469 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29797 |   79 | `		pSet->nCursor = 0;` |
|      29797 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43677 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43677 |   83 | `	if( ppEntry ){` |
|      43677 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21836 |   85 | `	}` |
|      43677 |   86 | `	pSet->nCursor++;` |
|      43677 |   87 | `	return SXRET_OK;` |
|      36737 |   88 | `}` |
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
|    1818656 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    1818661 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1179 |  103 | `		pSet->nUsed = nNewSize;` |
|        587 |  104 | `	}` |
|    1818661 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   37261668 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   37261673 |  109 | `	sxi32 rc = SXRET_OK;` |
|   37261673 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   19959463 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|    9979729 |  112 | `	}` |
|   37261673 |  113 | `	pSet->pBase = 0;` |
|   37261673 |  114 | `	pSet->nUsed = 0;` |
|   37261673 |  115 | `	pSet->nCursor = 0;` |
|   37261673 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   41065674 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   41065679 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15621 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   41050063 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   41050063 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   20532842 |  126 | `}` |
|    6705612 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    6705617 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2198973 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    4506649 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    4506649 |  135 | `	pSet->nUsed--;` |
|    4506649 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    4506649 |  137 | `	return pData;` |
|    3352811 |  138 | `}` |
|   24131912 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   24131917 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   24131895 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   24131895 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   12066313 |  148 | `}` |
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
|    1392422 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1392427 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1392427 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1392427 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1392427 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1392427 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1392427 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1392427 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1392427 |  180 | `	pHash->nEntry = 0;` |
|    1392427 |  181 | `	pHash->apBucket = apNew;` |
|    1392427 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1392427 |  183 | `	return SXRET_OK;` |
|     696216 |  184 | `}` |
|     345244 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     345249 |  193 | `	pEntry = pHash->pList;` |
|     184593 |  194 | `	for(;;){` |
|     369191 |  195 | `		if( pHash->nEntry == 0 ){` |
|     345249 |  196 | `			break;` |
|          - |  197 | `		}` |
|      23947 |  198 | `		pNext = pEntry->pNext;` |
|      23947 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      23947 |  200 | `		pEntry = pNext;` |
|      23947 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     345249 |  203 | `	if( pHash->apBucket ){` |
|     345249 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     172622 |  205 | `	}` |
|     345249 |  206 | `	pHash->apBucket = 0;` |
|     345249 |  207 | `	pHash->nBucketSize = 0;` |
|     345249 |  208 | `	pHash->pAllocator = 0;` |
|     345249 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   48029297 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   48029302 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   48029302 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   45279155 |  218 | `	for(;;){` |
|   90597830 |  219 | `		if( pEntry == 0 ){` |
|   18612734 |  220 | `			break;` |
|          - |  221 | `		}` |
|   86693162 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   29416632 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   29416573 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   42568533 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   18612734 |  229 | `	return 0;` |
|   24015165 |  230 | `}` |
|   52508091 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   52508096 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    4479121 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   48028980 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   48028980 |  244 | `	if( pEntry == 0 ){` |
|   18612734 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   29416251 |  247 | `	return (SyHashEntry *)pEntry;` |
|   26254562 |  248 | `}` |
|     221250 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     221255 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     177675 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      88840 |  254 | `	}else{` |
|      43585 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     221255 |  257 | `	if( pEntry->pNextCollide ){` |
|       4268 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2133 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     221255 |  261 | `	if( pHash->pLast == pEntry ){` |
|     214379 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     107187 |  263 | `	}` |
|     221255 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     221255 |  265 | `	pHash->nEntry--;` |
|     221255 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|        ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        ! 0 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     221255 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     221255 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        322 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        327 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        327 |  284 | `	if( pEntry == 0 ){` |
|        ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        327 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        327 |  288 | `	return rc;` |
|        166 |  289 | `}` |
|     220928 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     220933 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     220933 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     220933 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2221190 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2221195 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2221195 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   16688946 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   16688951 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2220929 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2220929 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   14468027 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   14468027 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   14468027 |  328 | `	return (SyHashEntry *)pEntry;` |
|    8344478 |  329 | `}` |
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
|       3365 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3355 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3355 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3355 |  348 | `		pEntry = pEntry->pNext;` |
|       1678 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      82550 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      82555 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      82555 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      82555 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      82555 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   10756027 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   10673477 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   10673477 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   10673477 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   10673477 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    5118322 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    2559148 |  375 | `		}` |
|   10673477 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   10673477 |  378 | `		pEntry = pEntry->pNext;` |
|    5336741 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      82555 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      82555 |  382 | `	pHash->apBucket = apNew;` |
|      82555 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      82555 |  384 | `	return SXRET_OK;` |
|      41280 |  385 | `}` |
|   14022086 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   14022091 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   14022091 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   14022091 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    8769654 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    4384752 |  393 | `	}` |
|   14022091 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   14022091 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   14022039 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   14022091 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     739267 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     739267 |  408 | `		pHash->pLast = pEntry;` |
|     369631 |  409 | `	}` |
|   14022091 |  410 | `	pHash->nEntry++;` |
|   14022091 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   14022086 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   14022091 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      82555 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      82555 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      41275 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   14022091 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   14022091 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   14022091 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   14022091 |  435 | `	pEntry->pHash = pHash;` |
|   14022091 |  436 | `	pEntry->pKey = pKey;` |
|   14022091 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   14022091 |  438 | `	pEntry->pUserData = pUserData;` |
|   14022091 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   14022091 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   14022091 |  442 | `	return rc;` |
|    7011048 |  443 | `}` |
|   14021958 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   14021963 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        128 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          2 |  455 | `{` |
|        130 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          2 |  457 | `}` |
|     261906 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     261911 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
