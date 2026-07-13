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
|   80570050 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|   80570055 |   16 | `	pSet->nSize = 0 ;` |
|   80570055 |   17 | `	pSet->nUsed = 0;` |
|   80570055 |   18 | `	pSet->nCursor = 0;` |
|   80570055 |   19 | `	pSet->eSize = ElemSize;` |
|   80570055 |   20 | `	pSet->pAllocator = pAllocator;` |
|   80570055 |   21 | `	pSet->pBase =  0;` |
|   80570055 |   22 | `	pSet->pUserData = 0;` |
|   80570055 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  174120413 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  174120418 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   11705771 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   11705771 |   33 | `		if( pSet->nSize <= 0 ){` |
|   10340111 |   34 | `			pSet->nSize = 4;` |
|    5170053 |   35 | `		}` |
|   11705771 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   11705771 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   11705771 |   40 | `		pSet->pBase = pNew;` |
|   11705771 |   41 | `		pSet->nSize <<= 1;` |
|    5852883 |   42 | `	}` |
|  174120418 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 1283038238 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  174120418 |   45 | `	pSet->nUsed++;` |
|  174120418 |   46 | `	return SXRET_OK;` |
|   87060255 |   47 | `}` |
|    8625784 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|    8625789 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|    8625789 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|    8625789 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    8625789 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|    8625789 |   60 | `	pSet->nSize = nItem;` |
|    8625789 |   61 | `	return SXRET_OK;` |
|    4312897 |   62 | `}` |
|   13504869 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   13504874 |   65 | `	pSet->nUsed   = 0;` |
|   13504874 |   66 | `	pSet->nCursor = 0;` |
|   13504874 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      67948 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      67953 |   71 | `	pSet->nCursor = 0;` |
|      67953 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      72108 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      72113 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29233 |   79 | `		pSet->nCursor = 0;` |
|      29233 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      42885 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      42885 |   83 | `	if( ppEntry ){` |
|      42885 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21440 |   85 | `	}` |
|      42885 |   86 | `	pSet->nCursor++;` |
|      42885 |   87 | `	return SXRET_OK;` |
|      36059 |   88 | `}` |
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
|    1392006 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    1392011 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        683 |  103 | `		pSet->nUsed = nNewSize;` |
|        339 |  104 | `	}` |
|    1392011 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   30523846 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   30523851 |  109 | `	sxi32 rc = SXRET_OK;` |
|   30523851 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   16148771 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|    8074383 |  112 | `	}` |
|   30523851 |  113 | `	pSet->pBase = 0;` |
|   30523851 |  114 | `	pSet->nUsed = 0;` |
|   30523851 |  115 | `	pSet->nCursor = 0;` |
|   30523851 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   30432742 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   30432747 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        133 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   30432619 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   30432619 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   15216376 |  126 | `}` |
|    6222712 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    6222717 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2392247 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    3830475 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    3830475 |  135 | `	pSet->nUsed--;` |
|    3830475 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    3830475 |  137 | `	return pData;` |
|    3111361 |  138 | `}` |
|   20994429 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   20994434 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|        ! 0 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   20994434 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   20994434 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   10497548 |  148 | `}` |
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
|    1145168 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1145173 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1145173 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1145173 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1145173 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1145173 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1145173 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1145173 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1145173 |  180 | `	pHash->nEntry = 0;` |
|    1145173 |  181 | `	pHash->apBucket = apNew;` |
|    1145173 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1145173 |  183 | `	return SXRET_OK;` |
|     572589 |  184 | `}` |
|     303674 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     303679 |  193 | `	pEntry = pHash->pList;` |
|     159926 |  194 | `	for(;;){` |
|     319857 |  195 | `		if( pHash->nEntry == 0 ){` |
|     303679 |  196 | `			break;` |
|          - |  197 | `		}` |
|      16183 |  198 | `		pNext = pEntry->pNext;` |
|      16183 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      16183 |  200 | `		pEntry = pNext;` |
|      16183 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     303679 |  203 | `	if( pHash->apBucket ){` |
|     303679 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     151837 |  205 | `	}` |
|     303679 |  206 | `	pHash->apBucket = 0;` |
|     303679 |  207 | `	pHash->nBucketSize = 0;` |
|     303679 |  208 | `	pHash->pAllocator = 0;` |
|     303679 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   39392810 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   39392815 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   39392815 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   36906513 |  218 | `	for(;;){` |
|   73763064 |  219 | `		if( pEntry == 0 ){` |
|   15768057 |  220 | `			break;` |
|          - |  221 | `		}` |
|   69807141 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   23624780 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   23624763 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   34370254 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   15768057 |  229 | `	return 0;` |
|   19696932 |  230 | `}` |
|   42926568 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   42926573 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    3534027 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   39392551 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   39392551 |  244 | `	if( pEntry == 0 ){` |
|   15768057 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   23624499 |  247 | `	return (SyHashEntry *)pEntry;` |
|   21463811 |  248 | `}` |
|     203780 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     203785 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     162107 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      81056 |  254 | `	}else{` |
|      41683 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     203785 |  257 | `	if( pEntry->pNextCollide ){` |
|       3622 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       1809 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     203785 |  261 | `	if( pHash->pLast == pEntry ){` |
|     197191 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|      98593 |  263 | `	}` |
|     203785 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     203785 |  265 | `	pHash->nEntry--;` |
|     203785 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|        ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        ! 0 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     203785 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     203785 |  272 | `	return rc;` |
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
|     203516 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     203521 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     203521 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     203521 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    1727296 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    1727301 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    1727301 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   12851394 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   12851399 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    1727039 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    1727039 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   11124365 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   11124365 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   11124365 |  328 | `	return (SyHashEntry *)pEntry;` |
|    6425702 |  329 | `}` |
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
|    4379092 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    2189750 |  375 | `		}` |
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
|   11262526 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   11262531 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   11262531 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   11262531 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    7067318 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    3533509 |  393 | `	}` |
|   11262531 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   11262531 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   11262479 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   11262531 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     594723 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     594723 |  408 | `		pHash->pLast = pEntry;` |
|     297359 |  409 | `	}` |
|   11262531 |  410 | `	pHash->nEntry++;` |
|   11262531 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   11262526 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   11262531 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      77073 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      77073 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      38534 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   11262531 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   11262531 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   11262531 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   11262531 |  435 | `	pEntry->pHash = pHash;` |
|   11262531 |  436 | `	pEntry->pKey = pKey;` |
|   11262531 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   11262531 |  438 | `	pEntry->pUserData = pUserData;` |
|   11262531 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   11262531 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   11262531 |  442 | `	return rc;` |
|    5631268 |  443 | `}` |
|   11262406 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   11262411 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     243930 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     243935 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
