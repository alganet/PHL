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
|  12852470 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  12852472 |   16 | `	pSet->nSize = 0 ;` |
|  12852472 |   17 | `	pSet->nUsed = 0;` |
|  12852472 |   18 | `	pSet->nCursor = 0;` |
|  12852472 |   19 | `	pSet->eSize = ElemSize;` |
|  12852472 |   20 | `	pSet->pAllocator = pAllocator;` |
|  12852472 |   21 | `	pSet->pBase =  0;` |
|  12852472 |   22 | `	pSet->pUserData = 0;` |
|  12852472 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  21212874 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  21212876 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3703412 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3703412 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3599830 |   34 | `			pSet->nSize = 4;` |
|   1799914 |   35 | `		}` |
|   3703412 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3703412 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3703412 |   40 | `		pSet->pBase = pNew;` |
|   3703412 |   41 | `		pSet->nSize <<= 1;` |
|   1851705 |   42 | `	}` |
|  21212876 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 157626436 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  21212876 |   45 | `	pSet->nUsed++;` |
|  21212876 |   46 | `	return SXRET_OK;` |
|  10606461 |   47 |  |
|    707924 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    707926 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    707926 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    707926 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    707926 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    707926 |   60 | `	pSet->nSize = nItem;` |
|    707926 |   61 | `	return SXRET_OK;` |
|    353964 |   62 |  |
|   1173694 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1173696 |   65 | `	pSet->nUsed   = 0;` |
|   1173696 |   66 | `	pSet->nCursor = 0;` |
|   1173696 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     41686 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     41688 |   71 | `	pSet->nCursor = 0;` |
|     41688 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     45568 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     45570 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     17102 |   79 | `		pSet->nCursor = 0;` |
|     17102 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     28470 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     28470 |   83 | `	if( ppEntry ){` |
|     28470 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     14234 |   85 | `	}` |
|     28470 |   86 | `	pSet->nCursor++;` |
|     28470 |   87 | `	return SXRET_OK;` |
|     22786 |   88 |  |
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
|     87588 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     87590 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     87590 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   7831328 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   7831330 |  109 | `	sxi32 rc = SXRET_OK;` |
|   7831330 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4048976 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2024487 |  112 | `	}` |
|   7831330 |  113 | `	pSet->pBase = 0;` |
|   7831330 |  114 | `	pSet->nUsed = 0;` |
|   7831330 |  115 | `	pSet->nCursor = 0;` |
|   7831330 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4238478 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4238480 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4238390 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4238390 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2119241 |  126 |  |
|   3195510 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3195512 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2140754 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1054760 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1054760 |  135 | `	pSet->nUsed--;` |
|   1054760 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1054760 |  137 | `	return pData;` |
|   1597757 |  138 |  |
|   9875132 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   9875134 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   9875134 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   9875134 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4937798 |  148 |  |
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
|    150012 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    150014 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    150014 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    150014 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    150014 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    150014 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    150014 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    150014 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    150014 |  180 | `	pHash->nEntry = 0;` |
|    150014 |  181 | `	pHash->apBucket = apNew;` |
|    150014 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    150014 |  183 | `	return SXRET_OK;` |
|     75008 |  184 |  |
|     28430 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     28432 |  193 | `	pEntry = pHash->pList;` |
|     15916 |  194 | `	for(;;){` |
|     31834 |  195 | `		if( pHash->nEntry == 0 ){` |
|     28432 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3404 |  198 | `		pNext = pEntry->pNext;` |
|      3404 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3404 |  200 | `		pEntry = pNext;` |
|      3404 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     28432 |  203 | `	if( pHash->apBucket ){` |
|     28432 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     14215 |  205 | `	}` |
|     28432 |  206 | `	pHash->apBucket = 0;` |
|     28432 |  207 | `	pHash->nBucketSize = 0;` |
|     28432 |  208 | `	pHash->pAllocator = 0;` |
|     28432 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  10539972 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  10539974 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  10539974 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   9211305 |  218 | `	for(;;){` |
|  18497110 |  219 | `		if( pEntry == 0 ){` |
|   5796716 |  220 | `			break;` |
|         - |  221 | `		}` |
|  15071895 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   4743262 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   4743260 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   7957138 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   5796716 |  229 | `	return 0;` |
|   5270252 |  230 |  |
|  10628386 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  10628388 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     88424 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  10539966 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  10539966 |  244 | `	if( pEntry == 0 ){` |
|   5796716 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   4743252 |  247 | `	return (SyHashEntry *)pEntry;` |
|   5314459 |  248 |  |
|     81096 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     81098 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     61560 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     30781 |  254 | `	}else{` |
|     19540 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     81098 |  257 | `	if( pEntry->pNextCollide ){` |
|      4133 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2066 |  259 | `	}` |
|     81098 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     81098 |  261 | `	pHash->nEntry--;` |
|     81098 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     81098 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     81098 |  268 | `	return rc;` |
|         2 |  269 |  |
|         8 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|        10 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        10 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|        10 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        10 |  284 | `	return rc;` |
|         6 |  285 |  |
|     81088 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     81090 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     81090 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     81090 |  296 | `	return rc;` |
|         2 |  297 |  |
|    203244 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    203246 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    203246 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   1500666 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   1500668 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    202812 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    202812 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   1297858 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   1297858 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   1297858 |  324 | `	return (SyHashEntry *)pEntry;` |
|    750335 |  325 |  |
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
|      1619 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1609 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1609 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1609 |  344 | `		pEntry = pEntry->pNext;` |
|       805 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     18608 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     18610 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     18610 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     18610 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     18610 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2563474 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2544866 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2544866 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2544866 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2544866 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1221941 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    610968 |  371 | `		}` |
|   2544866 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2544866 |  374 | `		pEntry = pEntry->pNext;` |
|   1272434 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     18610 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     18610 |  378 | `	pHash->apBucket = apNew;` |
|     18610 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     18610 |  380 | `	return SXRET_OK;` |
|      9306 |  381 |  |
|   2320354 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2320356 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2320356 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2320356 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1550652 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    775336 |  389 | `	}` |
|   2320356 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2320356 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2320356 |  393 | `	if( pHash->nEntry == 0 ){` |
|     92592 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     46295 |  395 | `	}` |
|   2320356 |  396 | `	pHash->nEntry++;` |
|   2320356 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2320354 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2320356 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     18610 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     18610 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      9304 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2320356 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2320356 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2320356 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2320356 |  421 | `	pEntry->pHash = pHash;` |
|   2320356 |  422 | `	pEntry->pKey = pKey;` |
|   2320356 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2320356 |  424 | `	pEntry->pUserData = pUserData;` |
|   2320356 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2320356 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2320356 |  428 | `	return rc;` |
|   1160179 |  429 |  |
|    104822 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    104824 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
