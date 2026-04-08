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
|  13936346 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  13936348 |   16 | `	pSet->nSize = 0 ;` |
|  13936348 |   17 | `	pSet->nUsed = 0;` |
|  13936348 |   18 | `	pSet->nCursor = 0;` |
|  13936348 |   19 | `	pSet->eSize = ElemSize;` |
|  13936348 |   20 | `	pSet->pAllocator = pAllocator;` |
|  13936348 |   21 | `	pSet->pBase =  0;` |
|  13936348 |   22 | `	pSet->pUserData = 0;` |
|  13936348 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  23032804 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  23032806 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3848566 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3848566 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3743522 |   34 | `			pSet->nSize = 4;` |
|   1871760 |   35 | `		}` |
|   3848566 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3848566 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3848566 |   40 | `		pSet->pBase = pNew;` |
|   3848566 |   41 | `		pSet->nSize <<= 1;` |
|   1924282 |   42 | `	}` |
|  23032806 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 171232166 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  23032806 |   45 | `	pSet->nUsed++;` |
|  23032806 |   46 | `	return SXRET_OK;` |
|  11516426 |   47 |  |
|    828112 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    828114 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    828114 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    828114 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    828114 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    828114 |   60 | `	pSet->nSize = nItem;` |
|    828114 |   61 | `	return SXRET_OK;` |
|    414058 |   62 |  |
|   1273692 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1273694 |   65 | `	pSet->nUsed   = 0;` |
|   1273694 |   66 | `	pSet->nCursor = 0;` |
|   1273694 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     43980 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     43982 |   71 | `	pSet->nCursor = 0;` |
|     43982 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     48034 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     48036 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     18018 |   79 | `		pSet->nCursor = 0;` |
|     18018 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     30020 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     30020 |   83 | `	if( ppEntry ){` |
|     30020 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     15009 |   85 | `	}` |
|     30020 |   86 | `	pSet->nCursor++;` |
|     30020 |   87 | `	return SXRET_OK;` |
|     24019 |   88 |  |
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
|    137586 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    137588 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|    137588 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8294490 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8294492 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8294492 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4236634 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2118316 |  112 | `	}` |
|   8294492 |  113 | `	pSet->pBase = 0;` |
|   8294492 |  114 | `	pSet->nUsed = 0;` |
|   8294492 |  115 | `	pSet->nCursor = 0;` |
|   8294492 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4492348 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4492350 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4492260 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4492260 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2246176 |  126 |  |
|   3258010 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3258012 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2141992 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1116022 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1116022 |  135 | `	pSet->nUsed--;` |
|   1116022 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1116022 |  137 | `	return pData;` |
|   1629007 |  138 |  |
|  10378550 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  10378552 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  10378552 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  10378552 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5189489 |  148 |  |
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
|    178624 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    178626 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    178626 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    178626 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    178626 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    178626 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    178626 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    178626 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    178626 |  180 | `	pHash->nEntry = 0;` |
|    178626 |  181 | `	pHash->apBucket = apNew;` |
|    178626 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    178626 |  183 | `	return SXRET_OK;` |
|     89314 |  184 |  |
|     29864 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     29866 |  193 | `	pEntry = pHash->pList;` |
|     16677 |  194 | `	for(;;){` |
|     33356 |  195 | `		if( pHash->nEntry == 0 ){` |
|     29866 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3492 |  198 | `		pNext = pEntry->pNext;` |
|      3492 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3492 |  200 | `		pEntry = pNext;` |
|      3492 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     29866 |  203 | `	if( pHash->apBucket ){` |
|     29866 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     14932 |  205 | `	}` |
|     29866 |  206 | `	pHash->apBucket = 0;` |
|     29866 |  207 | `	pHash->nBucketSize = 0;` |
|     29866 |  208 | `	pHash->pAllocator = 0;` |
|     29866 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  11308600 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  11308602 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  11308602 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  10349326 |  218 | `	for(;;){` |
|  20618492 |  219 | `		if( pEntry == 0 ){` |
|   6254340 |  220 | `			break;` |
|         - |  221 | `		}` |
|  16891155 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5054266 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5054264 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   9309892 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   6254340 |  229 | `	return 0;` |
|   5654566 |  230 |  |
|  11414516 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  11414518 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    105940 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  11308580 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  11308580 |  244 | `	if( pEntry == 0 ){` |
|   6254340 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5054242 |  247 | `	return (SyHashEntry *)pEntry;` |
|   5707524 |  248 |  |
|     85162 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     85164 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     64692 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     32347 |  254 | `	}else{` |
|     20474 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     85164 |  257 | `	if( pEntry->pNextCollide ){` |
|      4479 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2239 |  259 | `	}` |
|     85164 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     85164 |  261 | `	pHash->nEntry--;` |
|     85164 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     85164 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     85164 |  268 | `	return rc;` |
|         2 |  269 |  |
|        22 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|        24 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        24 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|        24 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        24 |  284 | `	return rc;` |
|        13 |  285 |  |
|     85140 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     85142 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     85142 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     85142 |  296 | `	return rc;` |
|         2 |  297 |  |
|    261208 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    261210 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    261210 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   1959822 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   1959824 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    260776 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    260776 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   1699050 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   1699050 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   1699050 |  324 | `	return (SyHashEntry *)pEntry;` |
|    979913 |  325 |  |
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
|      1761 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1751 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1751 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1751 |  344 | `		pEntry = pEntry->pNext;` |
|       876 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     21528 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     21530 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     21530 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     21530 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     21530 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2731130 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2709602 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2709602 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2709602 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2709602 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1294815 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    647433 |  371 | `		}` |
|   2709602 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2709602 |  374 | `		pEntry = pEntry->pNext;` |
|   1354802 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     21530 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     21530 |  378 | `	pHash->apBucket = apNew;` |
|     21530 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     21530 |  380 | `	return SXRET_OK;` |
|     10766 |  381 |  |
|   2686150 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2686152 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2686152 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2686152 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1787812 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    893948 |  389 | `	}` |
|   2686152 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2686152 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2686152 |  393 | `	if( pHash->nEntry == 0 ){` |
|    110700 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     55349 |  395 | `	}` |
|   2686152 |  396 | `	pHash->nEntry++;` |
|   2686152 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2686150 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2686152 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     21530 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     21530 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     10764 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2686152 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2686152 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2686152 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2686152 |  421 | `	pEntry->pHash = pHash;` |
|   2686152 |  422 | `	pEntry->pKey = pKey;` |
|   2686152 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2686152 |  424 | `	pEntry->pUserData = pUserData;` |
|   2686152 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2686152 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2686152 |  428 | `	return rc;` |
|   1343077 |  429 |  |
|    109328 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    109330 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
