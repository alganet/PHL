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
|  15181946 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  15181948 |   16 | `	pSet->nSize = 0 ;` |
|  15181948 |   17 | `	pSet->nUsed = 0;` |
|  15181948 |   18 | `	pSet->nCursor = 0;` |
|  15181948 |   19 | `	pSet->eSize = ElemSize;` |
|  15181948 |   20 | `	pSet->pAllocator = pAllocator;` |
|  15181948 |   21 | `	pSet->pBase =  0;` |
|  15181948 |   22 | `	pSet->pUserData = 0;` |
|  15181948 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  24666822 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  24666824 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3979486 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3979486 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3863714 |   34 | `			pSet->nSize = 4;` |
|   1931856 |   35 | `		}` |
|   3979486 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3979486 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3979486 |   40 | `		pSet->pBase = pNew;` |
|   3979486 |   41 | `		pSet->nSize <<= 1;` |
|   1989742 |   42 | `	}` |
|  24666824 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 183998076 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  24666824 |   45 | `	pSet->nUsed++;` |
|  24666824 |   46 | `	return SXRET_OK;` |
|  12333435 |   47 |  |
|    912546 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    912548 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    912548 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    912548 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    912548 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    912548 |   60 | `	pSet->nSize = nItem;` |
|    912548 |   61 | `	return SXRET_OK;` |
|    456275 |   62 |  |
|   1394926 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1394928 |   65 | `	pSet->nUsed   = 0;` |
|   1394928 |   66 | `	pSet->nCursor = 0;` |
|   1394928 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     48392 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     48394 |   71 | `	pSet->nCursor = 0;` |
|     48394 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     52474 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     52476 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     19890 |   79 | `		pSet->nCursor = 0;` |
|     19890 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     32588 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     32588 |   83 | `	if( ppEntry ){` |
|     32588 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     16293 |   85 | `	}` |
|     32588 |   86 | `	pSet->nCursor++;` |
|     32588 |   87 | `	return SXRET_OK;` |
|     26239 |   88 |  |
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
|    151690 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    151692 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|    151692 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8665874 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8665876 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8665876 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4405620 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2202809 |  112 | `	}` |
|   8665876 |  113 | `	pSet->pBase = 0;` |
|   8665876 |  114 | `	pSet->nUsed = 0;` |
|   8665876 |  115 | `	pSet->nCursor = 0;` |
|   8665876 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4737994 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4737996 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       108 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4737890 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4737890 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2368999 |  126 |  |
|   3327058 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3327060 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2146614 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1180448 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1180448 |  135 | `	pSet->nUsed--;` |
|   1180448 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1180448 |  137 | `	return pData;` |
|   1663531 |  138 |  |
|  11306695 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  11306697 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  11306697 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  11306697 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5653522 |  148 |  |
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
|    274690 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    274692 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    274692 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    274692 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    274692 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    274692 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    274692 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    274692 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    274692 |  180 | `	pHash->nEntry = 0;` |
|    274692 |  181 | `	pHash->apBucket = apNew;` |
|    274692 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    274692 |  183 | `	return SXRET_OK;` |
|    137347 |  184 |  |
|     81910 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     81912 |  193 | `	pEntry = pHash->pList;` |
|     43085 |  194 | `	for(;;){` |
|     86172 |  195 | `		if( pHash->nEntry == 0 ){` |
|     81912 |  196 | `			break;` |
|         - |  197 | `		}` |
|      4262 |  198 | `		pNext = pEntry->pNext;` |
|      4262 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      4262 |  200 | `		pEntry = pNext;` |
|      4262 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     81912 |  203 | `	if( pHash->apBucket ){` |
|     81912 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     40955 |  205 | `	}` |
|     81912 |  206 | `	pHash->apBucket = 0;` |
|     81912 |  207 | `	pHash->nBucketSize = 0;` |
|     81912 |  208 | `	pHash->pAllocator = 0;` |
|     81912 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  12504986 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  12504988 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  12504988 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  11283187 |  218 | `	for(;;){` |
|  22519075 |  219 | `		if( pEntry == 0 ){` |
|   6900980 |  220 | `			break;` |
|         - |  221 | `		}` |
|  18419971 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5604012 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5604010 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  10014089 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   6900980 |  229 | `	return 0;` |
|   6252759 |  230 |  |
|  13008650 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  13008652 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    503770 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  12504884 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  12504884 |  244 | `	if( pEntry == 0 ){` |
|   6900980 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5603906 |  247 | `	return (SyHashEntry *)pEntry;` |
|   6504591 |  248 |  |
|     95462 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     95464 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     72662 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     36332 |  254 | `	}else{` |
|     22804 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     95464 |  257 | `	if( pEntry->pNextCollide ){` |
|      4753 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2376 |  259 | `	}` |
|     95464 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     95464 |  261 | `	pHash->nEntry--;` |
|     95464 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     95464 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     95464 |  268 | `	return rc;` |
|         2 |  269 |  |
|       104 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       106 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       106 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       106 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       106 |  284 | `	return rc;` |
|        54 |  285 |  |
|     95358 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     95360 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     95360 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     95360 |  296 | `	return rc;` |
|         2 |  297 |  |
|    328342 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    328344 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    328344 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2608338 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2608340 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    327910 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    327910 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2280432 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2280432 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2280432 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1304171 |  325 |  |
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
|      1773 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1763 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1763 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1763 |  344 | `		pEntry = pEntry->pNext;` |
|       882 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     23732 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     23734 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     23734 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     23734 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     23734 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3014614 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2990882 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2990882 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2990882 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2990882 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1429222 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    714535 |  371 | `		}` |
|   2990882 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2990882 |  374 | `		pEntry = pEntry->pNext;` |
|   1495442 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     23734 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     23734 |  378 | `	pHash->apBucket = apNew;` |
|     23734 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     23734 |  380 | `	return SXRET_OK;` |
|     11868 |  381 |  |
|   3065752 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   3065754 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   3065754 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   3065754 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1985006 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    992518 |  389 | `	}` |
|   3065754 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   3065754 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   3065754 |  393 | `	if( pHash->nEntry == 0 ){` |
|    137170 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     68584 |  395 | `	}` |
|   3065754 |  396 | `	pHash->nEntry++;` |
|   3065754 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   3065752 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   3065754 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     23734 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     23734 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     11866 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   3065754 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   3065754 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   3065754 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   3065754 |  421 | `	pEntry->pHash = pHash;` |
|   3065754 |  422 | `	pEntry->pKey = pKey;` |
|   3065754 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   3065754 |  424 | `	pEntry->pUserData = pUserData;` |
|   3065754 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   3065754 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   3065754 |  428 | `	return rc;` |
|   1532878 |  429 |  |
|    121998 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    122000 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
