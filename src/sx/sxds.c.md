# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 290/304 lines (95.39%)

[Root index](../../index.md) | [Directory index](index.md)

|       Hits | Line | Source |
| ---------: | ---: | :--- |
|          - |    1 | `/**` |
|          - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|          - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|          - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|          - |    5 | ` */` |
|          - |    6 | `#include "sxtypes.h"` |
|          - |    7 | `#include "sxmacros.h"` |
|          - |    8 | `#include "sxset.h"` |
|          - |    9 | `#include "sxmem.h"` |
|          - |   10 | `#include "sxhashtable.h"` |
|          - |   11 | `#include "sxhash.h"` |
|          - |   12 | `#include "sxstr.h"` |
|          - |   13 |  |
|   83370940 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|   83370945 |   16 | `	pSet->nSize = 0 ;` |
|   83370945 |   17 | `	pSet->nUsed = 0;` |
|   83370945 |   18 | `	pSet->nCursor = 0;` |
|   83370945 |   19 | `	pSet->eSize = ElemSize;` |
|   83370945 |   20 | `	pSet->pAllocator = pAllocator;` |
|   83370945 |   21 | `	pSet->pBase =  0;` |
|   83370945 |   22 | `	pSet->pUserData = 0;` |
|   83370945 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  179450873 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  179450878 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   12032775 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   12032775 |   33 | `		if( pSet->nSize <= 0 ){` |
|   10609795 |   34 | `			pSet->nSize = 4;` |
|    5304895 |   35 | `		}` |
|   12032775 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   12032775 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   12032775 |   40 | `		pSet->pBase = pNew;` |
|   12032775 |   41 | `		pSet->nSize <<= 1;` |
|    6016385 |   42 | `	}` |
|  179450878 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 1317350254 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  179450878 |   45 | `	pSet->nUsed++;` |
|  179450878 |   46 | `	return SXRET_OK;` |
|   89725484 |   47 | `}` |
|    8836248 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|    8836253 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|    8836253 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|    8836253 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    8836253 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|    8836253 |   60 | `	pSet->nSize = nItem;` |
|    8836253 |   61 | `	return SXRET_OK;` |
|    4418129 |   62 | `}` |
|   13779293 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   13779298 |   65 | `	pSet->nUsed   = 0;` |
|   13779298 |   66 | `	pSet->nCursor = 0;` |
|   13779298 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      68848 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      68853 |   71 | `	pSet->nCursor = 0;` |
|      68853 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73030 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73035 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29623 |   79 | `		pSet->nCursor = 0;` |
|      29623 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43417 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43417 |   83 | `	if( ppEntry ){` |
|      43417 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21706 |   85 | `	}` |
|      43417 |   86 | `	pSet->nCursor++;` |
|      43417 |   87 | `	return SXRET_OK;` |
|      36520 |   88 | `}` |
|          - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|          8 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|          1 |   91 | `{` |
|          - |   92 | `	register unsigned char *zSrc;` |
|          9 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          3 |   94 | `		return 0;` |
|          - |   95 | `	}` |
|          7 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|          7 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|          5 |   98 | `}` |
|          - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    1406644 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    1406649 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1179 |  103 | `		pSet->nUsed = nNewSize;` |
|        587 |  104 | `	}` |
|    1406649 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   31033678 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   31033683 |  109 | `	sxi32 rc = SXRET_OK;` |
|   31033683 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   16598191 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|    8299093 |  112 | `	}` |
|   31033683 |  113 | `	pSet->pBase = 0;` |
|   31033683 |  114 | `	pSet->nUsed = 0;` |
|   31033683 |  115 | `	pSet->nCursor = 0;` |
|   31033683 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   31295180 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   31295185 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        133 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   31295057 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   31295057 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   15647595 |  126 | `}` |
|    6179830 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    6179835 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2195357 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    3984483 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    3984483 |  135 | `	pSet->nUsed--;` |
|    3984483 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    3984483 |  137 | `	return pData;` |
|    3089920 |  138 | `}` |
|   21312258 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   21312263 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         22 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   21312243 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   21312243 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   10656475 |  148 | `}` |
|          - |  149 | `/* Private hash entry */` |
|          - |  150 | `struct SyHashEntry_Pr` |
|          - |  151 | `{` |
|          - |  152 | `	const void *pKey; /* Hash key */` |
|          - |  153 | `	sxu32 nKeyLen;    /* Key length */` |
|          - |  154 | `	void *pUserData;  /* User private data */` |
|          - |  155 | `	/* Private fields */` |
|          - |  156 | `	sxu32 nHash;` |
|          - |  157 | `	SyHash *pHash;` |
|          - |  158 | `	SyHashEntry_Pr *pNext,*pPrev; /* Next and previous entry in the list */` |
|          - |  159 | `	SyHashEntry_Pr *pNextCollide,*pPrevCollide; /* Collision list */` |
|          - |  160 | `};` |
|          - |  161 | `#define INVALID_HASH(H) ((H)->apBucket == 0)` |
|    1161324 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1161329 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1161329 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1161329 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1161329 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1161329 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1161329 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1161329 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1161329 |  180 | `	pHash->nEntry = 0;` |
|    1161329 |  181 | `	pHash->apBucket = apNew;` |
|    1161329 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1161329 |  183 | `	return SXRET_OK;` |
|     580667 |  184 | `}` |
|     310684 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     310689 |  193 | `	pEntry = pHash->pList;` |
|     164140 |  194 | `	for(;;){` |
|     328285 |  195 | `		if( pHash->nEntry == 0 ){` |
|     310689 |  196 | `			break;` |
|          - |  197 | `		}` |
|      17601 |  198 | `		pNext = pEntry->pNext;` |
|      17601 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      17601 |  200 | `		pEntry = pNext;` |
|      17601 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     310689 |  203 | `	if( pHash->apBucket ){` |
|     310689 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     155342 |  205 | `	}` |
|     310689 |  206 | `	pHash->apBucket = 0;` |
|     310689 |  207 | `	pHash->nBucketSize = 0;` |
|     310689 |  208 | `	pHash->pAllocator = 0;` |
|     310689 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   40488735 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   40488740 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   40488740 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   38199241 |  218 | `	for(;;){` |
|   76034189 |  219 | `		if( pEntry == 0 ){` |
|   16138902 |  220 | `			break;` |
|          - |  221 | `		}` |
|   72069977 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   24349880 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   24349843 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   35545454 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   16138902 |  229 | `	return 0;` |
|   20244883 |  230 | `}` |
|   44125147 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   44125152 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    3636715 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   40488442 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   40488442 |  244 | `	if( pEntry == 0 ){` |
|   16138902 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   24349545 |  247 | `	return (SyHashEntry *)pEntry;` |
|   22063089 |  248 | `}` |
|     211098 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     211103 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     168721 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      84363 |  254 | `	}else{` |
|      42387 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     211103 |  257 | `	if( pEntry->pNextCollide ){` |
|       3980 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       1990 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     211103 |  261 | `	if( pHash->pLast == pEntry ){` |
|     204377 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     102186 |  263 | `	}` |
|     211103 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     211103 |  265 | `	pHash->nEntry--;` |
|     211103 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|        ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        ! 0 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     211103 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     211103 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        298 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        303 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        303 |  284 | `	if( pEntry == 0 ){` |
|        ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        303 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        303 |  288 | `	return rc;` |
|        154 |  289 | `}` |
|     210800 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     210805 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     210805 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     210805 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    1757404 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    1757409 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    1757409 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   13245382 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   13245387 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    1757145 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    1757145 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   11488247 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   11488247 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   11488247 |  328 | `	return (SyHashEntry *)pEntry;` |
|    6622696 |  329 | `}` |
|         10 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|          1 |  331 | `{` |
|          - |  332 | `	SyHashEntry_Pr *pEntry;` |
|          - |  333 | `	sxi32 rc;` |
|          - |  334 | `	sxu32 n;` |
|          - |  335 | `#if defined(UNTRUST)` |
|          - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|          - |  337 | `		return 0;` |
|          - |  338 | `	}` |
|          - |  339 | `#endif` |
|         11 |  340 | `	pEntry = pHash->pList;` |
|       3077 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3067 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3067 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3067 |  348 | `		pEntry = pEntry->pNext;` |
|       1534 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      77788 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      77793 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      77793 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      77793 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      77793 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|    9260769 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|    9182981 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|    9182981 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|    9182981 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|    9182981 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    4414568 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    2207285 |  375 | `		}` |
|    9182981 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|    9182981 |  378 | `		pEntry = pEntry->pNext;` |
|    4591493 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      77793 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      77793 |  382 | `	pHash->apBucket = apNew;` |
|      77793 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      77793 |  384 | `	return SXRET_OK;` |
|      38899 |  385 | `}` |
|   11466832 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   11466837 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   11466837 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   11466837 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    7223771 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    3611927 |  393 | `	}` |
|   11466837 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   11466837 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   11466785 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   11466837 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     604583 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     604583 |  408 | `		pHash->pLast = pEntry;` |
|     302289 |  409 | `	}` |
|   11466837 |  410 | `	pHash->nEntry++;` |
|   11466837 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   11466832 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   11466837 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      77793 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      77793 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      38894 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   11466837 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   11466837 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   11466837 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   11466837 |  435 | `	pEntry->pHash = pHash;` |
|   11466837 |  436 | `	pEntry->pKey = pKey;` |
|   11466837 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   11466837 |  438 | `	pEntry->pUserData = pUserData;` |
|   11466837 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   11466837 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   11466837 |  442 | `	return rc;` |
|    5733421 |  443 | `}` |
|   11466704 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   11466709 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        128 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          2 |  455 | `{` |
|        130 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          2 |  457 | `}` |
|     251818 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     251823 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
