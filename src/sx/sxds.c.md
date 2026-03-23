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
|  11825512 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  11825514 |   16 | `	pSet->nSize = 0 ;` |
|  11825514 |   17 | `	pSet->nUsed = 0;` |
|  11825514 |   18 | `	pSet->nCursor = 0;` |
|  11825514 |   19 | `	pSet->eSize = ElemSize;` |
|  11825514 |   20 | `	pSet->pAllocator = pAllocator;` |
|  11825514 |   21 | `	pSet->pBase =  0;` |
|  11825514 |   22 | `	pSet->pUserData = 0;` |
|  11825514 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  19214362 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  19214364 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3571288 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3571288 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3483372 |   34 | `			pSet->nSize = 4;` |
|   1741685 |   35 | `		}` |
|   3571288 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3571288 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3571288 |   40 | `		pSet->pBase = pNew;` |
|   3571288 |   41 | `		pSet->nSize <<= 1;` |
|   1785643 |   42 | `	}` |
|  19214364 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 142936548 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  19214364 |   45 | `	pSet->nUsed++;` |
|  19214364 |   46 | `	return SXRET_OK;` |
|   9607205 |   47 |  |
|    599350 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    599352 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    599352 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    599352 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    599352 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    599352 |   60 | `	pSet->nSize = nItem;` |
|    599352 |   61 | `	return SXRET_OK;` |
|    299677 |   62 |  |
|   1080714 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1080716 |   65 | `	pSet->nUsed   = 0;` |
|   1080716 |   66 | `	pSet->nCursor = 0;` |
|   1080716 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     39864 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     39866 |   71 | `	pSet->nCursor = 0;` |
|     39866 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     43736 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     43738 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     16198 |   79 | `		pSet->nCursor = 0;` |
|     16198 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     27542 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     27542 |   83 | `	if( ppEntry ){` |
|     27542 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     13770 |   85 | `	}` |
|     27542 |   86 | `	pSet->nCursor++;` |
|     27542 |   87 | `	return SXRET_OK;` |
|     21870 |   88 |  |
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
|     73162 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     73164 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     73164 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   7469748 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   7469750 |  109 | `	sxi32 rc = SXRET_OK;` |
|   7469750 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3866668 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1933333 |  112 | `	}` |
|   7469750 |  113 | `	pSet->pBase = 0;` |
|   7469750 |  114 | `	pSet->nUsed = 0;` |
|   7469750 |  115 | `	pSet->nCursor = 0;` |
|   7469750 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3811016 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3811018 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3810928 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3810928 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1905510 |  126 |  |
|   3137912 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3137914 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2134132 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1003784 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1003784 |  135 | `	pSet->nUsed--;` |
|   1003784 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1003784 |  137 | `	return pData;` |
|   1568958 |  138 |  |
|   9855153 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   9855155 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   9855155 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   9855155 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4927814 |  148 |  |
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
|    102756 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    102758 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    102758 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    102758 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    102758 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    102758 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    102758 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    102758 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    102758 |  180 | `	pHash->nEntry = 0;` |
|    102758 |  181 | `	pHash->apBucket = apNew;` |
|    102758 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    102758 |  183 | `	return SXRET_OK;` |
|     51380 |  184 |  |
|     12140 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     12142 |  193 | `	pEntry = pHash->pList;` |
|      7590 |  194 | `	for(;;){` |
|     15182 |  195 | `		if( pHash->nEntry == 0 ){` |
|     12142 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3042 |  198 | `		pNext = pEntry->pNext;` |
|      3042 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3042 |  200 | `		pEntry = pNext;` |
|      3042 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     12142 |  203 | `	if( pHash->apBucket ){` |
|     12142 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      6070 |  205 | `	}` |
|     12142 |  206 | `	pHash->apBucket = 0;` |
|     12142 |  207 | `	pHash->nBucketSize = 0;` |
|     12142 |  208 | `	pHash->pAllocator = 0;` |
|     12142 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  10104150 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  10104152 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  10104152 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   8686907 |  218 | `	for(;;){` |
|  17413787 |  219 | `		if( pEntry == 0 ){` |
|   5497726 |  220 | `			break;` |
|         - |  221 | `		}` |
|  14219146 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   4606430 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   4606428 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   7309637 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   5497726 |  229 | `	return 0;` |
|   5052341 |  230 |  |
|  10161788 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  10161790 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     57646 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  10104146 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  10104146 |  244 | `	if( pEntry == 0 ){` |
|   5497726 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   4606422 |  247 | `	return (SyHashEntry *)pEntry;` |
|   5081160 |  248 |  |
|     75594 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     75596 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     57042 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     28522 |  254 | `	}else{` |
|     18556 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     75596 |  257 | `	if( pEntry->pNextCollide ){` |
|      4133 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2066 |  259 | `	}` |
|     75596 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     75596 |  261 | `	pHash->nEntry--;` |
|     75596 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     75596 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     75596 |  268 | `	return rc;` |
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
|     75588 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     75590 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     75590 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     75590 |  296 | `	return rc;` |
|         2 |  297 |  |
|    144006 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    144008 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    144008 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   1001960 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   1001962 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    143574 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    143574 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    858390 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    858390 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    858390 |  324 | `	return (SyHashEntry *)pEntry;` |
|    500982 |  325 |  |
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
|     15432 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     15434 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     15434 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     15434 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     15434 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2124170 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2108738 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2108738 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2108738 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2108738 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1012538 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    506261 |  371 | `		}` |
|   2108738 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2108738 |  374 | `		pEntry = pEntry->pNext;` |
|   1054370 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     15434 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     15434 |  378 | `	pHash->apBucket = apNew;` |
|     15434 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     15434 |  380 | `	return SXRET_OK;` |
|      7718 |  381 |  |
|   1897742 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1897744 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1897744 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1897744 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1285594 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    642793 |  389 | `	}` |
|   1897744 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1897744 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1897744 |  393 | `	if( pHash->nEntry == 0 ){` |
|     73846 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     36922 |  395 | `	}` |
|   1897744 |  396 | `	pHash->nEntry++;` |
|   1897744 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1897742 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1897744 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     15434 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     15434 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      7716 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1897744 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1897744 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1897744 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1897744 |  421 | `	pEntry->pHash = pHash;` |
|   1897744 |  422 | `	pEntry->pKey = pKey;` |
|   1897744 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1897744 |  424 | `	pEntry->pUserData = pUserData;` |
|   1897744 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1897744 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1897744 |  428 | `	return rc;` |
|    948873 |  429 |  |
|     95206 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     95208 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
