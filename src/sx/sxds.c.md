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
|  14109642 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  14109644 |   16 | `	pSet->nSize = 0 ;` |
|  14109644 |   17 | `	pSet->nUsed = 0;` |
|  14109644 |   18 | `	pSet->nCursor = 0;` |
|  14109644 |   19 | `	pSet->eSize = ElemSize;` |
|  14109644 |   20 | `	pSet->pAllocator = pAllocator;` |
|  14109644 |   21 | `	pSet->pBase =  0;` |
|  14109644 |   22 | `	pSet->pUserData = 0;` |
|  14109644 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  23313830 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  23313832 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3875454 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3875454 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3768836 |   34 | `			pSet->nSize = 4;` |
|   1884417 |   35 | `		}` |
|   3875454 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3875454 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3875454 |   40 | `		pSet->pBase = pNew;` |
|   3875454 |   41 | `		pSet->nSize <<= 1;` |
|   1937726 |   42 | `	}` |
|  23313832 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 173212628 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  23313832 |   45 | `	pSet->nUsed++;` |
|  23313832 |   46 | `	return SXRET_OK;` |
|  11656939 |   47 |  |
|    840422 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    840424 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    840424 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    840424 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    840424 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    840424 |   60 | `	pSet->nSize = nItem;` |
|    840424 |   61 | `	return SXRET_OK;` |
|    420213 |   62 |  |
|   1293498 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1293500 |   65 | `	pSet->nUsed   = 0;` |
|   1293500 |   66 | `	pSet->nCursor = 0;` |
|   1293500 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     44784 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     44786 |   71 | `	pSet->nCursor = 0;` |
|     44786 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     48866 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     48868 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     18354 |   79 | `		pSet->nCursor = 0;` |
|     18354 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     30516 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     30516 |   83 | `	if( ppEntry ){` |
|     30516 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     15257 |   85 | `	}` |
|     30516 |   86 | `	pSet->nCursor++;` |
|     30516 |   87 | `	return SXRET_OK;` |
|     24435 |   88 |  |
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
|    139628 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    139630 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|    139630 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8367742 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8367744 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8367744 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4269320 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2134659 |  112 | `	}` |
|   8367744 |  113 | `	pSet->pBase = 0;` |
|   8367744 |  114 | `	pSet->nUsed = 0;` |
|   8367744 |  115 | `	pSet->nCursor = 0;` |
|   8367744 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4527716 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4527718 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       106 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4527614 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4527614 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2263860 |  126 |  |
|   3276244 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3276246 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2142668 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1133580 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1133580 |  135 | `	pSet->nUsed--;` |
|   1133580 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1133580 |  137 | `	return pData;` |
|   1638124 |  138 |  |
|  10818267 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  10818269 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  10818269 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  10818269 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5409334 |  148 |  |
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
|    249784 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    249786 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    249786 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    249786 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    249786 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    249786 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    249786 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    249786 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    249786 |  180 | `	pHash->nEntry = 0;` |
|    249786 |  181 | `	pHash->apBucket = apNew;` |
|    249786 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    249786 |  183 | `	return SXRET_OK;` |
|    124894 |  184 |  |
|     75256 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     75258 |  193 | `	pEntry = pHash->pList;` |
|     39420 |  194 | `	for(;;){` |
|     78842 |  195 | `		if( pHash->nEntry == 0 ){` |
|     75258 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3586 |  198 | `		pNext = pEntry->pNext;` |
|      3586 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3586 |  200 | `		pEntry = pNext;` |
|      3586 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     75258 |  203 | `	if( pHash->apBucket ){` |
|     75258 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     37628 |  205 | `	}` |
|     75258 |  206 | `	pHash->apBucket = 0;` |
|     75258 |  207 | `	pHash->nBucketSize = 0;` |
|     75258 |  208 | `	pHash->pAllocator = 0;` |
|     75258 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  11660584 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  11660586 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  11660586 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  10517801 |  218 | `	for(;;){` |
|  21069550 |  219 | `		if( pEntry == 0 ){` |
|   6425988 |  220 | `			break;` |
|         - |  221 | `		}` |
|  17260733 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5234602 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5234600 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   9408966 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   6425988 |  229 | `	return 0;` |
|   5830558 |  230 |  |
|  12113006 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  12113008 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    452446 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  11660564 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  11660564 |  244 | `	if( pEntry == 0 ){` |
|   6425988 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5234578 |  247 | `	return (SyHashEntry *)pEntry;` |
|   6056769 |  248 |  |
|     86484 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     86486 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     65648 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     32825 |  254 | `	}else{` |
|     20840 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     86486 |  257 | `	if( pEntry->pNextCollide ){` |
|      4529 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2264 |  259 | `	}` |
|     86486 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     86486 |  261 | `	pHash->nEntry--;` |
|     86486 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     86486 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     86486 |  268 | `	return rc;` |
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
|     86462 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     86464 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     86464 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     86464 |  296 | `	return rc;` |
|         2 |  297 |  |
|    300876 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    300878 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    300878 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2332498 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2332500 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    300444 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    300444 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2032058 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2032058 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2032058 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1166251 |  325 |  |
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
|     21850 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     21852 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     21852 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     21852 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     21852 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2774652 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2752802 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2752802 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2752802 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2752802 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1315489 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    657718 |  371 | `		}` |
|   2752802 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2752802 |  374 | `		pEntry = pEntry->pNext;` |
|   1376402 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     21852 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     21852 |  378 | `	pHash->apBucket = apNew;` |
|     21852 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     21852 |  380 | `	return SXRET_OK;` |
|     10927 |  381 |  |
|   2819420 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2819422 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2819422 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2819422 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1826650 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    913346 |  389 | `	}` |
|   2819422 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2819422 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2819422 |  393 | `	if( pHash->nEntry == 0 ){` |
|    125466 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     62732 |  395 | `	}` |
|   2819422 |  396 | `	pHash->nEntry++;` |
|   2819422 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2819420 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2819422 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     21852 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     21852 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     10925 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2819422 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2819422 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2819422 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2819422 |  421 | `	pEntry->pHash = pHash;` |
|   2819422 |  422 | `	pEntry->pKey = pKey;` |
|   2819422 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2819422 |  424 | `	pEntry->pUserData = pUserData;` |
|   2819422 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2819422 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2819422 |  428 | `	return rc;` |
|   1409712 |  429 |  |
|    110994 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    110996 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
