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
|  11677578 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  11677580 |   16 | `	pSet->nSize = 0 ;` |
|  11677580 |   17 | `	pSet->nUsed = 0;` |
|  11677580 |   18 | `	pSet->nCursor = 0;` |
|  11677580 |   19 | `	pSet->eSize = ElemSize;` |
|  11677580 |   20 | `	pSet->pAllocator = pAllocator;` |
|  11677580 |   21 | `	pSet->pBase =  0;` |
|  11677580 |   22 | `	pSet->pUserData = 0;` |
|  11677580 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  18928730 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  18928732 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3552070 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3552070 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3466394 |   34 | `			pSet->nSize = 4;` |
|   1733196 |   35 | `		}` |
|   3552070 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3552070 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3552070 |   40 | `		pSet->pBase = pNew;` |
|   3552070 |   41 | `		pSet->nSize <<= 1;` |
|   1776034 |   42 | `	}` |
|  18928732 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 140855592 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  18928732 |   45 | `	pSet->nUsed++;` |
|  18928732 |   46 | `	return SXRET_OK;` |
|   9464389 |   47 |  |
|    583726 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    583728 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    583728 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    583728 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    583728 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    583728 |   60 | `	pSet->nSize = nItem;` |
|    583728 |   61 | `	return SXRET_OK;` |
|    291865 |   62 |  |
|   1063374 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1063376 |   65 | `	pSet->nUsed   = 0;` |
|   1063376 |   66 | `	pSet->nCursor = 0;` |
|   1063376 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     39626 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     39628 |   71 | `	pSet->nCursor = 0;` |
|     39628 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     43498 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     43500 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     16078 |   79 | `		pSet->nCursor = 0;` |
|     16078 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     27424 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     27424 |   83 | `	if( ppEntry ){` |
|     27424 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     13711 |   85 | `	}` |
|     27424 |   86 | `	pSet->nCursor++;` |
|     27424 |   87 | `	return SXRET_OK;` |
|     21751 |   88 |  |
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
|     71122 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     71124 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     71124 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   7421506 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   7421508 |  109 | `	sxi32 rc = SXRET_OK;` |
|   7421508 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3840312 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1920155 |  112 | `	}` |
|   7421508 |  113 | `	pSet->pBase = 0;` |
|   7421508 |  114 | `	pSet->nUsed = 0;` |
|   7421508 |  115 | `	pSet->nCursor = 0;` |
|   7421508 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3765684 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3765686 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3765596 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3765596 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1882844 |  126 |  |
|   3129378 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3129380 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2133052 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    996330 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    996330 |  135 | `	pSet->nUsed--;` |
|    996330 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    996330 |  137 | `	return pData;` |
|   1564691 |  138 |  |
|   9800121 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   9800123 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   9800123 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   9800123 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4900308 |  148 |  |
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
|    100102 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    100104 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    100104 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    100104 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    100104 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    100104 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    100104 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    100104 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    100104 |  180 | `	pHash->nEntry = 0;` |
|    100104 |  181 | `	pHash->apBucket = apNew;` |
|    100104 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    100104 |  183 | `	return SXRET_OK;` |
|     50053 |  184 |  |
|     12006 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     12008 |  193 | `	pEntry = pHash->pList;` |
|      7493 |  194 | `	for(;;){` |
|     14988 |  195 | `		if( pHash->nEntry == 0 ){` |
|     12008 |  196 | `			break;` |
|         - |  197 | `		}` |
|      2982 |  198 | `		pNext = pEntry->pNext;` |
|      2982 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      2982 |  200 | `		pEntry = pNext;` |
|      2982 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     12008 |  203 | `	if( pHash->apBucket ){` |
|     12008 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      6003 |  205 | `	}` |
|     12008 |  206 | `	pHash->apBucket = 0;` |
|     12008 |  207 | `	pHash->nBucketSize = 0;` |
|     12008 |  208 | `	pHash->pAllocator = 0;` |
|     12008 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   9964048 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   9964050 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   9964050 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   8552271 |  218 | `	for(;;){` |
|  17104314 |  219 | `		if( pEntry == 0 ){` |
|   5413758 |  220 | `			break;` |
|         - |  221 | `		}` |
|  13965574 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   4550296 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   4550294 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   7140266 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   5413758 |  229 | `	return 0;` |
|   4982290 |  230 |  |
|  10020246 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  10020248 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     56206 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   9964044 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   9964044 |  244 | `	if( pEntry == 0 ){` |
|   5413758 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   4550288 |  247 | `	return (SyHashEntry *)pEntry;` |
|   5010389 |  248 |  |
|     74858 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     74860 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     56438 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     28220 |  254 | `	}else{` |
|     18424 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     74860 |  257 | `	if( pEntry->pNextCollide ){` |
|      4133 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2066 |  259 | `	}` |
|     74860 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     74860 |  261 | `	pHash->nEntry--;` |
|     74860 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     74860 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     74860 |  268 | `	return rc;` |
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
|     74852 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     74854 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     74854 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     74854 |  296 | `	return rc;` |
|         2 |  297 |  |
|    140998 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    141000 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    141000 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    980478 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    980480 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    140566 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    140566 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    839916 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    839916 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    839916 |  324 | `	return (SyHashEntry *)pEntry;` |
|    490241 |  325 |  |
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
|      1617 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1607 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1607 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1607 |  344 | `		pEntry = pEntry->pNext;` |
|       804 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     14952 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     14954 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     14954 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     14954 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     14954 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2057450 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2042498 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2042498 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2042498 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2042498 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    980745 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    490374 |  371 | `		}` |
|   2042498 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2042498 |  374 | `		pEntry = pEntry->pNext;` |
|   1021250 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     14954 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     14954 |  378 | `	pHash->apBucket = apNew;` |
|     14954 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     14954 |  380 | `	return SXRET_OK;` |
|      7478 |  381 |  |
|   1841458 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1841460 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1841460 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1841460 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1246326 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    623170 |  389 | `	}` |
|   1841460 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1841460 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1841460 |  393 | `	if( pHash->nEntry == 0 ){` |
|     71944 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     35971 |  395 | `	}` |
|   1841460 |  396 | `	pHash->nEntry++;` |
|   1841460 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1841458 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1841460 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     14954 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     14954 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      7476 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1841460 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1841460 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1841460 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1841460 |  421 | `	pEntry->pHash = pHash;` |
|   1841460 |  422 | `	pEntry->pKey = pKey;` |
|   1841460 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1841460 |  424 | `	pEntry->pUserData = pUserData;` |
|   1841460 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1841460 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1841460 |  428 | `	return rc;` |
|    920731 |  429 |  |
|     93810 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     93812 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
