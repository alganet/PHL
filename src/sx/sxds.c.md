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
|   80608646 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|   80608651 |   16 | `	pSet->nSize = 0 ;` |
|   80608651 |   17 | `	pSet->nUsed = 0;` |
|   80608651 |   18 | `	pSet->nCursor = 0;` |
|   80608651 |   19 | `	pSet->eSize = ElemSize;` |
|   80608651 |   20 | `	pSet->pAllocator = pAllocator;` |
|   80608651 |   21 | `	pSet->pBase =  0;` |
|   80608651 |   22 | `	pSet->pUserData = 0;` |
|   80608651 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  174137981 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  174137986 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   11709477 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   11709477 |   33 | `		if( pSet->nSize <= 0 ){` |
|   10343741 |   34 | `			pSet->nSize = 4;` |
|    5171868 |   35 | `		}` |
|   11709477 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   11709477 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   11709477 |   40 | `		pSet->pBase = pNew;` |
|   11709477 |   41 | `		pSet->nSize <<= 1;` |
|    5854736 |   42 | `	}` |
|  174137986 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 1283139174 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  174137986 |   45 | `	pSet->nUsed++;` |
|  174137986 |   46 | `	return SXRET_OK;` |
|   87069038 |   47 | `}` |
|    8626330 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|    8626335 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|    8626335 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|    8626335 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    8626335 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|    8626335 |   60 | `	pSet->nSize = nItem;` |
|    8626335 |   61 | `	return SXRET_OK;` |
|    4313170 |   62 | `}` |
|   13510741 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   13510746 |   65 | `	pSet->nUsed   = 0;` |
|   13510746 |   66 | `	pSet->nCursor = 0;` |
|   13510746 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      67998 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      68003 |   71 | `	pSet->nCursor = 0;` |
|      68003 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      72160 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      72165 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29257 |   79 | `		pSet->nCursor = 0;` |
|      29257 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      42913 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      42913 |   83 | `	if( ppEntry ){` |
|      42913 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21454 |   85 | `	}` |
|      42913 |   86 | `	pSet->nCursor++;` |
|      42913 |   87 | `	return SXRET_OK;` |
|      36085 |   88 | `}` |
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
|    1392086 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    1392091 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        683 |  103 | `		pSet->nUsed = nNewSize;` |
|        339 |  104 | `	}` |
|    1392091 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   30538000 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   30538005 |  109 | `	sxi32 rc = SXRET_OK;` |
|   30538005 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   16152661 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|    8076328 |  112 | `	}` |
|   30538005 |  113 | `	pSet->pBase = 0;` |
|   30538005 |  114 | `	pSet->nUsed = 0;` |
|   30538005 |  115 | `	pSet->nCursor = 0;` |
|   30538005 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   30434876 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   30434881 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        133 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   30434753 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   30434753 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   15217443 |  126 | `}` |
|    6232194 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    6232199 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2392287 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    3839917 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    3839917 |  135 | `	pSet->nUsed--;` |
|    3839917 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    3839917 |  137 | `	return pData;` |
|    3116102 |  138 | `}` |
|   21019721 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   21019726 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|        ! 0 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   21019726 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   21019726 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   10510207 |  148 | `}` |
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
|    1145640 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1145645 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1145645 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1145645 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1145645 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1145645 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1145645 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1145645 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1145645 |  180 | `	pHash->nEntry = 0;` |
|    1145645 |  181 | `	pHash->apBucket = apNew;` |
|    1145645 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1145645 |  183 | `	return SXRET_OK;` |
|     572825 |  184 | `}` |
|     303944 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     303949 |  193 | `	pEntry = pHash->pList;` |
|     160145 |  194 | `	for(;;){` |
|     320295 |  195 | `		if( pHash->nEntry == 0 ){` |
|     303949 |  196 | `			break;` |
|          - |  197 | `		}` |
|      16351 |  198 | `		pNext = pEntry->pNext;` |
|      16351 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      16351 |  200 | `		pEntry = pNext;` |
|      16351 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     303949 |  203 | `	if( pHash->apBucket ){` |
|     303949 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     151972 |  205 | `	}` |
|     303949 |  206 | `	pHash->apBucket = 0;` |
|     303949 |  207 | `	pHash->nBucketSize = 0;` |
|     303949 |  208 | `	pHash->pAllocator = 0;` |
|     303949 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   39398406 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   39398411 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   39398411 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   37036780 |  218 | `	for(;;){` |
|   73780289 |  219 | `		if( pEntry == 0 ){` |
|   15772349 |  220 | `			break;` |
|          - |  221 | `		}` |
|   69820736 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   23626092 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   23626067 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   34381883 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   15772349 |  229 | `	return 0;` |
|   19699718 |  230 | `}` |
|   42932902 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   42932907 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    3534765 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   39398147 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   39398147 |  244 | `	if( pEntry == 0 ){` |
|   15772349 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   23625803 |  247 | `	return (SyHashEntry *)pEntry;` |
|   21466966 |  248 | `}` |
|     204146 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     204151 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     162449 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      81227 |  254 | `	}else{` |
|      41707 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     204151 |  257 | `	if( pEntry->pNextCollide ){` |
|       3608 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       1802 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     204151 |  261 | `	if( pHash->pLast == pEntry ){` |
|     197557 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|      98776 |  263 | `	}` |
|     204151 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     204151 |  265 | `	pHash->nEntry--;` |
|     204151 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|        ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        ! 0 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     204151 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     204151 |  272 | `	return rc;` |
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
|     203882 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     203887 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     203887 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     203887 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    1733874 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    1733879 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    1733879 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   12935520 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   12935525 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    1733617 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    1733617 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   11201913 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   11201913 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   11201913 |  328 | `	return (SyHashEntry *)pEntry;` |
|    6467765 |  329 | `}` |
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
|       2805 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       2795 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       2795 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       2795 |  348 | `		pEntry = pEntry->pNext;` |
|       1398 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      77068 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      77073 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      77073 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      77073 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      77073 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|    9170193 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|    9093125 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|    9093125 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|    9093125 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|    9093125 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    4375863 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    2187760 |  375 | `		}` |
|    9093125 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|    9093125 |  378 | `		pEntry = pEntry->pNext;` |
|    4546565 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      77073 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      77073 |  382 | `	pHash->apBucket = apNew;` |
|      77073 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      77073 |  384 | `	return SXRET_OK;` |
|      38539 |  385 | `}` |
|   11266888 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   11266893 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   11266893 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   11266893 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    7074369 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    3537280 |  393 | `	}` |
|   11266893 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   11266893 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   11266841 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   11266893 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     594997 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     594997 |  408 | `		pHash->pLast = pEntry;` |
|     297496 |  409 | `	}` |
|   11266893 |  410 | `	pHash->nEntry++;` |
|   11266893 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   11266888 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   11266893 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      77073 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      77073 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      38534 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   11266893 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   11266893 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   11266893 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   11266893 |  435 | `	pEntry->pHash = pHash;` |
|   11266893 |  436 | `	pEntry->pKey = pKey;` |
|   11266893 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   11266893 |  438 | `	pEntry->pUserData = pUserData;` |
|   11266893 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   11266893 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   11266893 |  442 | `	return rc;` |
|    5633449 |  443 | `}` |
|   11266760 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   11266765 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     244304 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     244309 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
