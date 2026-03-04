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
|   9687006 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|   9687008 |   16 | `	pSet->nSize = 0 ;` |
|   9687008 |   17 | `	pSet->nUsed = 0;` |
|   9687008 |   18 | `	pSet->nCursor = 0;` |
|   9687008 |   19 | `	pSet->eSize = ElemSize;` |
|   9687008 |   20 | `	pSet->pAllocator = pAllocator;` |
|   9687008 |   21 | `	pSet->pBase =  0;` |
|   9687008 |   22 | `	pSet->pUserData = 0;` |
|   9687008 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  15265614 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  15265616 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3194248 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3194248 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3137106 |   34 | `			pSet->nSize = 4;` |
|   1568552 |   35 | `		}` |
|   3194248 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3194248 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3194248 |   40 | `		pSet->pBase = pNew;` |
|   3194248 |   41 | `		pSet->nSize <<= 1;` |
|   1597123 |   42 | `	}` |
|  15265616 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 115615952 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  15265616 |   45 | `	pSet->nUsed++;` |
|  15265616 |   46 | `	return SXRET_OK;` |
|   7632831 |   47 |  |
|    409528 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    409530 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    409530 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    409530 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    409530 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    409530 |   60 | `	pSet->nSize = nItem;` |
|    409530 |   61 | `	return SXRET_OK;` |
|    204766 |   62 |  |
|    816348 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|    816350 |   65 | `	pSet->nUsed   = 0;` |
|    816350 |   66 | `	pSet->nCursor = 0;` |
|    816350 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     33520 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     33522 |   71 | `	pSet->nCursor = 0;` |
|     33522 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     36702 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     36704 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     13398 |   79 | `		pSet->nCursor = 0;` |
|     13398 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     23308 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     23308 |   83 | `	if( ppEntry ){` |
|     23308 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     11653 |   85 | `	}` |
|     23308 |   86 | `	pSet->nCursor++;` |
|     23308 |   87 | `	return SXRET_OK;` |
|     18353 |   88 |  |
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
|     51676 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     51678 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     51678 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   6613748 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   6613750 |  109 | `	sxi32 rc = SXRET_OK;` |
|   6613750 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3394534 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1697266 |  112 | `	}` |
|   6613750 |  113 | `	pSet->pBase = 0;` |
|   6613750 |  114 | `	pSet->nUsed = 0;` |
|   6613750 |  115 | `	pSet->nCursor = 0;` |
|   6613750 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3224820 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3224822 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3224732 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3224732 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1612412 |  126 |  |
|   2898630 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   2898632 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2119812 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    778822 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    778822 |  135 | `	pSet->nUsed--;` |
|    778822 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    778822 |  137 | `	return pData;` |
|   1449317 |  138 |  |
|   8179298 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   8179300 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   8179300 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   8179300 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4089861 |  148 |  |
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
|     73568 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     73570 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     73570 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     73570 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     73570 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     73570 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     73570 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     73570 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     73570 |  180 | `	pHash->nEntry = 0;` |
|     73570 |  181 | `	pHash->apBucket = apNew;` |
|     73570 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     73570 |  183 | `	return SXRET_OK;` |
|     36786 |  184 |  |
|      9442 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|      9444 |  193 | `	pEntry = pHash->pList;` |
|      5399 |  194 | `	for(;;){` |
|     10800 |  195 | `		if( pHash->nEntry == 0 ){` |
|      9444 |  196 | `			break;` |
|         - |  197 | `		}` |
|      1358 |  198 | `		pNext = pEntry->pNext;` |
|      1358 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      1358 |  200 | `		pEntry = pNext;` |
|      1358 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|      9444 |  203 | `	if( pHash->apBucket ){` |
|      9444 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      4721 |  205 | `	}` |
|      9444 |  206 | `	pHash->apBucket = 0;` |
|      9444 |  207 | `	pHash->nBucketSize = 0;` |
|      9444 |  208 | `	pHash->pAllocator = 0;` |
|      9444 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   7490510 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   7490512 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   7490512 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   6522611 |  218 | `	for(;;){` |
|  13021776 |  219 | `		if( pEntry == 0 ){` |
|   4051110 |  220 | `			break;` |
|         - |  221 | `		}` |
|  10690239 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   3439406 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   3439404 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   5531266 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   4051110 |  229 | `	return 0;` |
|   3745521 |  230 |  |
|   7532178 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   7532180 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     41676 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   7490506 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   7490506 |  244 | `	if( pEntry == 0 ){` |
|   4051110 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   3439398 |  247 | `	return (SyHashEntry *)pEntry;` |
|   3766355 |  248 |  |
|     60852 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     60854 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     45514 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     22758 |  254 | `	}else{` |
|     15342 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     60854 |  257 | `	if( pEntry->pNextCollide ){` |
|      3623 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      1811 |  259 | `	}` |
|     60854 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     60854 |  261 | `	pHash->nEntry--;` |
|     60854 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     60854 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     60854 |  268 | `	return rc;` |
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
|     60846 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     60848 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     60848 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     60848 |  296 | `	return rc;` |
|         2 |  297 |  |
|    108448 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    108450 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    108450 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    756526 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    756528 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    108016 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    108016 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    648514 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    648514 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    648514 |  324 | `	return (SyHashEntry *)pEntry;` |
|    378265 |  325 |  |
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
|      1579 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1569 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1569 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1569 |  344 | `		pEntry = pEntry->pNext;` |
|       785 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     10252 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     10254 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     10254 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     10254 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     10254 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1402350 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1392098 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1392098 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1392098 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1392098 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    668522 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    334258 |  371 | `		}` |
|   1392098 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1392098 |  374 | `		pEntry = pEntry->pNext;` |
|    696050 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     10254 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     10254 |  378 | `	pHash->apBucket = apNew;` |
|     10254 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     10254 |  380 | `	return SXRET_OK;` |
|      5128 |  381 |  |
|   1270600 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1270602 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1270602 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1270602 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    844945 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    422494 |  389 | `	}` |
|   1270602 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1270602 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1270602 |  393 | `	if( pHash->nEntry == 0 ){` |
|     52714 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     26356 |  395 | `	}` |
|   1270602 |  396 | `	pHash->nEntry++;` |
|   1270602 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1270600 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1270602 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     10254 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     10254 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      5126 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1270602 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1270602 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1270602 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1270602 |  421 | `	pEntry->pHash = pHash;` |
|   1270602 |  422 | `	pEntry->pKey = pKey;` |
|   1270602 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1270602 |  424 | `	pEntry->pUserData = pUserData;` |
|   1270602 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1270602 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1270602 |  428 | `	return rc;` |
|    635302 |  429 |  |
|     73776 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     73778 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
