# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 289/304 lines (95.07%)

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
|   80521556 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|   80521561 |   16 | `	pSet->nSize = 0 ;` |
|   80521561 |   17 | `	pSet->nUsed = 0;` |
|   80521561 |   18 | `	pSet->nCursor = 0;` |
|   80521561 |   19 | `	pSet->eSize = ElemSize;` |
|   80521561 |   20 | `	pSet->pAllocator = pAllocator;` |
|   80521561 |   21 | `	pSet->pBase =  0;` |
|   80521561 |   22 | `	pSet->pUserData = 0;` |
|   80521561 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  174022005 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  174022010 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   11698273 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   11698273 |   33 | `		if( pSet->nSize <= 0 ){` |
|   10333357 |   34 | `			pSet->nSize = 4;` |
|    5166676 |   35 | `		}` |
|   11698273 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   11698273 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   11698273 |   40 | `		pSet->pBase = pNew;` |
|   11698273 |   41 | `		pSet->nSize <<= 1;` |
|    5849134 |   42 | `	}` |
|  174022010 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 1282334414 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  174022010 |   45 | `	pSet->nUsed++;` |
|  174022010 |   46 | `	return SXRET_OK;` |
|   87011050 |   47 | `}` |
|    8620956 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|    8620961 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|    8620961 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|    8620961 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    8620961 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|    8620961 |   60 | `	pSet->nSize = nItem;` |
|    8620961 |   61 | `	return SXRET_OK;` |
|    4310483 |   62 | `}` |
|   13496599 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   13496604 |   65 | `	pSet->nUsed   = 0;` |
|   13496604 |   66 | `	pSet->nCursor = 0;` |
|   13496604 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      67892 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      67897 |   71 | `	pSet->nCursor = 0;` |
|      67897 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      72052 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      72057 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29207 |   79 | `		pSet->nCursor = 0;` |
|      29207 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      42855 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      42855 |   83 | `	if( ppEntry ){` |
|      42855 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21425 |   85 | `	}` |
|      42855 |   86 | `	pSet->nCursor++;` |
|      42855 |   87 | `	return SXRET_OK;` |
|      36031 |   88 | `}` |
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
|    1391226 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    1391231 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        679 |  103 | `		pSet->nUsed = nNewSize;` |
|        337 |  104 | `	}` |
|    1391231 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   30504558 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   30504563 |  109 | `	sxi32 rc = SXRET_OK;` |
|   30504563 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   16138901 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|    8069448 |  112 | `	}` |
|   30504563 |  113 | `	pSet->pBase = 0;` |
|   30504563 |  114 | `	pSet->nUsed = 0;` |
|   30504563 |  115 | `	pSet->nCursor = 0;` |
|   30504563 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   30416880 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   30416885 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        133 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   30416757 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   30416757 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   15208445 |  126 | `}` |
|    6218650 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    6218655 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2392049 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    3826611 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    3826611 |  135 | `	pSet->nUsed--;` |
|    3826611 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    3826611 |  137 | `	return pData;` |
|    3109330 |  138 | `}` |
|   20973121 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   20973126 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|        ! 0 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   20973126 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   20973126 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   10486899 |  148 | `}` |
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
|    1144118 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1144123 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1144123 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1144123 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1144123 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1144123 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1144123 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1144123 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1144123 |  180 | `	pHash->nEntry = 0;` |
|    1144123 |  181 | `	pHash->apBucket = apNew;` |
|    1144123 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1144123 |  183 | `	return SXRET_OK;` |
|     572064 |  184 | `}` |
|     303118 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     303123 |  193 | `	pEntry = pHash->pList;` |
|     159505 |  194 | `	for(;;){` |
|     319015 |  195 | `		if( pHash->nEntry == 0 ){` |
|     303123 |  196 | `			break;` |
|          - |  197 | `		}` |
|      15897 |  198 | `		pNext = pEntry->pNext;` |
|      15897 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      15897 |  200 | `		pEntry = pNext;` |
|      15897 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     303123 |  203 | `	if( pHash->apBucket ){` |
|     303123 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     151559 |  205 | `	}` |
|     303123 |  206 | `	pHash->apBucket = 0;` |
|     303123 |  207 | `	pHash->nBucketSize = 0;` |
|     303123 |  208 | `	pHash->pAllocator = 0;` |
|     303123 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   39361552 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   39361557 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   39361557 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   36813458 |  218 | `	for(;;){` |
|   73803495 |  219 | `		if( pEntry == 0 ){` |
|   15754499 |  220 | `			break;` |
|          - |  221 | `		}` |
|   69852286 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   23607080 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   23607063 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   34441943 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   15754499 |  229 | `	return 0;` |
|   19681291 |  230 | `}` |
|   42892866 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   42892871 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    3531583 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   39361293 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   39361293 |  244 | `	if( pEntry == 0 ){` |
|   15754499 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   23606799 |  247 | `	return (SyHashEntry *)pEntry;` |
|   21446948 |  248 | `}` |
|     203056 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     203061 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     161449 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      80727 |  254 | `	}else{` |
|      41617 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     203061 |  257 | `	if( pEntry->pNextCollide ){` |
|       3604 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       1801 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     203061 |  261 | `	if( pHash->pLast == pEntry ){` |
|     196469 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|      98232 |  263 | `	}` |
|     203061 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     203061 |  265 | `	pHash->nEntry--;` |
|     203061 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|        ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        ! 0 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     203061 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     203061 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        264 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        269 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        269 |  284 | `	if( pEntry == 0 ){` |
|        ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        269 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        269 |  288 | `	return rc;` |
|        137 |  289 | `}` |
|     202792 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     202797 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     202797 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     202797 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    1723678 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    1723683 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    1723683 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   12831216 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   12831221 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    1723421 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    1723421 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   11107805 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   11107805 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   11107805 |  328 | `	return (SyHashEntry *)pEntry;` |
|    6415613 |  329 | `}` |
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
|       2777 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       2767 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       2767 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       2767 |  348 | `		pEntry = pEntry->pNext;` |
|       1384 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      77026 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      77031 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      77031 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      77031 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      77031 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|    9165159 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|    9088133 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|    9088133 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|    9088133 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|    9088133 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    4377206 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    2188589 |  375 | `		}` |
|    9088133 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|    9088133 |  378 | `		pEntry = pEntry->pNext;` |
|    4544069 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      77031 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      77031 |  382 | `	pHash->apBucket = apNew;` |
|      77031 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      77031 |  384 | `	return SXRET_OK;` |
|      38518 |  385 | `}` |
|   11255406 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   11255411 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   11255411 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   11255411 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    7062762 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    3531448 |  393 | `	}` |
|   11255411 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   11255411 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   11255359 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   11255411 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     594085 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     594085 |  408 | `		pHash->pLast = pEntry;` |
|     297040 |  409 | `	}` |
|   11255411 |  410 | `	pHash->nEntry++;` |
|   11255411 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   11255406 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   11255411 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      77031 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      77031 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      38513 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   11255411 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   11255411 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   11255411 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   11255411 |  435 | `	pEntry->pHash = pHash;` |
|   11255411 |  436 | `	pEntry->pKey = pKey;` |
|   11255411 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   11255411 |  438 | `	pEntry->pUserData = pUserData;` |
|   11255411 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   11255411 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   11255411 |  442 | `	return rc;` |
|    5627708 |  443 | `}` |
|   11255286 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   11255291 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        120 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          2 |  455 | `{` |
|        122 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          2 |  457 | `}` |
|     243182 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     243187 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
