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
|  162196806 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  162196811 |   16 | `	pSet->nSize = 0 ;` |
|  162196811 |   17 | `	pSet->nUsed = 0;` |
|  162196811 |   18 | `	pSet->nCursor = 0;` |
|  162196811 |   19 | `	pSet->eSize = ElemSize;` |
|  162196811 |   20 | `	pSet->pAllocator = pAllocator;` |
|  162196811 |   21 | `	pSet->pBase =  0;` |
|  162196811 |   22 | `	pSet->pUserData = 0;` |
|  162196811 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  369262859 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  369262864 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   21544327 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   21544327 |   33 | `		if( pSet->nSize <= 0 ){` |
|   18381663 |   34 | `			pSet->nSize = 4;` |
|    9190829 |   35 | `		}` |
|   21544327 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   21544327 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   21544327 |   40 | `		pSet->pBase = pNew;` |
|   21544327 |   41 | `		pSet->nSize <<= 1;` |
|   10772161 |   42 | `	}` |
|  369262864 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2912859262 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  369262864 |   45 | `	pSet->nUsed++;` |
|  369262864 |   46 | `	return SXRET_OK;` |
|  184631458 |   47 | `}` |
|   18319074 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   18319079 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   18319079 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   18319079 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   18319079 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   18319079 |   60 | `	pSet->nSize = nItem;` |
|   18319079 |   61 | `	return SXRET_OK;` |
|    9159542 |   62 | `}` |
|   27250687 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   27250692 |   65 | `	pSet->nUsed   = 0;` |
|   27250692 |   66 | `	pSet->nCursor = 0;` |
|   27250692 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      70524 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      70529 |   71 | `	pSet->nCursor = 0;` |
|      70529 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      74706 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      74711 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30507 |   79 | `		pSet->nCursor = 0;` |
|      30507 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      44209 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      44209 |   83 | `	if( ppEntry ){` |
|      44209 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      22102 |   85 | `	}` |
|      44209 |   86 | `	pSet->nCursor++;` |
|      44209 |   87 | `	return SXRET_OK;` |
|      37358 |   88 | `}` |
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
|    2754554 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2754559 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1199 |  103 | `		pSet->nUsed = nNewSize;` |
|        597 |  104 | `	}` |
|    2754559 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   56326748 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   56326753 |  109 | `	sxi32 rc = SXRET_OK;` |
|   56326753 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   30676705 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   15338350 |  112 | `	}` |
|   56326753 |  113 | `	pSet->pBase = 0;` |
|   56326753 |  114 | `	pSet->nUsed = 0;` |
|   56326753 |  115 | `	pSet->nCursor = 0;` |
|   56326753 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   65560854 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   65560859 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19237 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   65541627 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   65541627 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   32780432 |  126 | `}` |
|    9412606 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9412611 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2232949 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7179667 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7179667 |  135 | `	pSet->nUsed--;` |
|    7179667 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7179667 |  137 | `	return pData;` |
|    4706308 |  138 | `}` |
|   34443843 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   34443848 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   34443796 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   34443796 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   17222090 |  148 | `}` |
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
|    1772258 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1772263 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1772263 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1772263 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1772263 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1772263 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1772263 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1772263 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1772263 |  180 | `	pHash->nEntry = 0;` |
|    1772263 |  181 | `	pHash->apBucket = apNew;` |
|    1772263 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1772263 |  183 | `	return SXRET_OK;` |
|     886134 |  184 | `}` |
|     413324 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     413329 |  193 | `	pEntry = pHash->pList;` |
|     221191 |  194 | `	for(;;){` |
|     442387 |  195 | `		if( pHash->nEntry == 0 ){` |
|     413329 |  196 | `			break;` |
|          - |  197 | `		}` |
|      29063 |  198 | `		pNext = pEntry->pNext;` |
|      29063 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      29063 |  200 | `		pEntry = pNext;` |
|      29063 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     413329 |  203 | `	if( pHash->apBucket ){` |
|     413329 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     206662 |  205 | `	}` |
|     413329 |  206 | `	pHash->apBucket = 0;` |
|     413329 |  207 | `	pHash->nBucketSize = 0;` |
|     413329 |  208 | `	pHash->pAllocator = 0;` |
|     413329 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   67387051 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   67387056 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   67387056 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   60850909 |  218 | `	for(;;){` |
|  121941274 |  219 | `		if( pEntry == 0 ){` |
|   24361636 |  220 | `			break;` |
|          - |  221 | `		}` |
|  119094212 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   43029420 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   43025425 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   54554223 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   24361636 |  229 | `	return 0;` |
|   33693814 |  230 | `}` |
|   74401067 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   74401072 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    7014453 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   67386624 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   67386624 |  244 | `	if( pEntry == 0 ){` |
|   24361618 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   43025011 |  247 | `	return (SyHashEntry *)pEntry;` |
|   37200822 |  248 | `}` |
|     437788 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     437793 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     357243 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     178624 |  254 | `	}else{` |
|      80555 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     437793 |  257 | `	if( pEntry->pNextCollide ){` |
|       4466 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2232 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     437793 |  261 | `	if( pHash->pLast == pEntry ){` |
|     427803 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     213899 |  263 | `	}` |
|     437793 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     437793 |  265 | `	pHash->nEntry--;` |
|     437793 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     437793 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     437793 |  272 | `	return rc;` |
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
|     437374 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     437379 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     437379 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     437379 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2842374 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2842379 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2842379 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   21264174 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   21264179 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2842113 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2842113 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18422071 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18422071 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18422071 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10632092 |  329 | `}` |
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
|       4107 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4093 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4093 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4093 |  348 | `		pEntry = pEntry->pNext;` |
|       2047 |  349 | `	}` |
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
|    6866941 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3433340 |  375 | `		}` |
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
|   18444838 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   18444843 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   18444843 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   18444843 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11689306 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5844915 |  393 | `	}` |
|   18444843 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   18444843 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   18444791 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   18444843 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     977297 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     977297 |  408 | `		pHash->pLast = pEntry;` |
|     488646 |  409 | `	}` |
|   18444843 |  410 | `	pHash->nEntry++;` |
|   18444843 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   18444838 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   18444843 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      91985 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      91985 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45990 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   18444843 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   18444843 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   18444843 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   18444843 |  435 | `	pEntry->pHash = pHash;` |
|   18444843 |  436 | `	pEntry->pKey = pKey;` |
|   18444843 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   18444843 |  438 | `	pEntry->pUserData = pUserData;` |
|   18444843 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   18444843 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   18444843 |  442 | `	return rc;` |
|    9222424 |  443 | `}` |
|   18444704 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   18444709 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     477058 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     477063 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
