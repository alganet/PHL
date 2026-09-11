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
|  162187646 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  162187651 |   16 | `	pSet->nSize = 0 ;` |
|  162187651 |   17 | `	pSet->nUsed = 0;` |
|  162187651 |   18 | `	pSet->nCursor = 0;` |
|  162187651 |   19 | `	pSet->eSize = ElemSize;` |
|  162187651 |   20 | `	pSet->pAllocator = pAllocator;` |
|  162187651 |   21 | `	pSet->pBase =  0;` |
|  162187651 |   22 | `	pSet->pUserData = 0;` |
|  162187651 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  369246679 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  369246684 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   21543067 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   21543067 |   33 | `		if( pSet->nSize <= 0 ){` |
|   18380439 |   34 | `			pSet->nSize = 4;` |
|    9190217 |   35 | `		}` |
|   21543067 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   21543067 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   21543067 |   40 | `		pSet->pBase = pNew;` |
|   21543067 |   41 | `		pSet->nSize <<= 1;` |
|   10771531 |   42 | `	}` |
|  369246684 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2912663698 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  369246684 |   45 | `	pSet->nUsed++;` |
|  369246684 |   46 | `	return SXRET_OK;` |
|  184623368 |   47 | `}` |
|   18318668 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   18318673 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   18318673 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   18318673 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   18318673 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   18318673 |   60 | `	pSet->nSize = nItem;` |
|   18318673 |   61 | `	return SXRET_OK;` |
|    9159339 |   62 | `}` |
|   27249277 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   27249282 |   65 | `	pSet->nUsed   = 0;` |
|   27249282 |   66 | `	pSet->nCursor = 0;` |
|   27249282 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      70450 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      70455 |   71 | `	pSet->nCursor = 0;` |
|      70455 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      74632 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      74637 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30475 |   79 | `		pSet->nCursor = 0;` |
|      30475 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      44167 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      44167 |   83 | `	if( ppEntry ){` |
|      44167 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      22081 |   85 | `	}` |
|      44167 |   86 | `	pSet->nCursor++;` |
|      44167 |   87 | `	return SXRET_OK;` |
|      37321 |   88 | `}` |
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
|    2754542 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2754547 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1199 |  103 | `		pSet->nUsed = nNewSize;` |
|        597 |  104 | `	}` |
|    2754547 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   56323962 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   56323967 |  109 | `	sxi32 rc = SXRET_OK;` |
|   56323967 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   30675197 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   15337596 |  112 | `	}` |
|   56323967 |  113 | `	pSet->pBase = 0;` |
|   56323967 |  114 | `	pSet->nUsed = 0;` |
|   56323967 |  115 | `	pSet->nCursor = 0;` |
|   56323967 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   65551954 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   65551959 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19237 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   65532727 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   65532727 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   32775982 |  126 | `}` |
|    9411370 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9411375 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2232941 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7178439 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7178439 |  135 | `	pSet->nUsed--;` |
|    7178439 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7178439 |  137 | `	return pData;` |
|    4705690 |  138 | `}` |
|   34432981 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   34432986 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   34432934 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   34432934 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   17216660 |  148 | `}` |
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
|    1772202 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1772207 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1772207 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1772207 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1772207 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1772207 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1772207 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1772207 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1772207 |  180 | `	pHash->nEntry = 0;` |
|    1772207 |  181 | `	pHash->apBucket = apNew;` |
|    1772207 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1772207 |  183 | `	return SXRET_OK;` |
|     886106 |  184 | `}` |
|     413282 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     413287 |  193 | `	pEntry = pHash->pList;` |
|     221164 |  194 | `	for(;;){` |
|     442333 |  195 | `		if( pHash->nEntry == 0 ){` |
|     413287 |  196 | `			break;` |
|          - |  197 | `		}` |
|      29051 |  198 | `		pNext = pEntry->pNext;` |
|      29051 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      29051 |  200 | `		pEntry = pNext;` |
|      29051 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     413287 |  203 | `	if( pHash->apBucket ){` |
|     413287 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     206641 |  205 | `	}` |
|     413287 |  206 | `	pHash->apBucket = 0;` |
|     413287 |  207 | `	pHash->nBucketSize = 0;` |
|     413287 |  208 | `	pHash->pAllocator = 0;` |
|     413287 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   67449073 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   67449078 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   67449078 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   61366760 |  218 | `	for(;;){` |
|  122738461 |  219 | `		if( pEntry == 0 ){` |
|   24423560 |  220 | `			break;` |
|          - |  221 | `		}` |
|  119829538 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   43029546 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   43025523 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   55289388 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   24423560 |  229 | `	return 0;` |
|   33724825 |  230 | `}` |
|   74462983 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   74462988 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    7014393 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   67448600 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   67448600 |  244 | `	if( pEntry == 0 ){` |
|   24423542 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   43025063 |  247 | `	return (SyHashEntry *)pEntry;` |
|   37231780 |  248 | `}` |
|     437870 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     437875 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     357363 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     178684 |  254 | `	}else{` |
|      80517 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     437875 |  257 | `	if( pEntry->pNextCollide ){` |
|       4400 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2199 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     437875 |  261 | `	if( pHash->pLast == pEntry ){` |
|     427885 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     213940 |  263 | `	}` |
|     437875 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     437875 |  265 | `	pHash->nEntry--;` |
|     437875 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     437875 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     437875 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        478 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        483 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        483 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        465 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        465 |  288 | `	return rc;` |
|        244 |  289 | `}` |
|     437410 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     437415 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     437415 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     437415 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2841988 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2841993 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2841993 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   21258114 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   21258119 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2841727 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2841727 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18416397 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18416397 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18416397 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10629062 |  329 | `}` |
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
|       4105 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4091 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4091 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4091 |  348 | `		pEntry = pEntry->pNext;` |
|       2046 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|      91980 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      91985 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      91985 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      91985 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      91985 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14441105 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14349125 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14349125 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14349125 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14349125 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    6878910 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3439682 |  375 | `		}` |
|   14349125 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14349125 |  378 | `		pEntry = pEntry->pNext;` |
|    7174565 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      91985 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      91985 |  382 | `	pHash->apBucket = apNew;` |
|      91985 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      91985 |  384 | `	return SXRET_OK;` |
|      45995 |  385 | `}` |
|   18510366 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   18510371 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   18510371 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   18510371 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11743089 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5871124 |  393 | `	}` |
|   18510371 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   18510371 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   18510319 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   18510371 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     977353 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     977353 |  408 | `		pHash->pLast = pEntry;` |
|     488674 |  409 | `	}` |
|   18510371 |  410 | `	pHash->nEntry++;` |
|   18510371 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   18510366 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   18510371 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      91985 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      91985 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45990 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   18510371 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   18510371 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   18510371 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   18510371 |  435 | `	pEntry->pHash = pHash;` |
|   18510371 |  436 | `	pEntry->pKey = pKey;` |
|   18510371 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   18510371 |  438 | `	pEntry->pUserData = pUserData;` |
|   18510371 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   18510371 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   18510371 |  442 | `	return rc;` |
|    9255188 |  443 | `}` |
|   18510232 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   18510237 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     477066 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     477071 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
