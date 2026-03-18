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
|  11488384 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  11488386 |   16 | `	pSet->nSize = 0 ;` |
|  11488386 |   17 | `	pSet->nUsed = 0;` |
|  11488386 |   18 | `	pSet->nCursor = 0;` |
|  11488386 |   19 | `	pSet->eSize = ElemSize;` |
|  11488386 |   20 | `	pSet->pAllocator = pAllocator;` |
|  11488386 |   21 | `	pSet->pBase =  0;` |
|  11488386 |   22 | `	pSet->pUserData = 0;` |
|  11488386 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  18574492 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  18574494 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3524522 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3524522 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3441554 |   34 | `			pSet->nSize = 4;` |
|   1720776 |   35 | `		}` |
|   3524522 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3524522 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3524522 |   40 | `		pSet->pBase = pNew;` |
|   3524522 |   41 | `		pSet->nSize <<= 1;` |
|   1762260 |   42 | `	}` |
|  18574494 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 138312270 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  18574494 |   45 | `	pSet->nUsed++;` |
|  18574494 |   46 | `	return SXRET_OK;` |
|   9287270 |   47 |  |
|    564946 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    564948 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    564948 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    564948 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    564948 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    564948 |   60 | `	pSet->nSize = nItem;` |
|    564948 |   61 | `	return SXRET_OK;` |
|    282475 |   62 |  |
|   1041042 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1041044 |   65 | `	pSet->nUsed   = 0;` |
|   1041044 |   66 | `	pSet->nCursor = 0;` |
|   1041044 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     39220 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     39222 |   71 | `	pSet->nCursor = 0;` |
|     39222 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     43070 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     43072 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     15886 |   79 | `		pSet->nCursor = 0;` |
|     15886 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     27188 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     27188 |   83 | `	if( ppEntry ){` |
|     27188 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     13593 |   85 | `	}` |
|     27188 |   86 | `	pSet->nCursor++;` |
|     27188 |   87 | `	return SXRET_OK;` |
|     21537 |   88 |  |
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
|     68674 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     68676 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     68676 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   7354750 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   7354752 |  109 | `	sxi32 rc = SXRET_OK;` |
|   7354752 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3804168 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1902083 |  112 | `	}` |
|   7354752 |  113 | `	pSet->pBase = 0;` |
|   7354752 |  114 | `	pSet->nUsed = 0;` |
|   7354752 |  115 | `	pSet->nCursor = 0;` |
|   7354752 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3711162 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3711164 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3711074 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3711074 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1855583 |  126 |  |
|   3114824 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3114826 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2131694 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    983134 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    983134 |  135 | `	pSet->nUsed--;` |
|    983134 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    983134 |  137 | `	return pData;` |
|   1557414 |  138 |  |
|   9707319 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   9707321 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   9707321 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   9707321 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4853907 |  148 |  |
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
|     96886 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     96888 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     96888 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     96888 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     96888 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     96888 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     96888 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     96888 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     96888 |  180 | `	pHash->nEntry = 0;` |
|     96888 |  181 | `	pHash->apBucket = apNew;` |
|     96888 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     96888 |  183 | `	return SXRET_OK;` |
|     48445 |  184 |  |
|     11814 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     11816 |  193 | `	pEntry = pHash->pList;` |
|      7349 |  194 | `	for(;;){` |
|     14700 |  195 | `		if( pHash->nEntry == 0 ){` |
|     11816 |  196 | `			break;` |
|         - |  197 | `		}` |
|      2886 |  198 | `		pNext = pEntry->pNext;` |
|      2886 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      2886 |  200 | `		pEntry = pNext;` |
|      2886 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     11816 |  203 | `	if( pHash->apBucket ){` |
|     11816 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      5907 |  205 | `	}` |
|     11816 |  206 | `	pHash->apBucket = 0;` |
|     11816 |  207 | `	pHash->nBucketSize = 0;` |
|     11816 |  208 | `	pHash->pAllocator = 0;` |
|     11816 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   9774274 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   9774276 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   9774276 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   8332808 |  218 | `	for(;;){` |
|  16784888 |  219 | `		if( pEntry == 0 ){` |
|   5302080 |  220 | `			break;` |
|         - |  221 | `		}` |
|  13718778 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   4472200 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   4472198 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   7010614 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   5302080 |  229 | `	return 0;` |
|   4887403 |  230 |  |
|   9828728 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   9828730 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     54462 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   9774270 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   9774270 |  244 | `	if( pEntry == 0 ){` |
|   5302080 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   4472192 |  247 | `	return (SyHashEntry *)pEntry;` |
|   4914630 |  248 |  |
|     73798 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     73800 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     55586 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     27794 |  254 | `	}else{` |
|     18216 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     73800 |  257 | `	if( pEntry->pNextCollide ){` |
|      4125 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2062 |  259 | `	}` |
|     73800 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     73800 |  261 | `	pHash->nEntry--;` |
|     73800 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     73800 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     73800 |  268 | `	return rc;` |
|         2 |  269 |  |
|         6 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         1 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|         7 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|         7 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|         7 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|         7 |  284 | `	return rc;` |
|         4 |  285 |  |
|     73792 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     73794 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     73794 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     73794 |  296 | `	return rc;` |
|         2 |  297 |  |
|    137278 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    137280 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    137280 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    954176 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    954178 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    136846 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    136846 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    817334 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    817334 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    817334 |  324 | `	return (SyHashEntry *)pEntry;` |
|    477090 |  325 |  |
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
|      1617 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1607 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1607 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1607 |  344 | `		pEntry = pEntry->pNext;` |
|       804 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     14376 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     14378 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     14378 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     14378 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     14378 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1977386 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1963010 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1963010 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1963010 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1963010 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    942568 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    471279 |  371 | `		}` |
|   1963010 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1963010 |  374 | `		pEntry = pEntry->pNext;` |
|    981506 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     14378 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     14378 |  378 | `	pHash->apBucket = apNew;` |
|     14378 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     14378 |  380 | `	return SXRET_OK;` |
|      7190 |  381 |  |
|   1773756 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1773758 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1773758 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1773758 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1199337 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    599654 |  389 | `	}` |
|   1773758 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1773758 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1773758 |  393 | `	if( pHash->nEntry == 0 ){` |
|     69636 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     34817 |  395 | `	}` |
|   1773758 |  396 | `	pHash->nEntry++;` |
|   1773758 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1773756 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1773758 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     14378 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     14378 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      7188 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1773758 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1773758 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1773758 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1773758 |  421 | `	pEntry->pHash = pHash;` |
|   1773758 |  422 | `	pEntry->pKey = pKey;` |
|   1773758 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1773758 |  424 | `	pEntry->pUserData = pUserData;` |
|   1773758 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1773758 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1773758 |  428 | `	return rc;` |
|    886880 |  429 |  |
|     91970 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     91972 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
