# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 289/304 lines (95.07%)

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
|  19841748 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 | `{` |
|  19841753 |   16 | `	pSet->nSize = 0 ;` |
|  19841753 |   17 | `	pSet->nUsed = 0;` |
|  19841753 |   18 | `	pSet->nCursor = 0;` |
|  19841753 |   19 | `	pSet->eSize = ElemSize;` |
|  19841753 |   20 | `	pSet->pAllocator = pAllocator;` |
|  19841753 |   21 | `	pSet->pBase =  0;` |
|  19841753 |   22 | `	pSet->pUserData = 0;` |
|  19841753 |   23 | `	return SXRET_OK;` |
|         5 |   24 | `}` |
|  32810527 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 | `{` |
|         - |   27 | `	unsigned char *zbase;` |
|  32810532 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4674791 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4674791 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4512403 |   34 | `			pSet->nSize = 4;` |
|   2256199 |   35 | `		}` |
|   4674791 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4674791 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4674791 |   40 | `		pSet->pBase = pNew;` |
|   4674791 |   41 | `		pSet->nSize <<= 1;` |
|   2337393 |   42 | `	}` |
|  32810532 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 245849348 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  32810532 |   45 | `	pSet->nUsed++;` |
|  32810532 |   46 | `	return SXRET_OK;` |
|  16405312 |   47 | `}` |
|   1353394 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 | `{` |
|   1353399 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1353399 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1353399 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1353399 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1353399 |   60 | `	pSet->nSize = nItem;` |
|   1353399 |   61 | `	return SXRET_OK;` |
|    676702 |   62 | `}` |
|   1864649 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 | `{` |
|   1864654 |   65 | `	pSet->nUsed   = 0;` |
|   1864654 |   66 | `	pSet->nCursor = 0;` |
|   1864654 |   67 | `	return SXRET_OK;` |
|         5 |   68 | `}` |
|     58678 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 | `{` |
|     58683 |   71 | `	pSet->nCursor = 0;` |
|     58683 |   72 | `	return SXRET_OK;` |
|         5 |   73 | `}` |
|     62882 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 | `{` |
|         - |   76 | `	register unsigned char *zSrc;` |
|     62887 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     24333 |   79 | `		pSet->nCursor = 0;` |
|     24333 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     38559 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     38559 |   83 | `	if( ppEntry ){` |
|     38559 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     19277 |   85 | `	}` |
|     38559 |   86 | `	pSet->nCursor++;` |
|     38559 |   87 | `	return SXRET_OK;` |
|     31446 |   88 | `}` |
|         - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         8 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|         1 |   91 | `{` |
|         - |   92 | `	register unsigned char *zSrc;` |
|         9 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         3 |   94 | `		return 0;` |
|         - |   95 | `	}` |
|         7 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|         7 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|         5 |   98 | `}` |
|         - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    227892 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 | `{` |
|    227897 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       275 |  103 | `		pSet->nUsed = nNewSize;` |
|       135 |  104 | `	}` |
|    227897 |  105 | `	return SXRET_OK;` |
|         5 |  106 | `}` |
|  10201058 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 | `{` |
|  10201063 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10201063 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5117349 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2558672 |  112 | `	}` |
|  10201063 |  113 | `	pSet->pBase = 0;` |
|  10201063 |  114 | `	pSet->nUsed = 0;` |
|  10201063 |  115 | `	pSet->nCursor = 0;` |
|  10201063 |  116 | `	return rc;` |
|         5 |  117 | `}` |
|   5937980 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 | `{` |
|         - |  120 | `	const char *zBase;` |
|   5937985 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       133 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5937857 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5937857 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2968995 |  126 | `}` |
|   3628804 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 | `{` |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3628809 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2184649 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1444165 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1444165 |  135 | `	pSet->nUsed--;` |
|   1444165 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1444165 |  137 | `	return pData;` |
|   1814407 |  138 | `}` |
|  13684676 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	const char *zBase;` |
|  13684681 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13684681 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13684681 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6842711 |  148 | `}` |
|         - |  149 | `/* Private hash entry */` |
|         - |  150 | `struct SyHashEntry_Pr` |
|         - |  151 | `{` |
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
|    596250 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 | `{` |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    596255 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    596255 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    596255 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    596255 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    596255 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    596255 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    596255 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    596255 |  180 | `	pHash->nEntry = 0;` |
|    596255 |  181 | `	pHash->apBucket = apNew;` |
|    596255 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    596255 |  183 | `	return SXRET_OK;` |
|    298130 |  184 | `}` |
|    107190 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 | `{` |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    107195 |  193 | `	pEntry = pHash->pList;` |
|     57543 |  194 | `	for(;;){` |
|    115091 |  195 | `		if( pHash->nEntry == 0 ){` |
|    107195 |  196 | `			break;` |
|         - |  197 | `		}` |
|      7901 |  198 | `		pNext = pEntry->pNext;` |
|      7901 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      7901 |  200 | `		pEntry = pNext;` |
|      7901 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    107195 |  203 | `	if( pHash->apBucket ){` |
|    107195 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     53595 |  205 | `	}` |
|    107195 |  206 | `	pHash->apBucket = 0;` |
|    107195 |  207 | `	pHash->nBucketSize = 0;` |
|    107195 |  208 | `	pHash->pAllocator = 0;` |
|    107195 |  209 | `	return SXRET_OK;` |
|         5 |  210 | `}` |
|  18020236 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 | `{` |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  18020241 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  18020241 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  16369908 |  218 | `	for(;;){` |
|  32695592 |  219 | `		if( pEntry == 0 ){` |
|   9589991 |  220 | `			break;` |
|         - |  221 | `		}` |
|  27320475 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   8430260 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   8430255 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  14675356 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   9589991 |  229 | `	return 0;` |
|   9010645 |  230 | `}` |
|  18920202 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 | `{` |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  18920207 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    900181 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  18020031 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  18020031 |  244 | `	if( pEntry == 0 ){` |
|   9589991 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   8430045 |  247 | `	return (SyHashEntry *)pEntry;` |
|   9460628 |  248 | `}` |
|    133168 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 | `{` |
|         - |  251 | `	sxi32 rc;` |
|    133173 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    103237 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     51621 |  254 | `	}else{` |
|     29941 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    133173 |  257 | `	if( pEntry->pNextCollide ){` |
|      5111 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2555 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    133173 |  261 | `	if( pHash->pLast == pEntry ){` |
|    126865 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     63430 |  263 | `	}` |
|    133173 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    133173 |  265 | `	pHash->nEntry--;` |
|    133173 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    133173 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    133173 |  272 | `	return rc;` |
|         5 |  273 | `}` |
|       210 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 | `{` |
|         - |  276 | `	SyHashEntry_Pr *pEntry;` |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( INVALID_HASH(pHash) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|       215 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       215 |  284 | `	if( pEntry == 0 ){` |
|       ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|         - |  286 | `	}` |
|       215 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       215 |  288 | `	return rc;` |
|       110 |  289 | `}` |
|    132958 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 | `{` |
|    132963 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    132963 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    132963 |  300 | `	return rc;` |
|         5 |  301 | `}` |
|   1197944 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 | `{` |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1197949 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1197949 |  310 | `	return SXRET_OK;` |
|         5 |  311 | `}` |
|   7598668 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 | `{` |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   7598673 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1197687 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1197687 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   6400991 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   6400991 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   6400991 |  328 | `	return (SyHashEntry *)pEntry;` |
|   3799339 |  329 | `}` |
|        10 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         1 |  331 | `{` |
|         - |  332 | `	SyHashEntry_Pr *pEntry;` |
|         - |  333 | `	sxi32 rc;` |
|         - |  334 | `	sxu32 n;` |
|         - |  335 | `#if defined(UNTRUST)` |
|         - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  337 | `		return 0;` |
|         - |  338 | `	}` |
|         - |  339 | `#endif` |
|        11 |  340 | `	pEntry = pHash->pList;` |
|      2001 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|      1991 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1991 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|      1991 |  348 | `		pEntry = pEntry->pNext;` |
|       996 |  349 | `	}` |
|        11 |  350 | `	return SXRET_OK;` |
|         6 |  351 | `}` |
|     31238 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  353 | `{` |
|     31243 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  355 | `	SyHashEntry_Pr *pEntry;` |
|         - |  356 | `	SyHashEntry_Pr **apNew;` |
|         - |  357 | `	sxu32 n,iBucket;` |
|         - |  358 |  |
|         - |  359 | `	/* Allocate a new larger table */` |
|     31243 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     31243 |  361 | `	if( apNew == 0 ){` |
|         - |  362 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  363 | `		return SXRET_OK;` |
|         - |  364 | `	}` |
|         - |  365 | `	/* Zero the new table */` |
|     31243 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  367 | `	/* Rehash all entries */` |
|   3937675 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3906437 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  370 | `		/* Install in the new bucket */` |
|   3906437 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3906437 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3906437 |  373 | `		if( apNew[iBucket] != 0 ){` |
|   1874928 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    937518 |  375 | `		}` |
|   3906437 |  376 | `		apNew[iBucket] = pEntry;` |
|         - |  377 | `		/* Point to the next entry */` |
|   3906437 |  378 | `		pEntry = pEntry->pNext;` |
|   1953221 |  379 | `	}` |
|         - |  380 | `	/* Release the old table and reflect the change */` |
|     31243 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     31243 |  382 | `	pHash->apBucket = apNew;` |
|     31243 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     31243 |  384 | `	return SXRET_OK;` |
|     15624 |  385 | `}` |
|   5146404 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 | `{` |
|   5146409 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   5146409 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   5146409 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2910906 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1455443 |  393 | `	}` |
|   5146409 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   5146409 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        51 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        51 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        51 |  401 | `		pHash->pLast = pEntry;` |
|        26 |  402 | `	}else{` |
|   5146359 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   5146409 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    321539 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    321539 |  408 | `		pHash->pLast = pEntry;` |
|    160767 |  409 | `	}` |
|   5146409 |  410 | `	pHash->nEntry++;` |
|   5146409 |  411 | `	return SXRET_OK;` |
|         5 |  412 | `}` |
|   5146404 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 | `{` |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   5146409 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     31243 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     31243 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     15619 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   5146409 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   5146409 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   5146409 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   5146409 |  435 | `	pEntry->pHash = pHash;` |
|   5146409 |  436 | `	pEntry->pKey = pKey;` |
|   5146409 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   5146409 |  438 | `	pEntry->pUserData = pUserData;` |
|   5146409 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   5146409 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   5146409 |  442 | `	return rc;` |
|   2573207 |  443 | `}` |
|   5146288 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 | `{` |
|   5146293 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|         5 |  447 | `}` |
|         - |  448 | `/*` |
|         - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|         - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|         - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|         - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|         - |  453 | ` */` |
|       116 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  455 | `{` |
|       118 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|         2 |  457 | `}` |
|    170646 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 | `{` |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    170651 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 | `}` |
|         - |  468 |  |
