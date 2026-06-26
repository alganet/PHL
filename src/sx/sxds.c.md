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
|  19163862 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 |  |
|  19163867 |   16 | `	pSet->nSize = 0 ;` |
|  19163867 |   17 | `	pSet->nUsed = 0;` |
|  19163867 |   18 | `	pSet->nCursor = 0;` |
|  19163867 |   19 | `	pSet->eSize = ElemSize;` |
|  19163867 |   20 | `	pSet->pAllocator = pAllocator;` |
|  19163867 |   21 | `	pSet->pBase =  0;` |
|  19163867 |   22 | `	pSet->pUserData = 0;` |
|  19163867 |   23 | `	return SXRET_OK;` |
|         5 |   24 |  |
|  31515395 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  31515400 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4556071 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4556071 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4398877 |   34 | `			pSet->nSize = 4;` |
|   2199436 |   35 | `		}` |
|   4556071 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4556071 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4556071 |   40 | `		pSet->pBase = pNew;` |
|   4556071 |   41 | `		pSet->nSize <<= 1;` |
|   2278033 |   42 | `	}` |
|  31515400 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 235644868 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  31515400 |   45 | `	pSet->nUsed++;` |
|  31515400 |   46 | `	return SXRET_OK;` |
|  15757746 |   47 |  |
|   1298882 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 |  |
|   1298887 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1298887 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1298887 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1298887 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1298887 |   60 | `	pSet->nSize = nItem;` |
|   1298887 |   61 | `	return SXRET_OK;` |
|    649446 |   62 |  |
|   1795573 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 |  |
|   1795578 |   65 | `	pSet->nUsed   = 0;` |
|   1795578 |   66 | `	pSet->nCursor = 0;` |
|   1795578 |   67 | `	return SXRET_OK;` |
|         5 |   68 |  |
|     57360 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 |  |
|     57365 |   71 | `	pSet->nCursor = 0;` |
|     57365 |   72 | `	return SXRET_OK;` |
|         5 |   73 |  |
|     61566 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     61571 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     23749 |   79 | `		pSet->nCursor = 0;` |
|     23749 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     37827 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     37827 |   83 | `	if( ppEntry ){` |
|     37827 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     18911 |   85 | `	}` |
|     37827 |   86 | `	pSet->nCursor++;` |
|     37827 |   87 | `	return SXRET_OK;` |
|     30788 |   88 |  |
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
|    220340 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 |  |
|    220345 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       136 |  103 | `		pSet->nUsed = nNewSize;` |
|        66 |  104 | `	}` |
|    220345 |  105 | `	return SXRET_OK;` |
|         5 |  106 |  |
|   9973934 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 |  |
|   9973939 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9973939 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4988851 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2494423 |  112 | `	}` |
|   9973939 |  113 | `	pSet->pBase = 0;` |
|   9973939 |  114 | `	pSet->nUsed = 0;` |
|   9973939 |  115 | `	pSet->nCursor = 0;` |
|   9973939 |  116 | `	return rc;` |
|         5 |  117 |  |
|   5741612 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5741617 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       131 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5741491 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5741491 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2870811 |  126 |  |
|   3581832 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3581837 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2180749 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1401093 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1401093 |  135 | `	pSet->nUsed--;` |
|   1401093 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1401093 |  137 | `	return pData;` |
|   1790921 |  138 |  |
|  13314798 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  13314803 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13314803 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13314803 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6657738 |  148 |  |
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
|    564308 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    564313 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    564313 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    564313 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    564313 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    564313 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    564313 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    564313 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    564313 |  180 | `	pHash->nEntry = 0;` |
|    564313 |  181 | `	pHash->apBucket = apNew;` |
|    564313 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    564313 |  183 | `	return SXRET_OK;` |
|    282159 |  184 |  |
|    102390 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    102395 |  193 | `	pEntry = pHash->pList;` |
|     54792 |  194 | `	for(;;){` |
|    109589 |  195 | `		if( pHash->nEntry == 0 ){` |
|    102395 |  196 | `			break;` |
|         - |  197 | `		}` |
|      7199 |  198 | `		pNext = pEntry->pNext;` |
|      7199 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      7199 |  200 | `		pEntry = pNext;` |
|      7199 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    102395 |  203 | `	if( pHash->apBucket ){` |
|    102395 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     51195 |  205 | `	}` |
|    102395 |  206 | `	pHash->apBucket = 0;` |
|    102395 |  207 | `	pHash->nBucketSize = 0;` |
|    102395 |  208 | `	pHash->pAllocator = 0;` |
|    102395 |  209 | `	return SXRET_OK;` |
|         5 |  210 |  |
|  17404608 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  17404613 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  17404613 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  15601186 |  218 | `	for(;;){` |
|  31144484 |  219 | `		if( pEntry == 0 ){` |
|   9236577 |  220 | `			break;` |
|         - |  221 | `		}` |
|  25991671 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   8168040 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   8168041 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  13739876 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   9236577 |  229 | `	return 0;` |
|   8702831 |  230 |  |
|  18237812 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  18237817 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    833411 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  17404411 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  17404411 |  244 | `	if( pEntry == 0 ){` |
|   9236577 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   8167839 |  247 | `	return (SyHashEntry *)pEntry;` |
|   9119433 |  248 |  |
|    122756 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    122761 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     94587 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     47296 |  254 | `	}else{` |
|     28179 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    122761 |  257 | `	if( pEntry->pNextCollide ){` |
|      5039 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2519 |  259 | `	}` |
|    122761 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    122761 |  261 | `	pHash->nEntry--;` |
|    122761 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    122761 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    122761 |  268 | `	return rc;` |
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
|    122554 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  287 |  |
|    122559 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    122559 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    122559 |  296 | `	return rc;` |
|         5 |  297 |  |
|   1145788 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|   1145793 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   1145793 |  306 | `	return SXRET_OK;` |
|         5 |  307 |  |
|   7242152 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   7242157 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1145341 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   1145341 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   6096821 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   6096821 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   6096821 |  324 | `	return (SyHashEntry *)pEntry;` |
|   3621081 |  325 |  |
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
|     29382 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  349 |  |
|     29387 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     29387 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     29387 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     29387 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3734315 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3704933 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3704933 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3704933 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3704933 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1777309 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    888635 |  371 | `		}` |
|   3704933 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3704933 |  374 | `		pEntry = pEntry->pNext;` |
|   1852469 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     29387 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     29387 |  378 | `	pHash->apBucket = apNew;` |
|     29387 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     29387 |  380 | `	return SXRET_OK;` |
|     14696 |  381 |  |
|   4928010 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         5 |  383 |  |
|   4928015 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   4928015 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4928015 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2785638 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1392839 |  389 | `	}` |
|   4928015 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   4928015 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   4928015 |  393 | `	if( pHash->nEntry == 0 ){` |
|    308997 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    154496 |  395 | `	}` |
|   4928015 |  396 | `	pHash->nEntry++;` |
|   4928015 |  397 | `	return SXRET_OK;` |
|         5 |  398 |  |
|   4928010 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   4928015 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     29387 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     29387 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     14691 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   4928015 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4928015 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   4928015 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4928015 |  421 | `	pEntry->pHash = pHash;` |
|   4928015 |  422 | `	pEntry->pKey = pKey;` |
|   4928015 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   4928015 |  424 | `	pEntry->pUserData = pUserData;` |
|   4928015 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4928015 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   4928015 |  428 | `	return rc;` |
|   2464010 |  429 |  |
|    158974 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    158979 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  439 |  |
|         - |  440 |  |
