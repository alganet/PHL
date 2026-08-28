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
|  143849924 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  143849929 |   16 | `	pSet->nSize = 0 ;` |
|  143849929 |   17 | `	pSet->nUsed = 0;` |
|  143849929 |   18 | `	pSet->nCursor = 0;` |
|  143849929 |   19 | `	pSet->eSize = ElemSize;` |
|  143849929 |   20 | `	pSet->pAllocator = pAllocator;` |
|  143849929 |   21 | `	pSet->pBase =  0;` |
|  143849929 |   22 | `	pSet->pUserData = 0;` |
|  143849929 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  322282409 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  322282414 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   18776563 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   18776563 |   33 | `		if( pSet->nSize <= 0 ){` |
|   16036831 |   34 | `			pSet->nSize = 4;` |
|    8018413 |   35 | `		}` |
|   18776563 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   18776563 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   18776563 |   40 | `		pSet->pBase = pNew;` |
|   18776563 |   41 | `		pSet->nSize <<= 1;` |
|    9388279 |   42 | `	}` |
|  322282414 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2389955834 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  322282414 |   45 | `	pSet->nUsed++;` |
|  322282414 |   46 | `	return SXRET_OK;` |
|  161141253 |   47 | `}` |
|   15899466 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   15899471 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   15899471 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   15899471 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   15899471 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   15899471 |   60 | `	pSet->nSize = nItem;` |
|   15899471 |   61 | `	return SXRET_OK;` |
|    7949738 |   62 | `}` |
|   22824615 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   22824620 |   65 | `	pSet->nUsed   = 0;` |
|   22824620 |   66 | `	pSet->nCursor = 0;` |
|   22824620 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69538 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69543 |   71 | `	pSet->nCursor = 0;` |
|      69543 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73780 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73785 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29939 |   79 | `		pSet->nCursor = 0;` |
|      29939 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43851 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43851 |   83 | `	if( ppEntry ){` |
|      43851 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21923 |   85 | `	}` |
|      43851 |   86 | `	pSet->nCursor++;` |
|      43851 |   87 | `	return SXRET_OK;` |
|      36895 |   88 | `}` |
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
|    2612750 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2612755 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1179 |  103 | `		pSet->nUsed = nNewSize;` |
|        587 |  104 | `	}` |
|    2612755 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   49402532 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   49402537 |  109 | `	sxi32 rc = SXRET_OK;` |
|   49402537 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   26396045 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   13198020 |  112 | `	}` |
|   49402537 |  113 | `	pSet->pBase = 0;` |
|   49402537 |  114 | `	pSet->nUsed = 0;` |
|   49402537 |  115 | `	pSet->nCursor = 0;` |
|   49402537 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   58207548 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   58207553 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15781 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   58191777 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   58191777 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   29103779 |  126 | `}` |
|    7933772 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    7933777 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2222713 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    5711069 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    5711069 |  135 | `	pSet->nUsed--;` |
|    5711069 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    5711069 |  137 | `	return pData;` |
|    3966891 |  138 | `}` |
|   28626597 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   28626602 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   28626580 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   28626580 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   14313620 |  148 | `}` |
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
|    1774972 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1774977 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1774977 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1774977 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1774977 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1774977 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1774977 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1774977 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1774977 |  180 | `	pHash->nEntry = 0;` |
|    1774977 |  181 | `	pHash->apBucket = apNew;` |
|    1774977 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1774977 |  183 | `	return SXRET_OK;` |
|     887491 |  184 | `}` |
|     384196 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     384201 |  193 | `	pEntry = pHash->pList;` |
|     204786 |  194 | `	for(;;){` |
|     409577 |  195 | `		if( pHash->nEntry == 0 ){` |
|     384201 |  196 | `			break;` |
|          - |  197 | `		}` |
|      25381 |  198 | `		pNext = pEntry->pNext;` |
|      25381 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      25381 |  200 | `		pEntry = pNext;` |
|      25381 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     384201 |  203 | `	if( pHash->apBucket ){` |
|     384201 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     192098 |  205 | `	}` |
|     384201 |  206 | `	pHash->apBucket = 0;` |
|     384201 |  207 | `	pHash->nBucketSize = 0;` |
|     384201 |  208 | `	pHash->pAllocator = 0;` |
|     384201 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   60609447 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   60609452 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   60609452 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   55962967 |  218 | `	for(;;){` |
|  111942418 |  219 | `		if( pEntry == 0 ){` |
|   22293106 |  220 | `			break;` |
|          - |  221 | `		}` |
|  108807281 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   38316450 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   38316351 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   51332971 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   22293106 |  229 | `	return 0;` |
|   30305252 |  230 | `}` |
|   66919323 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   66919328 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    6310233 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   60609100 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   60609100 |  244 | `	if( pEntry == 0 ){` |
|   22293088 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   38316017 |  247 | `	return (SyHashEntry *)pEntry;` |
|   33460190 |  248 | `}` |
|     232374 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     232379 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     187781 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      93893 |  254 | `	}else{` |
|      44603 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     232379 |  257 | `	if( pEntry->pNextCollide ){` |
|       4414 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2205 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     232379 |  261 | `	if( pHash->pLast == pEntry ){` |
|     225321 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     112658 |  263 | `	}` |
|     232379 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     232379 |  265 | `	pHash->nEntry--;` |
|     232379 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     232379 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     232379 |  272 | `	return rc;` |
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
|     232040 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     232045 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     232045 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     232045 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2913068 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2913073 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2913073 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   21668718 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   21668723 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2912807 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2912807 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18755921 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18755921 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18755921 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10834364 |  329 | `}` |
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
|       3791 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3781 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3781 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3781 |  348 | `		pEntry = pEntry->pNext;` |
|       1891 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      94428 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      94433 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      94433 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      94433 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      94433 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14873441 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14779013 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14779013 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14779013 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14779013 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    7077737 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3538765 |  375 | `		}` |
|   14779013 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14779013 |  378 | `		pEntry = pEntry->pNext;` |
|    7389509 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      94433 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      94433 |  382 | `	pHash->apBucket = apNew;` |
|      94433 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      94433 |  384 | `	return SXRET_OK;` |
|      47219 |  385 | `}` |
|   17797236 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   17797241 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   17797241 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   17797241 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11153350 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5576584 |  393 | `	}` |
|   17797241 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   17797241 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   17797189 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   17797241 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     962553 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     962553 |  408 | `		pHash->pLast = pEntry;` |
|     481274 |  409 | `	}` |
|   17797241 |  410 | `	pHash->nEntry++;` |
|   17797241 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   17797236 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   17797241 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      94433 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      94433 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      47214 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   17797241 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   17797241 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   17797241 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   17797241 |  435 | `	pEntry->pHash = pHash;` |
|   17797241 |  436 | `	pEntry->pKey = pKey;` |
|   17797241 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   17797241 |  438 | `	pEntry->pUserData = pUserData;` |
|   17797241 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   17797241 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   17797241 |  442 | `	return rc;` |
|    8898623 |  443 | `}` |
|   17797104 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   17797109 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     273478 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     273483 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
