# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 293/304 lines (96.38%)

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
|  171943280 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  171943285 |   16 | `	pSet->nSize = 0 ;` |
|  171943285 |   17 | `	pSet->nUsed = 0;` |
|  171943285 |   18 | `	pSet->nCursor = 0;` |
|  171943285 |   19 | `	pSet->eSize = ElemSize;` |
|  171943285 |   20 | `	pSet->pAllocator = pAllocator;` |
|  171943285 |   21 | `	pSet->pBase =  0;` |
|  171943285 |   22 | `	pSet->pUserData = 0;` |
|  171943285 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  389433596 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  389433601 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   22451543 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   22451543 |   33 | `		if( pSet->nSize <= 0 ){` |
|   19123121 |   34 | `			pSet->nSize = 4;` |
|    9562233 |   35 | `		}` |
|   22451543 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   22451543 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   22451543 |   40 | `		pSet->pBase = pNew;` |
|   22451543 |   41 | `		pSet->nSize <<= 1;` |
|   11226444 |   42 | `	}` |
|  389433601 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3072108053 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  389433601 |   45 | `	pSet->nUsed++;` |
|  389433601 |   46 | `	return SXRET_OK;` |
|  194719265 |   47 | `}` |
|   19375098 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   19375103 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   19375103 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   19375103 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   19375103 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   19375103 |   60 | `	pSet->nSize = nItem;` |
|   19375103 |   61 | `	return SXRET_OK;` |
|    9687554 |   62 | `}` |
|   28845190 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   28845195 |   65 | `	pSet->nUsed   = 0;` |
|   28845195 |   66 | `	pSet->nCursor = 0;` |
|   28845195 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      70932 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      70937 |   71 | `	pSet->nCursor = 0;` |
|      70937 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      75120 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      75125 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30703 |   79 | `		pSet->nCursor = 0;` |
|      30703 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      44427 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      44427 |   83 | `	if( ppEntry ){` |
|      44427 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      22211 |   85 | `	}` |
|      44427 |   86 | `	pSet->nCursor++;` |
|      44427 |   87 | `	return SXRET_OK;` |
|      37565 |   88 | `}` |
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
|    2996678 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2996683 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1199 |  103 | `		pSet->nUsed = nNewSize;` |
|        597 |  104 | `	}` |
|    2996683 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   59228938 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   59228943 |  109 | `	sxi32 rc = SXRET_OK;` |
|   59228943 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   32033519 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   16017432 |  112 | `	}` |
|   59228943 |  113 | `	pSet->pBase = 0;` |
|   59228943 |  114 | `	pSet->nUsed = 0;` |
|   59228943 |  115 | `	pSet->nCursor = 0;` |
|   59228943 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   69292650 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   69292655 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19467 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   69273193 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   69273193 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   34646330 |  126 | `}` |
|    9679564 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9679569 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2234813 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7444761 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7444761 |  135 | `	pSet->nUsed--;` |
|    7444761 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7444761 |  137 | `	return pData;` |
|    4840237 |  138 | `}` |
|   35785629 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   35785634 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   35785582 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   35785582 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   17896979 |  148 | `}` |
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
|    1897724 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1897729 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1897729 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1897729 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1897729 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1897729 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1897729 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1897729 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1897729 |  180 | `	pHash->nEntry = 0;` |
|    1897729 |  181 | `	pHash->apBucket = apNew;` |
|    1897729 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1897729 |  183 | `	return SXRET_OK;` |
|     948942 |  184 | `}` |
|     441094 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     441099 |  193 | `	pEntry = pHash->pList;` |
|     235284 |  194 | `	for(;;){` |
|     470423 |  195 | `		if( pHash->nEntry == 0 ){` |
|     441099 |  196 | `			break;` |
|          - |  197 | `		}` |
|      29329 |  198 | `		pNext = pEntry->pNext;` |
|      29329 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      29329 |  200 | `		pEntry = pNext;` |
|      29329 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     441099 |  203 | `	if( pHash->apBucket ){` |
|     441099 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     220622 |  205 | `	}` |
|     441099 |  206 | `	pHash->apBucket = 0;` |
|     441099 |  207 | `	pHash->nBucketSize = 0;` |
|     441099 |  208 | `	pHash->pAllocator = 0;` |
|     441099 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   72414938 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   72414943 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   72414943 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   65234579 |  218 | `	for(;;){` |
|  130480991 |  219 | `		if( pEntry == 0 ){` |
|   26653917 |  220 | `			break;` |
|          - |  221 | `		}` |
|  126706694 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   45765062 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   45761031 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   58066053 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   26653917 |  229 | `	return 0;` |
|   36214119 |  230 | `}` |
|   79815282 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   79815287 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    7400781 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   72414511 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   72414511 |  244 | `	if( pEntry == 0 ){` |
|   26653899 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   45760617 |  247 | `	return (SyHashEntry *)pEntry;` |
|   39914366 |  248 | `}` |
|     487826 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     487831 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     398961 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     199858 |  254 | `	}else{` |
|      88875 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     487831 |  257 | `	if( pEntry->pNextCollide ){` |
|       4630 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2314 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     487831 |  261 | `	if( pHash->pLast == pEntry ){` |
|     477791 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     239343 |  263 | `	}` |
|     487831 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     487831 |  265 | `	pHash->nEntry--;` |
|     487831 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     487831 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     487831 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        432 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        437 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        437 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        419 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        419 |  288 | `	return rc;` |
|        221 |  289 | `}` |
|     487412 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     487417 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     487417 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     487417 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3096630 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3096635 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3096635 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   23813406 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   23813411 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3096369 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3096369 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   20717047 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   20717047 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   20717047 |  328 | `	return (SyHashEntry *)pEntry;` |
|   11906708 |  329 | `}` |
|         14 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|          1 |  331 | `{` |
|          - |  332 | `	SyHashEntry_Pr *pEntry;` |
|          - |  333 | `	sxi32 rc;` |
|          - |  334 | `	sxu32 n;` |
|          - |  335 | `#if defined(UNTRUST)` |
|          - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|          - |  337 | `		return 0;` |
|          - |  338 | `	}` |
|          - |  339 | `#endif` |
|         15 |  340 | `	pEntry = pHash->pList;` |
|       4557 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4543 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4543 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4543 |  348 | `		pEntry = pEntry->pNext;` |
|       2272 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|      99954 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      99959 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      99959 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      99959 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      99959 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18559991 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18460037 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18460037 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18460037 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18460037 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8843953 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4422396 |  375 | `		}` |
|   18460037 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18460037 |  378 | `		pEntry = pEntry->pNext;` |
|    9230021 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      99959 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      99959 |  382 | `	pHash->apBucket = apNew;` |
|      99959 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      99959 |  384 | `	return SXRET_OK;` |
|      49982 |  385 | `}` |
|   20482696 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   20482701 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   20482701 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   20482701 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   13062463 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    6531009 |  393 | `	}` |
|   20482701 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   20482701 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   20482649 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   20482701 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1062607 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1062607 |  408 | `		pHash->pLast = pEntry;` |
|     531376 |  409 | `	}` |
|   20482701 |  410 | `	pHash->nEntry++;` |
|   20482701 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   20482696 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   20482701 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      99959 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      99959 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      49977 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   20482701 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   20482701 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   20482701 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   20482701 |  435 | `	pEntry->pHash = pHash;` |
|   20482701 |  436 | `	pEntry->pKey = pKey;` |
|   20482701 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   20482701 |  438 | `	pEntry->pUserData = pUserData;` |
|   20482701 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   20482701 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   20482701 |  442 | `	return rc;` |
|   10241803 |  443 | `}` |
|   20482562 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   20482567 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        134 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          2 |  455 | `{` |
|        136 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          2 |  457 | `}` |
|     527700 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     527705 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
