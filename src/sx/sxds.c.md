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
|  184252938 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  184252943 |   16 | `	pSet->nSize = 0 ;` |
|  184252943 |   17 | `	pSet->nUsed = 0;` |
|  184252943 |   18 | `	pSet->nCursor = 0;` |
|  184252943 |   19 | `	pSet->eSize = ElemSize;` |
|  184252943 |   20 | `	pSet->pAllocator = pAllocator;` |
|  184252943 |   21 | `	pSet->pBase =  0;` |
|  184252943 |   22 | `	pSet->pUserData = 0;` |
|  184252943 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  416474944 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  416474949 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   23897186 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   23897186 |   33 | `		if( pSet->nSize <= 0 ){` |
|   20339300 |   34 | `			pSet->nSize = 4;` |
|   10170322 |   35 | `		}` |
|   23897186 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   23897186 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   23897186 |   40 | `		pSet->pBase = pNew;` |
|   23897186 |   41 | `		pSet->nSize <<= 1;` |
|   11949265 |   42 | `	}` |
|  416474949 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3292762527 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  416474949 |   45 | `	pSet->nUsed++;` |
|  416474949 |   46 | `	return SXRET_OK;` |
|  208239916 |   47 | `}` |
|   20702200 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   20702205 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   20702205 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   20702205 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   20702205 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   20702205 |   60 | `	pSet->nSize = nItem;` |
|   20702205 |   61 | `	return SXRET_OK;` |
|   10351105 |   62 | `}` |
|   30742590 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   30742595 |   65 | `	pSet->nUsed   = 0;` |
|   30742595 |   66 | `	pSet->nCursor = 0;` |
|   30742595 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      70190 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      70195 |   71 | `	pSet->nCursor = 0;` |
|      70195 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      70438 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      70443 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30441 |   79 | `		pSet->nCursor = 0;` |
|      30441 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      40007 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      40007 |   83 | `	if( ppEntry ){` |
|      40007 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      20001 |   85 | `	}` |
|      40007 |   86 | `	pSet->nCursor++;` |
|      40007 |   87 | `	return SXRET_OK;` |
|      35224 |   88 | `}` |
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
|    3283154 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3283159 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1213 |  103 | `		pSet->nUsed = nNewSize;` |
|        604 |  104 | `	}` |
|    3283159 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   62775608 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   62775613 |  109 | `	sxi32 rc = SXRET_OK;` |
|   62775613 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   33845972 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   16923658 |  112 | `	}` |
|   62775613 |  113 | `	pSet->pBase = 0;` |
|   62775613 |  114 | `	pSet->nUsed = 0;` |
|   62775613 |  115 | `	pSet->nCursor = 0;` |
|   62775613 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   74184708 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   74184713 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19539 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   74165179 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   74165179 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   37092359 |  126 | `}` |
|    9867451 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9867456 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2236923 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7630538 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7630538 |  135 | `	pSet->nUsed--;` |
|    7630538 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7630538 |  137 | `	return pData;` |
|    4934180 |  138 | `}` |
|   37205893 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   37205898 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   37205846 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   37205846 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   18606939 |  148 | `}` |
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
|    2145392 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    2145397 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2145397 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    2145397 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2145397 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    2145397 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    2145397 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    2145397 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    2145397 |  180 | `	pHash->nEntry = 0;` |
|    2145397 |  181 | `	pHash->apBucket = apNew;` |
|    2145397 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    2145397 |  183 | `	return SXRET_OK;` |
|    1072776 |  184 | `}` |
|     520110 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     520115 |  193 | `	pEntry = pHash->pList;` |
|     275029 |  194 | `	for(;;){` |
|     549913 |  195 | `		if( pHash->nEntry == 0 ){` |
|     520115 |  196 | `			break;` |
|          - |  197 | `		}` |
|      29803 |  198 | `		pNext = pEntry->pNext;` |
|      29803 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      29803 |  200 | `		pEntry = pNext;` |
|      29803 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     520115 |  203 | `	if( pHash->apBucket ){` |
|     520115 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     260130 |  205 | `	}` |
|     520115 |  206 | `	pHash->apBucket = 0;` |
|     520115 |  207 | `	pHash->nBucketSize = 0;` |
|     520115 |  208 | `	pHash->pAllocator = 0;` |
|     520115 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   76196389 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   76196394 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   76196394 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   68664667 |  218 | `	for(;;){` |
|  137228222 |  219 | `		if( pEntry == 0 ){` |
|   28221249 |  220 | `			break;` |
|          - |  221 | `		}` |
|  132993759 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   47979149 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   47975150 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   61031833 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   28221249 |  229 | `	return 0;` |
|   38104580 |  230 | `}` |
|   84307035 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   84307040 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    8111091 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   76195954 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   76195954 |  244 | `	if( pEntry == 0 ){` |
|   28221231 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   47974728 |  247 | `	return (SyHashEntry *)pEntry;` |
|   42159978 |  248 | `}` |
|     489690 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     489695 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     400945 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     200850 |  254 | `	}else{` |
|      88755 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     489695 |  257 | `	if( pEntry->pNextCollide ){` |
|       4607 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2302 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     489695 |  261 | `	if( pHash->pLast == pEntry ){` |
|     479901 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     240398 |  263 | `	}` |
|     489695 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     489695 |  265 | `	pHash->nEntry--;` |
|     489695 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     489695 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     489695 |  272 | `	return rc;` |
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
|     489268 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     489273 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     489273 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     489273 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3353030 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3353035 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3353035 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   25995656 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   25995661 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3352769 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3352769 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   22642897 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   22642897 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   22642897 |  328 | `	return (SyHashEntry *)pEntry;` |
|   12997833 |  329 | `}` |
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
|       4871 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4857 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4857 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4857 |  348 | `		pEntry = pEntry->pNext;` |
|       2429 |  349 | `	}` |
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
|    8969380 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4484524 |  375 | `		}` |
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
|   22412466 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   22412471 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   22412471 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   22412471 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   14201046 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    7100785 |  393 | `	}` |
|   22412471 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   22412471 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         63 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         63 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         63 |  401 | `		pHash->pLast = pEntry;` |
|         33 |  402 | `	}else{` |
|   22412411 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   22412471 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1184083 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1184083 |  408 | `		pHash->pLast = pEntry;` |
|     592114 |  409 | `	}` |
|   22412471 |  410 | `	pHash->nEntry++;` |
|   22412471 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   22412466 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   22412471 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100793 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100793 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50394 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   22412471 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   22412471 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   22412471 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   22412471 |  435 | `	pEntry->pHash = pHash;` |
|   22412471 |  436 | `	pEntry->pKey = pKey;` |
|   22412471 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   22412471 |  438 | `	pEntry->pUserData = pUserData;` |
|   22412471 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   22412471 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   22412471 |  442 | `	return rc;` |
|   11206688 |  443 | `}` |
|   22412314 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   22412319 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     529792 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     529797 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
