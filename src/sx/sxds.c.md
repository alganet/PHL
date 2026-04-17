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
|  16303670 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  16303672 |   16 | `	pSet->nSize = 0 ;` |
|  16303672 |   17 | `	pSet->nUsed = 0;` |
|  16303672 |   18 | `	pSet->nCursor = 0;` |
|  16303672 |   19 | `	pSet->eSize = ElemSize;` |
|  16303672 |   20 | `	pSet->pAllocator = pAllocator;` |
|  16303672 |   21 | `	pSet->pBase =  0;` |
|  16303672 |   22 | `	pSet->pUserData = 0;` |
|  16303672 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  26774770 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  26774772 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4085506 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4085506 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3953954 |   34 | `			pSet->nSize = 4;` |
|   1976976 |   35 | `		}` |
|   4085506 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4085506 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4085506 |   40 | `		pSet->pBase = pNew;` |
|   4085506 |   41 | `		pSet->nSize <<= 1;` |
|   2042752 |   42 | `	}` |
|  26774772 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 199963638 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  26774772 |   45 | `	pSet->nUsed++;` |
|  26774772 |   46 | `	return SXRET_OK;` |
|  13387409 |   47 |  |
|   1055382 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|   1055384 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1055384 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1055384 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1055384 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1055384 |   60 | `	pSet->nSize = nItem;` |
|   1055384 |   61 | `	return SXRET_OK;` |
|    527693 |   62 |  |
|   1533746 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1533748 |   65 | `	pSet->nUsed   = 0;` |
|   1533748 |   66 | `	pSet->nCursor = 0;` |
|   1533748 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     50158 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     50160 |   71 | `	pSet->nCursor = 0;` |
|     50160 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     54240 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     54242 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     20650 |   79 | `		pSet->nCursor = 0;` |
|     20650 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     33594 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     33594 |   83 | `	if( ppEntry ){` |
|     33594 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     16796 |   85 | `	}` |
|     33594 |   86 | `	pSet->nCursor++;` |
|     33594 |   87 | `	return SXRET_OK;` |
|     27122 |   88 |  |
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
|    183500 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    183502 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       110 |  103 | `		pSet->nUsed = nNewSize;` |
|        54 |  104 | `	}` |
|    183502 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   9011164 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   9011166 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9011166 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4552616 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2276307 |  112 | `	}` |
|   9011166 |  113 | `	pSet->pBase = 0;` |
|   9011166 |  114 | `	pSet->nUsed = 0;` |
|   9011166 |  115 | `	pSet->nCursor = 0;` |
|   9011166 |  116 | `	return rc;` |
|         2 |  117 |  |
|   5082438 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5082440 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       108 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5082334 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5082334 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2541221 |  126 |  |
|   3355264 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3355266 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2147978 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1207290 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1207290 |  135 | `	pSet->nUsed--;` |
|   1207290 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1207290 |  137 | `	return pData;` |
|   1677634 |  138 |  |
|  11734549 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  11734551 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  11734551 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  11734551 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5867463 |  148 |  |
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
|    294278 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    294280 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    294280 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    294280 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    294280 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    294280 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    294280 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    294280 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    294280 |  180 | `	pHash->nEntry = 0;` |
|    294280 |  181 | `	pHash->apBucket = apNew;` |
|    294280 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    294280 |  183 | `	return SXRET_OK;` |
|    147141 |  184 |  |
|     85674 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     85676 |  193 | `	pEntry = pHash->pList;` |
|     45349 |  194 | `	for(;;){` |
|     90700 |  195 | `		if( pHash->nEntry == 0 ){` |
|     85676 |  196 | `			break;` |
|         - |  197 | `		}` |
|      5026 |  198 | `		pNext = pEntry->pNext;` |
|      5026 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      5026 |  200 | `		pEntry = pNext;` |
|      5026 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     85676 |  203 | `	if( pHash->apBucket ){` |
|     85676 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     42837 |  205 | `	}` |
|     85676 |  206 | `	pHash->apBucket = 0;` |
|     85676 |  207 | `	pHash->nBucketSize = 0;` |
|     85676 |  208 | `	pHash->pAllocator = 0;` |
|     85676 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  13187778 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  13187780 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  13187780 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  11692738 |  218 | `	for(;;){` |
|  23377432 |  219 | `		if( pEntry == 0 ){` |
|   7195794 |  220 | `			break;` |
|         - |  221 | `		}` |
|  19177503 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5991990 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5991988 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  10189654 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   7195794 |  229 | `	return 0;` |
|   6594155 |  230 |  |
|  13745568 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  13745570 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    557934 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  13187638 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  13187638 |  244 | `	if( pEntry == 0 ){` |
|   7195794 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5991846 |  247 | `	return (SyHashEntry *)pEntry;` |
|   6873050 |  248 |  |
|    103914 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    103916 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     79506 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     39754 |  254 | `	}else{` |
|     24412 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    103916 |  257 | `	if( pEntry->pNextCollide ){` |
|      4933 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2466 |  259 | `	}` |
|    103916 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    103916 |  261 | `	pHash->nEntry--;` |
|    103916 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    103916 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    103916 |  268 | `	return rc;` |
|         2 |  269 |  |
|       142 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       144 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       144 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       144 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       144 |  284 | `	return rc;` |
|        73 |  285 |  |
|    103772 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|    103774 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    103774 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    103774 |  296 | `	return rc;` |
|         2 |  297 |  |
|    367626 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    367628 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    367628 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2844800 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2844802 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    367192 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    367192 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2477612 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2477612 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2477612 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1422402 |  325 |  |
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
|      1791 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1781 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1781 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1781 |  344 | `		pEntry = pEntry->pNext;` |
|       891 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     24356 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     24358 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     24358 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     24358 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     24358 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3092038 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3067682 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3067682 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3067682 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3067682 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1468990 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    734535 |  371 | `		}` |
|   3067682 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3067682 |  374 | `		pEntry = pEntry->pNext;` |
|   1533842 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     24358 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     24358 |  378 | `	pHash->apBucket = apNew;` |
|     24358 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     24358 |  380 | `	return SXRET_OK;` |
|     12180 |  381 |  |
|   3211222 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   3211224 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   3211224 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   3211224 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2064596 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1032268 |  389 | `	}` |
|   3211224 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   3211224 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   3211224 |  393 | `	if( pHash->nEntry == 0 ){` |
|    146106 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     73052 |  395 | `	}` |
|   3211224 |  396 | `	pHash->nEntry++;` |
|   3211224 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   3211222 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   3211224 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     24358 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     24358 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     12178 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   3211224 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   3211224 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   3211224 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   3211224 |  421 | `	pEntry->pHash = pHash;` |
|   3211224 |  422 | `	pEntry->pKey = pKey;` |
|   3211224 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   3211224 |  424 | `	pEntry->pUserData = pUserData;` |
|   3211224 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   3211224 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   3211224 |  428 | `	return rc;` |
|   1605613 |  429 |  |
|    131128 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    131130 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
