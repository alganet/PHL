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
|  19179352 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 |  |
|  19179357 |   16 | `	pSet->nSize = 0 ;` |
|  19179357 |   17 | `	pSet->nUsed = 0;` |
|  19179357 |   18 | `	pSet->nCursor = 0;` |
|  19179357 |   19 | `	pSet->eSize = ElemSize;` |
|  19179357 |   20 | `	pSet->pAllocator = pAllocator;` |
|  19179357 |   21 | `	pSet->pBase =  0;` |
|  19179357 |   22 | `	pSet->pUserData = 0;` |
|  19179357 |   23 | `	return SXRET_OK;` |
|         5 |   24 |  |
|  31542255 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  31542260 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4558223 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4558223 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4400851 |   34 | `			pSet->nSize = 4;` |
|   2200423 |   35 | `		}` |
|   4558223 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4558223 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4558223 |   40 | `		pSet->pBase = pNew;` |
|   4558223 |   41 | `		pSet->nSize <<= 1;` |
|   2279109 |   42 | `	}` |
|  31542260 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 235843456 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  31542260 |   45 | `	pSet->nUsed++;` |
|  31542260 |   46 | `	return SXRET_OK;` |
|  15771175 |   47 |  |
|   1300340 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 |  |
|   1300345 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1300345 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1300345 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1300345 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1300345 |   60 | `	pSet->nSize = nItem;` |
|   1300345 |   61 | `	return SXRET_OK;` |
|    650175 |   62 |  |
|   1797093 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 |  |
|   1797098 |   65 | `	pSet->nUsed   = 0;` |
|   1797098 |   66 | `	pSet->nCursor = 0;` |
|   1797098 |   67 | `	return SXRET_OK;` |
|         5 |   68 |  |
|     57376 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 |  |
|     57381 |   71 | `	pSet->nCursor = 0;` |
|     57381 |   72 | `	return SXRET_OK;` |
|         5 |   73 |  |
|     61582 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     61587 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     23757 |   79 | `		pSet->nCursor = 0;` |
|     23757 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     37835 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     37835 |   83 | `	if( ppEntry ){` |
|     37835 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     18915 |   85 | `	}` |
|     37835 |   86 | `	pSet->nCursor++;` |
|     37835 |   87 | `	return SXRET_OK;` |
|     30796 |   88 |  |
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
|    220602 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 |  |
|    220607 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       137 |  103 | `		pSet->nUsed = nNewSize;` |
|        66 |  104 | `	}` |
|    220607 |  105 | `	return SXRET_OK;` |
|         5 |  106 |  |
|   9979656 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 |  |
|   9979661 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9979661 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4991423 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2495709 |  112 | `	}` |
|   9979661 |  113 | `	pSet->pBase = 0;` |
|   9979661 |  114 | `	pSet->nUsed = 0;` |
|   9979661 |  115 | `	pSet->nCursor = 0;` |
|   9979661 |  116 | `	return rc;` |
|         5 |  117 |  |
|   5745850 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5745855 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       131 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5745729 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5745729 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2872930 |  126 |  |
|   3582908 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3582913 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2180893 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1402025 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1402025 |  135 | `	pSet->nUsed--;` |
|   1402025 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1402025 |  137 | `	return pData;` |
|   1791459 |  138 |  |
|  13320934 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  13320939 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13320939 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13320939 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6660796 |  148 |  |
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
|    564966 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    564971 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    564971 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    564971 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    564971 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    564971 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    564971 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    564971 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    564971 |  180 | `	pHash->nEntry = 0;` |
|    564971 |  181 | `	pHash->apBucket = apNew;` |
|    564971 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    564971 |  183 | `	return SXRET_OK;` |
|    282488 |  184 |  |
|    102528 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    102533 |  193 | `	pEntry = pHash->pList;` |
|     54915 |  194 | `	for(;;){` |
|    109835 |  195 | `		if( pHash->nEntry == 0 ){` |
|    102533 |  196 | `			break;` |
|         - |  197 | `		}` |
|      7307 |  198 | `		pNext = pEntry->pNext;` |
|      7307 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      7307 |  200 | `		pEntry = pNext;` |
|      7307 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    102533 |  203 | `	if( pHash->apBucket ){` |
|    102533 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     51264 |  205 | `	}` |
|    102533 |  206 | `	pHash->apBucket = 0;` |
|    102533 |  207 | `	pHash->nBucketSize = 0;` |
|    102533 |  208 | `	pHash->pAllocator = 0;` |
|    102533 |  209 | `	return SXRET_OK;` |
|         5 |  210 |  |
|  17418070 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  17418075 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  17418075 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  15594830 |  218 | `	for(;;){` |
|  31186426 |  219 | `		if( pEntry == 0 ){` |
|   9243919 |  220 | `			break;` |
|         - |  221 | `		}` |
|  26029337 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   8174160 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   8174161 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  13768356 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   9243919 |  229 | `	return 0;` |
|   8709550 |  230 |  |
|  18252358 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  18252363 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    834495 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  17417873 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  17417873 |  244 | `	if( pEntry == 0 ){` |
|   9243919 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   8173959 |  247 | `	return (SyHashEntry *)pEntry;` |
|   9126694 |  248 |  |
|    122884 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    122889 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     94707 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     47356 |  254 | `	}else{` |
|     28187 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    122889 |  257 | `	if( pEntry->pNextCollide ){` |
|      5039 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2519 |  259 | `	}` |
|    122889 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    122889 |  261 | `	pHash->nEntry--;` |
|    122889 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    122889 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    122889 |  268 | `	return rc;` |
|         5 |  269 |  |
|       202 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       207 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       207 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       207 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       207 |  284 | `	return rc;` |
|       106 |  285 |  |
|    122682 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  287 |  |
|    122687 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    122687 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    122687 |  296 | `	return rc;` |
|         5 |  297 |  |
|   1146976 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|   1146981 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   1146981 |  306 | `	return SXRET_OK;` |
|         5 |  307 |  |
|   7249488 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   7249493 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1146529 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   1146529 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   6102969 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   6102969 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   6102969 |  324 | `	return (SyHashEntry *)pEntry;` |
|   3624749 |  325 |  |
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
|      1995 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1985 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1985 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1985 |  344 | `		pEntry = pEntry->pNext;` |
|       993 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     29420 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  349 |  |
|     29425 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     29425 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     29425 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     29425 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3739153 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3709733 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3709733 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3709733 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3709733 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1779805 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    889907 |  371 | `		}` |
|   3709733 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3709733 |  374 | `		pEntry = pEntry->pNext;` |
|   1854869 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     29425 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     29425 |  378 | `	pHash->apBucket = apNew;` |
|     29425 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     29425 |  380 | `	return SXRET_OK;` |
|     14715 |  381 |  |
|   4934010 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         5 |  383 |  |
|   4934015 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   4934015 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4934015 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2789118 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1394547 |  389 | `	}` |
|   4934015 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   4934015 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   4934015 |  393 | `	if( pHash->nEntry == 0 ){` |
|    309379 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    154687 |  395 | `	}` |
|   4934015 |  396 | `	pHash->nEntry++;` |
|   4934015 |  397 | `	return SXRET_OK;` |
|         5 |  398 |  |
|   4934010 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   4934015 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     29425 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     29425 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     14710 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   4934015 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4934015 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   4934015 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4934015 |  421 | `	pEntry->pHash = pHash;` |
|   4934015 |  422 | `	pEntry->pKey = pKey;` |
|   4934015 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   4934015 |  424 | `	pEntry->pUserData = pUserData;` |
|   4934015 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4934015 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   4934015 |  428 | `	return rc;` |
|   2467010 |  429 |  |
|    159146 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    159151 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  439 |  |
|         - |  440 |  |
