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
|  11995190 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  11995192 |   16 | `	pSet->nSize = 0 ;` |
|  11995192 |   17 | `	pSet->nUsed = 0;` |
|  11995192 |   18 | `	pSet->nCursor = 0;` |
|  11995192 |   19 | `	pSet->eSize = ElemSize;` |
|  11995192 |   20 | `	pSet->pAllocator = pAllocator;` |
|  11995192 |   21 | `	pSet->pBase =  0;` |
|  11995192 |   22 | `	pSet->pUserData = 0;` |
|  11995192 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  19538724 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  19538726 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3595032 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3595032 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3504698 |   34 | `			pSet->nSize = 4;` |
|   1752348 |   35 | `		}` |
|   3595032 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3595032 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3595032 |   40 | `		pSet->pBase = pNew;` |
|   3595032 |   41 | `		pSet->nSize <<= 1;` |
|   1797515 |   42 | `	}` |
|  19538726 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 145295178 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  19538726 |   45 | `	pSet->nUsed++;` |
|  19538726 |   46 | `	return SXRET_OK;` |
|   9769386 |   47 |  |
|    615758 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    615760 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    615760 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    615760 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    615760 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    615760 |   60 | `	pSet->nSize = nItem;` |
|    615760 |   61 | `	return SXRET_OK;` |
|    307881 |   62 |  |
|   1099586 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1099588 |   65 | `	pSet->nUsed   = 0;` |
|   1099588 |   66 | `	pSet->nCursor = 0;` |
|   1099588 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     40188 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     40190 |   71 | `	pSet->nCursor = 0;` |
|     40190 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     44080 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     44082 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     16350 |   79 | `		pSet->nCursor = 0;` |
|     16350 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     27734 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     27734 |   83 | `	if( ppEntry ){` |
|     27734 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     13866 |   85 | `	}` |
|     27734 |   86 | `	pSet->nCursor++;` |
|     27734 |   87 | `	return SXRET_OK;` |
|     22042 |   88 |  |
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
|     75338 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     75340 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     75340 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   7530002 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   7530004 |  109 | `	sxi32 rc = SXRET_OK;` |
|   7530004 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3897938 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1948968 |  112 | `	}` |
|   7530004 |  113 | `	pSet->pBase = 0;` |
|   7530004 |  114 | `	pSet->nUsed = 0;` |
|   7530004 |  115 | `	pSet->nCursor = 0;` |
|   7530004 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3859276 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3859278 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3859188 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3859188 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1929640 |  126 |  |
|   3150248 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3150250 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2135288 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1014964 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1014964 |  135 | `	pSet->nUsed--;` |
|   1014964 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1014964 |  137 | `	return pData;` |
|   1575126 |  138 |  |
|   9922766 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   9922768 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   9922768 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   9922768 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4961580 |  148 |  |
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
|    112206 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    112208 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    112208 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    112208 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    112208 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    112208 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    112208 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    112208 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    112208 |  180 | `	pHash->nEntry = 0;` |
|    112208 |  181 | `	pHash->apBucket = apNew;` |
|    112208 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    112208 |  183 | `	return SXRET_OK;` |
|     56105 |  184 |  |
|     12314 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     12316 |  193 | `	pEntry = pHash->pList;` |
|      7731 |  194 | `	for(;;){` |
|     15464 |  195 | `		if( pHash->nEntry == 0 ){` |
|     12316 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3150 |  198 | `		pNext = pEntry->pNext;` |
|      3150 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3150 |  200 | `		pEntry = pNext;` |
|      3150 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     12316 |  203 | `	if( pHash->apBucket ){` |
|     12316 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      6157 |  205 | `	}` |
|     12316 |  206 | `	pHash->apBucket = 0;` |
|     12316 |  207 | `	pHash->nBucketSize = 0;` |
|     12316 |  208 | `	pHash->pAllocator = 0;` |
|     12316 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  10327862 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  10327864 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  10327864 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   9021305 |  218 | `	for(;;){` |
|  17855011 |  219 | `		if( pEntry == 0 ){` |
|   5618864 |  220 | `			break;` |
|         - |  221 | `		}` |
|  14590519 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   4709004 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   4709002 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   7527149 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   5618864 |  229 | `	return 0;` |
|   5164197 |  230 |  |
|  10391382 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  10391384 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     63528 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  10327858 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  10327858 |  244 | `	if( pEntry == 0 ){` |
|   5618864 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   4708996 |  247 | `	return (SyHashEntry *)pEntry;` |
|   5195957 |  248 |  |
|     76488 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     76490 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     57770 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     28886 |  254 | `	}else{` |
|     18722 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     76490 |  257 | `	if( pEntry->pNextCollide ){` |
|      4133 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2066 |  259 | `	}` |
|     76490 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     76490 |  261 | `	pHash->nEntry--;` |
|     76490 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     76490 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     76490 |  268 | `	return rc;` |
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
|     76482 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     76484 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     76484 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     76484 |  296 | `	return rc;` |
|         2 |  297 |  |
|    160006 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    160008 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    160008 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   1145000 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   1145002 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    159574 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    159574 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    985430 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    985430 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    985430 |  324 | `	return (SyHashEntry *)pEntry;` |
|    572502 |  325 |  |
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
|      1619 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1609 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1609 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1609 |  344 | `		pEntry = pEntry->pNext;` |
|       805 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     15944 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     15946 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     15946 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     15946 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     15946 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2195338 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2179394 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2179394 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2179394 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2179394 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1046459 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    523225 |  371 | `		}` |
|   2179394 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2179394 |  374 | `		pEntry = pEntry->pNext;` |
|   1089698 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     15946 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     15946 |  378 | `	pHash->apBucket = apNew;` |
|     15946 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     15946 |  380 | `	return SXRET_OK;` |
|      7974 |  381 |  |
|   1997050 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1997052 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1997052 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1997052 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1331507 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    665740 |  389 | `	}` |
|   1997052 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1997052 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1997052 |  393 | `	if( pHash->nEntry == 0 ){` |
|     80264 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     40131 |  395 | `	}` |
|   1997052 |  396 | `	pHash->nEntry++;` |
|   1997052 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1997050 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1997052 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     15946 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     15946 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      7972 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1997052 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1997052 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1997052 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1997052 |  421 | `	pEntry->pHash = pHash;` |
|   1997052 |  422 | `	pEntry->pKey = pKey;` |
|   1997052 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1997052 |  424 | `	pEntry->pUserData = pUserData;` |
|   1997052 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1997052 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1997052 |  428 | `	return rc;` |
|    998527 |  429 |  |
|     96734 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     96736 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
