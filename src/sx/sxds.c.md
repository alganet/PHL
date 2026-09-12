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
|  162315866 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  162315871 |   16 | `	pSet->nSize = 0 ;` |
|  162315871 |   17 | `	pSet->nUsed = 0;` |
|  162315871 |   18 | `	pSet->nCursor = 0;` |
|  162315871 |   19 | `	pSet->eSize = ElemSize;` |
|  162315871 |   20 | `	pSet->pAllocator = pAllocator;` |
|  162315871 |   21 | `	pSet->pBase =  0;` |
|  162315871 |   22 | `	pSet->pUserData = 0;` |
|  162315871 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  369484043 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  369484048 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   21559011 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   21559011 |   33 | `		if( pSet->nSize <= 0 ){` |
|   18394667 |   34 | `			pSet->nSize = 4;` |
|    9197331 |   35 | `		}` |
|   21559011 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   21559011 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   21559011 |   40 | `		pSet->pBase = pNew;` |
|   21559011 |   41 | `		pSet->nSize <<= 1;` |
|   10779503 |   42 | `	}` |
|  369484048 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2914598740 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  369484048 |   45 | `	pSet->nUsed++;` |
|  369484048 |   46 | `	return SXRET_OK;` |
|  184742050 |   47 | `}` |
|   18328864 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   18328869 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   18328869 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   18328869 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   18328869 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   18328869 |   60 | `	pSet->nSize = nItem;` |
|   18328869 |   61 | `	return SXRET_OK;` |
|    9164437 |   62 | `}` |
|   27273319 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   27273324 |   65 | `	pSet->nUsed   = 0;` |
|   27273324 |   66 | `	pSet->nCursor = 0;` |
|   27273324 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      70598 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      70603 |   71 | `	pSet->nCursor = 0;` |
|      70603 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      74780 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      74785 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30541 |   79 | `		pSet->nCursor = 0;` |
|      30541 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      44249 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      44249 |   83 | `	if( ppEntry ){` |
|      44249 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      22122 |   85 | `	}` |
|      44249 |   86 | `	pSet->nCursor++;` |
|      44249 |   87 | `	return SXRET_OK;` |
|      37395 |   88 | `}` |
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
|    2756000 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2756005 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1199 |  103 | `		pSet->nUsed = nNewSize;` |
|        597 |  104 | `	}` |
|    2756005 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   56362958 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   56362963 |  109 | `	sxi32 rc = SXRET_OK;` |
|   56362963 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   30692383 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   15346189 |  112 | `	}` |
|   56362963 |  113 | `	pSet->pBase = 0;` |
|   56362963 |  114 | `	pSet->nUsed = 0;` |
|   56362963 |  115 | `	pSet->nCursor = 0;` |
|   56362963 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   65594762 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   65594767 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19247 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   65575525 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   65575525 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   32797386 |  126 | `}` |
|    9417046 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9417051 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2233043 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7184013 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7184013 |  135 | `	pSet->nUsed--;` |
|    7184013 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7184013 |  137 | `	return pData;` |
|    4708528 |  138 | `}` |
|   34464231 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   34464236 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   34464184 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   34464184 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   17232266 |  148 | `}` |
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
|    1784682 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1784687 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1784687 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1784687 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1784687 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1784687 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1784687 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1784687 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1784687 |  180 | `	pHash->nEntry = 0;` |
|    1784687 |  181 | `	pHash->apBucket = apNew;` |
|    1784687 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1784687 |  183 | `	return SXRET_OK;` |
|     892346 |  184 | `}` |
|     413532 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     413537 |  193 | `	pEntry = pHash->pList;` |
|     221311 |  194 | `	for(;;){` |
|     442627 |  195 | `		if( pHash->nEntry == 0 ){` |
|     413537 |  196 | `			break;` |
|          - |  197 | `		}` |
|      29095 |  198 | `		pNext = pEntry->pNext;` |
|      29095 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      29095 |  200 | `		pEntry = pNext;` |
|      29095 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     413537 |  203 | `	if( pHash->apBucket ){` |
|     413537 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     206766 |  205 | `	}` |
|     413537 |  206 | `	pHash->apBucket = 0;` |
|     413537 |  207 | `	pHash->nBucketSize = 0;` |
|     413537 |  208 | `	pHash->pAllocator = 0;` |
|     413537 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   67547159 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   67547164 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   67547164 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   61022449 |  218 | `	for(;;){` |
|  122009751 |  219 | `		if( pEntry == 0 ){` |
|   24438712 |  220 | `			break;` |
|          - |  221 | `		}` |
|  119127131 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   43112456 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   43108457 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   54462592 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   24438712 |  229 | `	return 0;` |
|   33773868 |  230 | `}` |
|   74576467 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   74576472 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    7029745 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   67546732 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   67546732 |  244 | `	if( pEntry == 0 ){` |
|   24438694 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   43108043 |  247 | `	return (SyHashEntry *)pEntry;` |
|   37288522 |  248 | `}` |
|     437940 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     437945 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     357361 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     178683 |  254 | `	}else{` |
|      80589 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     437945 |  257 | `	if( pEntry->pNextCollide ){` |
|       4542 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2270 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     437945 |  261 | `	if( pHash->pLast == pEntry ){` |
|     427955 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     213975 |  263 | `	}` |
|     437945 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     437945 |  265 | `	pHash->nEntry--;` |
|     437945 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     437945 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     437945 |  272 | `	return rc;` |
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
|     437526 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     437531 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     437531 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     437531 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2880492 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2880497 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2880497 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   21489220 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   21489225 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2880231 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2880231 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18608999 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18608999 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18608999 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10744615 |  329 | `}` |
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
|       4113 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4099 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4099 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4099 |  348 | `		pEntry = pEntry->pNext;` |
|       2050 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|      92030 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      92035 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      92035 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      92035 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      92035 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14449027 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14356997 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14356997 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14356997 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14356997 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    6873512 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3436609 |  375 | `		}` |
|   14356997 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14356997 |  378 | `		pEntry = pEntry->pNext;` |
|    7178501 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      92035 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      92035 |  382 | `	pHash->apBucket = apNew;` |
|      92035 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      92035 |  384 | `	return SXRET_OK;` |
|      46020 |  385 | `}` |
|   18530010 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   18530015 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   18530015 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   18530015 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11717771 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5858857 |  393 | `	}` |
|   18530015 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   18530015 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   18529963 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   18530015 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     985453 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     985453 |  408 | `		pHash->pLast = pEntry;` |
|     492724 |  409 | `	}` |
|   18530015 |  410 | `	pHash->nEntry++;` |
|   18530015 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   18530010 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   18530015 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      92035 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      92035 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      46015 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   18530015 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   18530015 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   18530015 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   18530015 |  435 | `	pEntry->pHash = pHash;` |
|   18530015 |  436 | `	pEntry->pKey = pKey;` |
|   18530015 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   18530015 |  438 | `	pEntry->pUserData = pUserData;` |
|   18530015 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   18530015 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   18530015 |  442 | `	return rc;` |
|    9265010 |  443 | `}` |
|   18529876 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   18529881 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     477248 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     477253 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
