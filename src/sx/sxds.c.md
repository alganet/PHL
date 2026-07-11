# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 289/304 lines (95.07%)

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
|  20736838 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 | `{` |
|  20736843 |   16 | `	pSet->nSize = 0 ;` |
|  20736843 |   17 | `	pSet->nUsed = 0;` |
|  20736843 |   18 | `	pSet->nCursor = 0;` |
|  20736843 |   19 | `	pSet->eSize = ElemSize;` |
|  20736843 |   20 | `	pSet->pAllocator = pAllocator;` |
|  20736843 |   21 | `	pSet->pBase =  0;` |
|  20736843 |   22 | `	pSet->pUserData = 0;` |
|  20736843 |   23 | `	return SXRET_OK;` |
|         5 |   24 | `}` |
|  34363791 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 | `{` |
|         - |   27 | `	unsigned char *zbase;` |
|  34363796 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4858649 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4858649 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4687821 |   34 | `			pSet->nSize = 4;` |
|   2343908 |   35 | `		}` |
|   4858649 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4858649 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4858649 |   40 | `		pSet->pBase = pNew;` |
|   4858649 |   41 | `		pSet->nSize <<= 1;` |
|   2429322 |   42 | `	}` |
|  34363796 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 256897934 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  34363796 |   45 | `	pSet->nUsed++;` |
|  34363796 |   46 | `	return SXRET_OK;` |
|  17181943 |   47 | `}` |
|   1429752 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 | `{` |
|   1429757 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1429757 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1429757 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1429757 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1429757 |   60 | `	pSet->nSize = nItem;` |
|   1429757 |   61 | `	return SXRET_OK;` |
|    714881 |   62 | `}` |
|   2300251 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 | `{` |
|   2300256 |   65 | `	pSet->nUsed   = 0;` |
|   2300256 |   66 | `	pSet->nCursor = 0;` |
|   2300256 |   67 | `	return SXRET_OK;` |
|         5 |   68 | `}` |
|     66464 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 | `{` |
|     66469 |   71 | `	pSet->nCursor = 0;` |
|     66469 |   72 | `	return SXRET_OK;` |
|         5 |   73 | `}` |
|     70586 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 | `{` |
|         - |   76 | `	register unsigned char *zSrc;` |
|     70591 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     28585 |   79 | `		pSet->nCursor = 0;` |
|     28585 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     42011 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     42011 |   83 | `	if( ppEntry ){` |
|     42011 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     21003 |   85 | `	}` |
|     42011 |   86 | `	pSet->nCursor++;` |
|     42011 |   87 | `	return SXRET_OK;` |
|     35298 |   88 | `}` |
|         - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         8 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|         1 |   91 | `{` |
|         - |   92 | `	register unsigned char *zSrc;` |
|         9 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         3 |   94 | `		return 0;` |
|         - |   95 | `	}` |
|         7 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|         7 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|         5 |   98 | `}` |
|         - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    240824 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 | `{` |
|    240829 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       677 |  103 | `		pSet->nUsed = nNewSize;` |
|       336 |  104 | `	}` |
|    240829 |  105 | `	return SXRET_OK;` |
|         5 |  106 | `}` |
|  10593386 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 | `{` |
|  10593391 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10593391 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5318485 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2659240 |  112 | `	}` |
|  10593391 |  113 | `	pSet->pBase = 0;` |
|  10593391 |  114 | `	pSet->nUsed = 0;` |
|  10593391 |  115 | `	pSet->nCursor = 0;` |
|  10593391 |  116 | `	return rc;` |
|         5 |  117 | `}` |
|   6150746 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 | `{` |
|         - |  120 | `	const char *zBase;` |
|   6150751 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       133 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   6150623 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   6150623 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   3075378 |  126 | `}` |
|   3725214 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 | `{` |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3725219 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2193095 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1532129 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1532129 |  135 | `	pSet->nUsed--;` |
|   1532129 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1532129 |  137 | `	return pData;` |
|   1862612 |  138 | `}` |
|  14125077 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	const char *zBase;` |
|  14125082 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  14125082 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  14125082 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   7062911 |  148 | `}` |
|         - |  149 | `/* Private hash entry */` |
|         - |  150 | `struct SyHashEntry_Pr` |
|         - |  151 | `{` |
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
|    681440 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 | `{` |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    681445 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    681445 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    681445 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    681445 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    681445 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    681445 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    681445 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    681445 |  180 | `	pHash->nEntry = 0;` |
|    681445 |  181 | `	pHash->apBucket = apNew;` |
|    681445 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    681445 |  183 | `	return SXRET_OK;` |
|    340725 |  184 | `}` |
|    154322 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 | `{` |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    154327 |  193 | `	pEntry = pHash->pList;` |
|     82557 |  194 | `	for(;;){` |
|    165119 |  195 | `		if( pHash->nEntry == 0 ){` |
|    154327 |  196 | `			break;` |
|         - |  197 | `		}` |
|     10797 |  198 | `		pNext = pEntry->pNext;` |
|     10797 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     10797 |  200 | `		pEntry = pNext;` |
|     10797 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    154327 |  203 | `	if( pHash->apBucket ){` |
|    154327 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     77161 |  205 | `	}` |
|    154327 |  206 | `	pHash->apBucket = 0;` |
|    154327 |  207 | `	pHash->nBucketSize = 0;` |
|    154327 |  208 | `	pHash->pAllocator = 0;` |
|    154327 |  209 | `	return SXRET_OK;` |
|         5 |  210 | `}` |
|  19957962 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 | `{` |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  19957967 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  19957967 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  18088581 |  218 | `	for(;;){` |
|  36080021 |  219 | `		if( pEntry == 0 ){` |
|  10157499 |  220 | `			break;` |
|         - |  221 | `		}` |
|  30822512 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   9800480 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   9800473 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  16122059 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|  10157499 |  229 | `	return 0;` |
|   9979496 |  230 | `}` |
|  20929340 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 | `{` |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  20929345 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    971607 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  19957743 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  19957743 |  244 | `	if( pEntry == 0 ){` |
|  10157499 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   9800249 |  247 | `	return (SyHashEntry *)pEntry;` |
|  10465185 |  248 | `}` |
|    177532 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 | `{` |
|         - |  251 | `	sxi32 rc;` |
|    177537 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    138991 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     69498 |  254 | `	}else{` |
|     38551 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    177537 |  257 | `	if( pEntry->pNextCollide ){` |
|      5142 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2570 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    177537 |  261 | `	if( pHash->pLast == pEntry ){` |
|    171137 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     85566 |  263 | `	}` |
|    177537 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    177537 |  265 | `	pHash->nEntry--;` |
|    177537 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    177537 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    177537 |  272 | `	return rc;` |
|         5 |  273 | `}` |
|       224 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 | `{` |
|         - |  276 | `	SyHashEntry_Pr *pEntry;` |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( INVALID_HASH(pHash) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|       229 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       229 |  284 | `	if( pEntry == 0 ){` |
|       ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|         - |  286 | `	}` |
|       229 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       229 |  288 | `	return rc;` |
|       117 |  289 | `}` |
|    177308 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 | `{` |
|    177313 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    177313 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    177313 |  300 | `	return rc;` |
|         5 |  301 | `}` |
|   1330544 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 | `{` |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1330549 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1330549 |  310 | `	return SXRET_OK;` |
|         5 |  311 | `}` |
|   8352928 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 | `{` |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   8352933 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1330287 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1330287 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   7022651 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   7022651 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   7022651 |  328 | `	return (SyHashEntry *)pEntry;` |
|   4176469 |  329 | `}` |
|        10 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         1 |  331 | `{` |
|         - |  332 | `	SyHashEntry_Pr *pEntry;` |
|         - |  333 | `	sxi32 rc;` |
|         - |  334 | `	sxu32 n;` |
|         - |  335 | `#if defined(UNTRUST)` |
|         - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  337 | `		return 0;` |
|         - |  338 | `	}` |
|         - |  339 | `#endif` |
|        11 |  340 | `	pEntry = pHash->pList;` |
|      2083 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|      2073 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      2073 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|      2073 |  348 | `		pEntry = pEntry->pNext;` |
|      1037 |  349 | `	}` |
|        11 |  350 | `	return SXRET_OK;` |
|         6 |  351 | `}` |
|     33054 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  353 | `{` |
|     33059 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  355 | `	SyHashEntry_Pr *pEntry;` |
|         - |  356 | `	SyHashEntry_Pr **apNew;` |
|         - |  357 | `	sxu32 n,iBucket;` |
|         - |  358 |  |
|         - |  359 | `	/* Allocate a new larger table */` |
|     33059 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     33059 |  361 | `	if( apNew == 0 ){` |
|         - |  362 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  363 | `		return SXRET_OK;` |
|         - |  364 | `	}` |
|         - |  365 | `	/* Zero the new table */` |
|     33059 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  367 | `	/* Rehash all entries */` |
|   4166723 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   4133669 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  370 | `		/* Install in the new bucket */` |
|   4133669 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   4133669 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   4133669 |  373 | `		if( apNew[iBucket] != 0 ){` |
|   1984102 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    992076 |  375 | `		}` |
|   4133669 |  376 | `		apNew[iBucket] = pEntry;` |
|         - |  377 | `		/* Point to the next entry */` |
|   4133669 |  378 | `		pEntry = pEntry->pNext;` |
|   2066837 |  379 | `	}` |
|         - |  380 | `	/* Release the old table and reflect the change */` |
|     33059 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     33059 |  382 | `	pHash->apBucket = apNew;` |
|     33059 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     33059 |  384 | `	return SXRET_OK;` |
|     16532 |  385 | `}` |
|   5585860 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 | `{` |
|   5585865 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   5585865 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   5585865 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   3123244 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1561627 |  393 | `	}` |
|   5585865 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   5585865 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        51 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        51 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        51 |  401 | `		pHash->pLast = pEntry;` |
|        26 |  402 | `	}else{` |
|   5585815 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   5585865 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    361003 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    361003 |  408 | `		pHash->pLast = pEntry;` |
|    180499 |  409 | `	}` |
|   5585865 |  410 | `	pHash->nEntry++;` |
|   5585865 |  411 | `	return SXRET_OK;` |
|         5 |  412 | `}` |
|   5585860 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 | `{` |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   5585865 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     33059 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     33059 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     16527 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   5585865 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   5585865 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   5585865 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   5585865 |  435 | `	pEntry->pHash = pHash;` |
|   5585865 |  436 | `	pEntry->pKey = pKey;` |
|   5585865 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   5585865 |  438 | `	pEntry->pUserData = pUserData;` |
|   5585865 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   5585865 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   5585865 |  442 | `	return rc;` |
|   2792935 |  443 | `}` |
|   5585744 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 | `{` |
|   5585749 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|         5 |  447 | `}` |
|         - |  448 | `/*` |
|         - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|         - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|         - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|         - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|         - |  453 | ` */` |
|       116 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  455 | `{` |
|       118 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|         2 |  457 | `}` |
|    217368 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 | `{` |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    217373 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 | `}` |
|         - |  468 |  |
