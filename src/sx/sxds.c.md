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
|  10017102 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  10017104 |   16 | `	pSet->nSize = 0 ;` |
|  10017104 |   17 | `	pSet->nUsed = 0;` |
|  10017104 |   18 | `	pSet->nCursor = 0;` |
|  10017104 |   19 | `	pSet->eSize = ElemSize;` |
|  10017104 |   20 | `	pSet->pAllocator = pAllocator;` |
|  10017104 |   21 | `	pSet->pBase =  0;` |
|  10017104 |   22 | `	pSet->pUserData = 0;` |
|  10017104 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  15808944 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  15808946 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3256768 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3256768 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3195914 |   34 | `			pSet->nSize = 4;` |
|   1597956 |   35 | `		}` |
|   3256768 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3256768 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3256768 |   40 | `		pSet->pBase = pNew;` |
|   3256768 |   41 | `		pSet->nSize <<= 1;` |
|   1628383 |   42 | `	}` |
|  15808946 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 119314290 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  15808946 |   45 | `	pSet->nUsed++;` |
|  15808946 |   46 | `	return SXRET_OK;` |
|   7904496 |   47 |  |
|    435518 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    435520 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    435520 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    435520 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    435520 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    435520 |   60 | `	pSet->nSize = nItem;` |
|    435520 |   61 | `	return SXRET_OK;` |
|    217761 |   62 |  |
|    857656 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|    857658 |   65 | `	pSet->nUsed   = 0;` |
|    857658 |   66 | `	pSet->nCursor = 0;` |
|    857658 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     34822 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     34824 |   71 | `	pSet->nCursor = 0;` |
|     34824 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     38152 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     38154 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     13966 |   79 | `		pSet->nCursor = 0;` |
|     13966 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     24190 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     24190 |   83 | `	if( ppEntry ){` |
|     24190 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     12094 |   85 | `	}` |
|     24190 |   86 | `	pSet->nCursor++;` |
|     24190 |   87 | `	return SXRET_OK;` |
|     19078 |   88 |  |
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
|     55212 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     55214 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     55214 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   6754256 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   6754258 |  109 | `	sxi32 rc = SXRET_OK;` |
|   6754258 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3468576 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1734287 |  112 | `	}` |
|   6754258 |  113 | `	pSet->pBase = 0;` |
|   6754258 |  114 | `	pSet->nUsed = 0;` |
|   6754258 |  115 | `	pSet->nCursor = 0;` |
|   6754258 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3298224 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3298226 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3298136 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3298136 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1649114 |  126 |  |
|   2942348 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   2942350 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2122216 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    820136 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    820136 |  135 | `	pSet->nUsed--;` |
|    820136 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    820136 |  137 | `	return pData;` |
|   1471176 |  138 |  |
|   8494010 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   8494012 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   8494012 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   8494012 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4247226 |  148 |  |
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
|     78426 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     78428 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     78428 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     78428 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     78428 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     78428 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     78428 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     78428 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     78428 |  180 | `	pHash->nEntry = 0;` |
|     78428 |  181 | `	pHash->apBucket = apNew;` |
|     78428 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     78428 |  183 | `	return SXRET_OK;` |
|     39215 |  184 |  |
|      9932 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|      9934 |  193 | `	pEntry = pHash->pList;` |
|      5800 |  194 | `	for(;;){` |
|     11602 |  195 | `		if( pHash->nEntry == 0 ){` |
|      9934 |  196 | `			break;` |
|         - |  197 | `		}` |
|      1670 |  198 | `		pNext = pEntry->pNext;` |
|      1670 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      1670 |  200 | `		pEntry = pNext;` |
|      1670 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|      9934 |  203 | `	if( pHash->apBucket ){` |
|      9934 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      4966 |  205 | `	}` |
|      9934 |  206 | `	pHash->apBucket = 0;` |
|      9934 |  207 | `	pHash->nBucketSize = 0;` |
|      9934 |  208 | `	pHash->pAllocator = 0;` |
|      9934 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   7936598 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   7936600 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   7936600 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   6911272 |  218 | `	for(;;){` |
|  13823044 |  219 | `		if( pEntry == 0 ){` |
|   4299748 |  220 | `			break;` |
|         - |  221 | `		}` |
|  11341594 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   3636856 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   3636854 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   5886446 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   4299748 |  229 | `	return 0;` |
|   3968565 |  230 |  |
|   7980896 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   7980898 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     44306 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   7936594 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   7936594 |  244 | `	if( pEntry == 0 ){` |
|   4299748 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   3636848 |  247 | `	return (SyHashEntry *)pEntry;` |
|   3990714 |  248 |  |
|     63606 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     63608 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     47634 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     23818 |  254 | `	}else{` |
|     15976 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     63608 |  257 | `	if( pEntry->pNextCollide ){` |
|      3733 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      1866 |  259 | `	}` |
|     63608 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     63608 |  261 | `	pHash->nEntry--;` |
|     63608 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     63608 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     63608 |  268 | `	return rc;` |
|         2 |  269 |  |
|         6 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         1 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|         7 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|         7 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|         7 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|         7 |  284 | `	return rc;` |
|         4 |  285 |  |
|     63600 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     63602 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     63602 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     63602 |  296 | `	return rc;` |
|         2 |  297 |  |
|    114452 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    114454 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    114454 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    797600 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    797602 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    114020 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    114020 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    683584 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    683584 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    683584 |  324 | `	return (SyHashEntry *)pEntry;` |
|    398802 |  325 |  |
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
|      1579 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1569 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1569 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1569 |  344 | `		pEntry = pEntry->pNext;` |
|       785 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     11084 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     11086 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     11086 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     11086 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     11086 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1517998 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1506914 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1506914 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1506914 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1506914 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    723649 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    361823 |  371 | `		}` |
|   1506914 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1506914 |  374 | `		pEntry = pEntry->pNext;` |
|    753458 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     11086 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     11086 |  378 | `	pHash->apBucket = apNew;` |
|     11086 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     11086 |  380 | `	return SXRET_OK;` |
|      5544 |  381 |  |
|   1368394 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1368396 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1368396 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1368396 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    912050 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    456025 |  389 | `	}` |
|   1368396 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1368396 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1368396 |  393 | `	if( pHash->nEntry == 0 ){` |
|     56224 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     28111 |  395 | `	}` |
|   1368396 |  396 | `	pHash->nEntry++;` |
|   1368396 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1368394 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1368396 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     11086 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     11086 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      5542 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1368396 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1368396 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1368396 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1368396 |  421 | `	pEntry->pHash = pHash;` |
|   1368396 |  422 | `	pEntry->pKey = pKey;` |
|   1368396 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1368396 |  424 | `	pEntry->pUserData = pUserData;` |
|   1368396 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1368396 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1368396 |  428 | `	return rc;` |
|    684199 |  429 |  |
|     77574 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     77576 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
