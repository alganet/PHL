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
|  17297892 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  17297894 |   16 | `	pSet->nSize = 0 ;` |
|  17297894 |   17 | `	pSet->nUsed = 0;` |
|  17297894 |   18 | `	pSet->nCursor = 0;` |
|  17297894 |   19 | `	pSet->eSize = ElemSize;` |
|  17297894 |   20 | `	pSet->pAllocator = pAllocator;` |
|  17297894 |   21 | `	pSet->pBase =  0;` |
|  17297894 |   22 | `	pSet->pUserData = 0;` |
|  17297894 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  28444364 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  28444366 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4247880 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4247880 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4107852 |   34 | `			pSet->nSize = 4;` |
|   2053925 |   35 | `		}` |
|   4247880 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4247880 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4247880 |   40 | `		pSet->pBase = pNew;` |
|   4247880 |   41 | `		pSet->nSize <<= 1;` |
|   2123939 |   42 | `	}` |
|  28444366 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 212957108 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  28444366 |   45 | `	pSet->nUsed++;` |
|  28444366 |   46 | `	return SXRET_OK;` |
|  14222206 |   47 |  |
|   1151526 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|   1151528 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1151528 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1151528 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1151528 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1151528 |   60 | `	pSet->nSize = nItem;` |
|   1151528 |   61 | `	return SXRET_OK;` |
|    575765 |   62 |  |
|   1627172 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1627174 |   65 | `	pSet->nUsed   = 0;` |
|   1627174 |   66 | `	pSet->nCursor = 0;` |
|   1627174 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     53140 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     53142 |   71 | `	pSet->nCursor = 0;` |
|     53142 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     57250 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     57252 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     21906 |   79 | `		pSet->nCursor = 0;` |
|     21906 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     35348 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     35348 |   83 | `	if( ppEntry ){` |
|     35348 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     17673 |   85 | `	}` |
|     35348 |   86 | `	pSet->nCursor++;` |
|     35348 |   87 | `	return SXRET_OK;` |
|     28627 |   88 |  |
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
|    195336 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    195338 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       110 |  103 | `		pSet->nUsed = nNewSize;` |
|        54 |  104 | `	}` |
|    195338 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   9299258 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   9299260 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9299260 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4697638 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2348818 |  112 | `	}` |
|   9299260 |  113 | `	pSet->pBase = 0;` |
|   9299260 |  114 | `	pSet->nUsed = 0;` |
|   9299260 |  115 | `	pSet->nCursor = 0;` |
|   9299260 |  116 | `	return rc;` |
|         2 |  117 |  |
|   5309414 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5309416 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       108 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5309310 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5309310 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2654709 |  126 |  |
|   3424544 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3424546 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2151374 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1273174 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1273174 |  135 | `	pSet->nUsed--;` |
|   1273174 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1273174 |  137 | `	return pData;` |
|   1712274 |  138 |  |
|  12439236 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  12439238 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  12439238 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  12439238 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6219777 |  148 |  |
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
|    369910 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    369912 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    369912 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    369912 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    369912 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    369912 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    369912 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    369912 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    369912 |  180 | `	pHash->nEntry = 0;` |
|    369912 |  181 | `	pHash->apBucket = apNew;` |
|    369912 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    369912 |  183 | `	return SXRET_OK;` |
|    184957 |  184 |  |
|     91568 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     91570 |  193 | `	pEntry = pHash->pList;` |
|     48780 |  194 | `	for(;;){` |
|     97562 |  195 | `		if( pHash->nEntry == 0 ){` |
|     91570 |  196 | `			break;` |
|         - |  197 | `		}` |
|      5994 |  198 | `		pNext = pEntry->pNext;` |
|      5994 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      5994 |  200 | `		pEntry = pNext;` |
|      5994 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     91570 |  203 | `	if( pHash->apBucket ){` |
|     91570 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     45784 |  205 | `	}` |
|     91570 |  206 | `	pHash->apBucket = 0;` |
|     91570 |  207 | `	pHash->nBucketSize = 0;` |
|     91570 |  208 | `	pHash->pAllocator = 0;` |
|     91570 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  14111382 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  14111384 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  14111384 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  12668459 |  218 | `	for(;;){` |
|  25259120 |  219 | `		if( pEntry == 0 ){` |
|   7650564 |  220 | `			break;` |
|         - |  221 | `		}` |
|  20838838 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   6460824 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   6460822 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  11147738 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   7650564 |  229 | `	return 0;` |
|   7055957 |  230 |  |
|  14731382 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  14731384 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    620156 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  14111230 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  14111230 |  244 | `	if( pEntry == 0 ){` |
|   7650564 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   6460668 |  247 | `	return (SyHashEntry *)pEntry;` |
|   7365957 |  248 |  |
|    109736 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    109738 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     83962 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     41982 |  254 | `	}else{` |
|     25778 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    109738 |  257 | `	if( pEntry->pNextCollide ){` |
|      4831 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2415 |  259 | `	}` |
|    109738 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    109738 |  261 | `	pHash->nEntry--;` |
|    109738 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    109738 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    109738 |  268 | `	return rc;` |
|         2 |  269 |  |
|       154 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       156 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       156 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       156 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       156 |  284 | `	return rc;` |
|        79 |  285 |  |
|    109582 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|    109584 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    109584 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    109584 |  296 | `	return rc;` |
|         2 |  297 |  |
|    606332 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    606334 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    606334 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   3469930 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   3469932 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    605898 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    605898 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2864036 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2864036 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2864036 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1734967 |  325 |  |
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
|      1819 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1809 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1809 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1809 |  344 | `		pEntry = pEntry->pNext;` |
|       905 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     26030 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     26032 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     26032 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     26032 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     26032 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3307984 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3281954 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3281954 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3281954 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3281954 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1568989 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    784527 |  371 | `		}` |
|   3281954 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3281954 |  374 | `		pEntry = pEntry->pNext;` |
|   1640978 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     26032 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     26032 |  378 | `	pHash->apBucket = apNew;` |
|     26032 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     26032 |  380 | `	return SXRET_OK;` |
|     13017 |  381 |  |
|   3501930 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   3501932 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   3501932 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   3501932 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2236235 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1118205 |  389 | `	}` |
|   3501932 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   3501932 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   3501932 |  393 | `	if( pHash->nEntry == 0 ){` |
|    177288 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     88643 |  395 | `	}` |
|   3501932 |  396 | `	pHash->nEntry++;` |
|   3501932 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   3501930 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   3501932 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     26032 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     26032 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     13015 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   3501932 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   3501932 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   3501932 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   3501932 |  421 | `	pEntry->pHash = pHash;` |
|   3501932 |  422 | `	pEntry->pKey = pKey;` |
|   3501932 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   3501932 |  424 | `	pEntry->pUserData = pUserData;` |
|   3501932 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   3501932 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   3501932 |  428 | `	return rc;` |
|   1750967 |  429 |  |
|    138918 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    138920 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
