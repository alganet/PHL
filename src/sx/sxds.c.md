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
|  16532888 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  16532890 |   16 | `	pSet->nSize = 0 ;` |
|  16532890 |   17 | `	pSet->nUsed = 0;` |
|  16532890 |   18 | `	pSet->nCursor = 0;` |
|  16532890 |   19 | `	pSet->eSize = ElemSize;` |
|  16532890 |   20 | `	pSet->pAllocator = pAllocator;` |
|  16532890 |   21 | `	pSet->pBase =  0;` |
|  16532890 |   22 | `	pSet->pUserData = 0;` |
|  16532890 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  27100154 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  27100156 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4130812 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4130812 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3997344 |   34 | `			pSet->nSize = 4;` |
|   1998671 |   35 | `		}` |
|   4130812 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4130812 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4130812 |   40 | `		pSet->pBase = pNew;` |
|   4130812 |   41 | `		pSet->nSize <<= 1;` |
|   2065405 |   42 | `	}` |
|  27100156 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 202069086 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  27100156 |   45 | `	pSet->nUsed++;` |
|  27100156 |   46 | `	return SXRET_OK;` |
|  13550101 |   47 |  |
|   1069178 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|   1069180 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1069180 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1069180 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1069180 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1069180 |   60 | `	pSet->nSize = nItem;` |
|   1069180 |   61 | `	return SXRET_OK;` |
|    534591 |   62 |  |
|   1563740 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1563742 |   65 | `	pSet->nUsed   = 0;` |
|   1563742 |   66 | `	pSet->nCursor = 0;` |
|   1563742 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     51668 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     51670 |   71 | `	pSet->nCursor = 0;` |
|     51670 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     55750 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     55752 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     21258 |   79 | `		pSet->nCursor = 0;` |
|     21258 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     34496 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     34496 |   83 | `	if( ppEntry ){` |
|     34496 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     17247 |   85 | `	}` |
|     34496 |   86 | `	pSet->nCursor++;` |
|     34496 |   87 | `	return SXRET_OK;` |
|     27877 |   88 |  |
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
|    185750 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    185752 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       110 |  103 | `		pSet->nUsed = nNewSize;` |
|        54 |  104 | `	}` |
|    185752 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   9112058 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   9112060 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9112060 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4603866 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2301932 |  112 | `	}` |
|   9112060 |  113 | `	pSet->pBase = 0;` |
|   9112060 |  114 | `	pSet->nUsed = 0;` |
|   9112060 |  115 | `	pSet->nCursor = 0;` |
|   9112060 |  116 | `	return rc;` |
|         2 |  117 |  |
|   5123326 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5123328 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       108 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5123222 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5123222 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2561665 |  126 |  |
|   3390226 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3390228 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2148904 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1241326 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1241326 |  135 | `	pSet->nUsed--;` |
|   1241326 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1241326 |  137 | `	return pData;` |
|   1695115 |  138 |  |
|  11976078 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  11976080 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  11976080 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  11976080 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5988198 |  148 |  |
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
|    299110 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    299112 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    299112 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    299112 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    299112 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    299112 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    299112 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    299112 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    299112 |  180 | `	pHash->nEntry = 0;` |
|    299112 |  181 | `	pHash->apBucket = apNew;` |
|    299112 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    299112 |  183 | `	return SXRET_OK;` |
|    149557 |  184 |  |
|     87926 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     87928 |  193 | `	pEntry = pHash->pList;` |
|     46734 |  194 | `	for(;;){` |
|     93470 |  195 | `		if( pHash->nEntry == 0 ){` |
|     87928 |  196 | `			break;` |
|         - |  197 | `		}` |
|      5544 |  198 | `		pNext = pEntry->pNext;` |
|      5544 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      5544 |  200 | `		pEntry = pNext;` |
|      5544 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     87928 |  203 | `	if( pHash->apBucket ){` |
|     87928 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     43963 |  205 | `	}` |
|     87928 |  206 | `	pHash->apBucket = 0;` |
|     87928 |  207 | `	pHash->nBucketSize = 0;` |
|     87928 |  208 | `	pHash->pAllocator = 0;` |
|     87928 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  13459188 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  13459190 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  13459190 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  11952147 |  218 | `	for(;;){` |
|  24079297 |  219 | `		if( pEntry == 0 ){` |
|   7346580 |  220 | `			break;` |
|         - |  221 | `		}` |
|  19788894 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   6112614 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   6112612 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  10620109 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   7346580 |  229 | `	return 0;` |
|   6729860 |  230 |  |
|  14025154 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  14025156 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    566112 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  13459046 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  13459046 |  244 | `	if( pEntry == 0 ){` |
|   7346580 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   6112468 |  247 | `	return (SyHashEntry *)pEntry;` |
|   7012843 |  248 |  |
|    106368 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    106370 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     81302 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     40652 |  254 | `	}else{` |
|     25070 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    106370 |  257 | `	if( pEntry->pNextCollide ){` |
|      4721 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2360 |  259 | `	}` |
|    106370 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    106370 |  261 | `	pHash->nEntry--;` |
|    106370 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    106370 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    106370 |  268 | `	return rc;` |
|         2 |  269 |  |
|       144 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       146 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       146 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       146 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       146 |  284 | `	return rc;` |
|        74 |  285 |  |
|    106224 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|    106226 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    106226 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    106226 |  296 | `	return rc;` |
|         2 |  297 |  |
|    373768 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    373770 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    373770 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2924004 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2924006 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    373334 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    373334 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2550674 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2550674 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2550674 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1462004 |  325 |  |
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
|      1801 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1791 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1791 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1791 |  344 | `		pEntry = pEntry->pNext;` |
|       896 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     24680 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     24682 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     24682 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     24682 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     24682 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3133834 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3109154 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3109154 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3109154 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3109154 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1486336 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    743240 |  371 | `		}` |
|   3109154 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3109154 |  374 | `		pEntry = pEntry->pNext;` |
|   1554578 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     24682 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     24682 |  378 | `	pHash->apBucket = apNew;` |
|     24682 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     24682 |  380 | `	return SXRET_OK;` |
|     12342 |  381 |  |
|   3268058 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   3268060 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   3268060 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   3268060 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2105512 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1052713 |  389 | `	}` |
|   3268060 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   3268060 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   3268060 |  393 | `	if( pHash->nEntry == 0 ){` |
|    148258 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     74128 |  395 | `	}` |
|   3268060 |  396 | `	pHash->nEntry++;` |
|   3268060 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   3268058 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   3268060 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     24682 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     24682 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     12340 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   3268060 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   3268060 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   3268060 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   3268060 |  421 | `	pEntry->pHash = pHash;` |
|   3268060 |  422 | `	pEntry->pKey = pKey;` |
|   3268060 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   3268060 |  424 | `	pEntry->pUserData = pUserData;` |
|   3268060 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   3268060 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   3268060 |  428 | `	return rc;` |
|   1634031 |  429 |  |
|    133944 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    133946 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
