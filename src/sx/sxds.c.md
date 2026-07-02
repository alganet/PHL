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
|  20036156 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 | `{` |
|  20036161 |   16 | `	pSet->nSize = 0 ;` |
|  20036161 |   17 | `	pSet->nUsed = 0;` |
|  20036161 |   18 | `	pSet->nCursor = 0;` |
|  20036161 |   19 | `	pSet->eSize = ElemSize;` |
|  20036161 |   20 | `	pSet->pAllocator = pAllocator;` |
|  20036161 |   21 | `	pSet->pBase =  0;` |
|  20036161 |   22 | `	pSet->pUserData = 0;` |
|  20036161 |   23 | `	return SXRET_OK;` |
|         5 |   24 | `}` |
|  33139857 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 | `{` |
|         - |   27 | `	unsigned char *zbase;` |
|  33139862 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4701773 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4701773 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4537605 |   34 | `			pSet->nSize = 4;` |
|   2268800 |   35 | `		}` |
|   4701773 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4701773 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4701773 |   40 | `		pSet->pBase = pNew;` |
|   4701773 |   41 | `		pSet->nSize <<= 1;` |
|   2350884 |   42 | `	}` |
|  33139862 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 248369314 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  33139862 |   45 | `	pSet->nUsed++;` |
|  33139862 |   46 | `	return SXRET_OK;` |
|  16569976 |   47 | `}` |
|   1375794 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 | `{` |
|   1375799 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1375799 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1375799 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1375799 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1375799 |   60 | `	pSet->nSize = nItem;` |
|   1375799 |   61 | `	return SXRET_OK;` |
|    687902 |   62 | `}` |
|   1905237 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 | `{` |
|   1905242 |   65 | `	pSet->nUsed   = 0;` |
|   1905242 |   66 | `	pSet->nCursor = 0;` |
|   1905242 |   67 | `	return SXRET_OK;` |
|         5 |   68 | `}` |
|     58888 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 | `{` |
|     58893 |   71 | `	pSet->nCursor = 0;` |
|     58893 |   72 | `	return SXRET_OK;` |
|         5 |   73 | `}` |
|     63094 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 | `{` |
|         - |   76 | `	register unsigned char *zSrc;` |
|     63099 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     24437 |   79 | `		pSet->nCursor = 0;` |
|     24437 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     38667 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     38667 |   83 | `	if( ppEntry ){` |
|     38667 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     19331 |   85 | `	}` |
|     38667 |   86 | `	pSet->nCursor++;` |
|     38667 |   87 | `	return SXRET_OK;` |
|     31552 |   88 | `}` |
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
|    230536 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 | `{` |
|    230541 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       295 |  103 | `		pSet->nUsed = nNewSize;` |
|       145 |  104 | `	}` |
|    230541 |  105 | `	return SXRET_OK;` |
|         5 |  106 | `}` |
|  10279286 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 | `{` |
|  10279291 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10279291 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5152205 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2576100 |  112 | `	}` |
|  10279291 |  113 | `	pSet->pBase = 0;` |
|  10279291 |  114 | `	pSet->nUsed = 0;` |
|  10279291 |  115 | `	pSet->nCursor = 0;` |
|  10279291 |  116 | `	return rc;` |
|         5 |  117 | `}` |
|   5981488 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 | `{` |
|         - |  120 | `	const char *zBase;` |
|   5981493 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       133 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5981365 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5981365 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2990749 |  126 | `}` |
|   3636912 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 | `{` |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3636917 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2185995 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1450927 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1450927 |  135 | `	pSet->nUsed--;` |
|   1450927 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1450927 |  137 | `	return pData;` |
|   1818461 |  138 | `}` |
|  13729406 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	const char *zBase;` |
|  13729411 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13729411 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13729411 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6865079 |  148 | `}` |
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
|    643060 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 | `{` |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    643065 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    643065 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    643065 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    643065 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    643065 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    643065 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    643065 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    643065 |  180 | `	pHash->nEntry = 0;` |
|    643065 |  181 | `	pHash->apBucket = apNew;` |
|    643065 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    643065 |  183 | `	return SXRET_OK;` |
|    321535 |  184 | `}` |
|    137318 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 | `{` |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    137323 |  193 | `	pEntry = pHash->pList;` |
|     72625 |  194 | `	for(;;){` |
|    145255 |  195 | `		if( pHash->nEntry == 0 ){` |
|    137323 |  196 | `			break;` |
|         - |  197 | `		}` |
|      7937 |  198 | `		pNext = pEntry->pNext;` |
|      7937 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      7937 |  200 | `		pEntry = pNext;` |
|      7937 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    137323 |  203 | `	if( pHash->apBucket ){` |
|    137323 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     68659 |  205 | `	}` |
|    137323 |  206 | `	pHash->apBucket = 0;` |
|    137323 |  207 | `	pHash->nBucketSize = 0;` |
|    137323 |  208 | `	pHash->pAllocator = 0;` |
|    137323 |  209 | `	return SXRET_OK;` |
|         5 |  210 | `}` |
|  18308230 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 | `{` |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  18308235 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  18308235 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  16604286 |  218 | `	for(;;){` |
|  33160373 |  219 | `		if( pEntry == 0 ){` |
|   9732453 |  220 | `			break;` |
|         - |  221 | `		}` |
|  27715566 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   8575792 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   8575787 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  14852143 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   9732453 |  229 | `	return 0;` |
|   9154630 |  230 | `}` |
|  19225842 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 | `{` |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  19225847 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    917827 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  18308025 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  18308025 |  244 | `	if( pEntry == 0 ){` |
|   9732453 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   8575577 |  247 | `	return (SyHashEntry *)pEntry;` |
|   9613436 |  248 | `}` |
|    133926 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 | `{` |
|         - |  251 | `	sxi32 rc;` |
|    133931 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    103885 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     51945 |  254 | `	}else{` |
|     30051 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    133931 |  257 | `	if( pEntry->pNextCollide ){` |
|      5229 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2614 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    133931 |  261 | `	if( pHash->pLast == pEntry ){` |
|    127623 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     63809 |  263 | `	}` |
|    133931 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    133931 |  265 | `	pHash->nEntry--;` |
|    133931 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    133931 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    133931 |  272 | `	return rc;` |
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
|    133716 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 | `{` |
|    133721 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    133721 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    133721 |  300 | `	return rc;` |
|         5 |  301 | `}` |
|   1290334 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 | `{` |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1290339 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1290339 |  310 | `	return SXRET_OK;` |
|         5 |  311 | `}` |
|   8114722 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 | `{` |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   8114727 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1290077 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1290077 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   6824655 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   6824655 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   6824655 |  328 | `	return (SyHashEntry *)pEntry;` |
|   4057366 |  329 | `}` |
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
|      2021 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|      2011 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      2011 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|      2011 |  348 | `		pEntry = pEntry->pNext;` |
|      1006 |  349 | `	}` |
|        11 |  350 | `	return SXRET_OK;` |
|         6 |  351 | `}` |
|     31632 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  353 | `{` |
|     31637 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  355 | `	SyHashEntry_Pr *pEntry;` |
|         - |  356 | `	SyHashEntry_Pr **apNew;` |
|         - |  357 | `	sxu32 n,iBucket;` |
|         - |  358 |  |
|         - |  359 | `	/* Allocate a new larger table */` |
|     31637 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     31637 |  361 | `	if( apNew == 0 ){` |
|         - |  362 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  363 | `		return SXRET_OK;` |
|         - |  364 | `	}` |
|         - |  365 | `	/* Zero the new table */` |
|     31637 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  367 | `	/* Rehash all entries */` |
|   3987989 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3956357 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  370 | `		/* Install in the new bucket */` |
|   3956357 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3956357 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3956357 |  373 | `		if( apNew[iBucket] != 0 ){` |
|   1895596 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    947731 |  375 | `		}` |
|   3956357 |  376 | `		apNew[iBucket] = pEntry;` |
|         - |  377 | `		/* Point to the next entry */` |
|   3956357 |  378 | `		pEntry = pEntry->pNext;` |
|   1978181 |  379 | `	}` |
|         - |  380 | `	/* Release the old table and reflect the change */` |
|     31637 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     31637 |  382 | `	pHash->apBucket = apNew;` |
|     31637 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     31637 |  384 | `	return SXRET_OK;` |
|     15821 |  385 | `}` |
|   5290702 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 | `{` |
|   5290707 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   5290707 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   5290707 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2974667 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1487292 |  393 | `	}` |
|   5290707 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   5290707 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        51 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        51 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        51 |  401 | `		pHash->pLast = pEntry;` |
|        26 |  402 | `	}else{` |
|   5290657 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   5290707 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    332603 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    332603 |  408 | `		pHash->pLast = pEntry;` |
|    166299 |  409 | `	}` |
|   5290707 |  410 | `	pHash->nEntry++;` |
|   5290707 |  411 | `	return SXRET_OK;` |
|         5 |  412 | `}` |
|   5290702 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 | `{` |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   5290707 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     31637 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     31637 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     15816 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   5290707 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   5290707 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   5290707 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   5290707 |  435 | `	pEntry->pHash = pHash;` |
|   5290707 |  436 | `	pEntry->pKey = pKey;` |
|   5290707 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   5290707 |  438 | `	pEntry->pUserData = pUserData;` |
|   5290707 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   5290707 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   5290707 |  442 | `	return rc;` |
|   2645356 |  443 | `}` |
|   5290586 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 | `{` |
|   5290591 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|    171918 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 | `{` |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    171923 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 | `}` |
|         - |  468 |  |
