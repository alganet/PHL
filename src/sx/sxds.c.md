# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 286/304 lines (94.08%)

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
|  184234000 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  184234005 |   16 | `	pSet->nSize = 0 ;` |
|  184234005 |   17 | `	pSet->nUsed = 0;` |
|  184234005 |   18 | `	pSet->nCursor = 0;` |
|  184234005 |   19 | `	pSet->eSize = ElemSize;` |
|  184234005 |   20 | `	pSet->pAllocator = pAllocator;` |
|  184234005 |   21 | `	pSet->pBase =  0;` |
|  184234005 |   22 | `	pSet->pUserData = 0;` |
|  184234005 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  417507667 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  417507672 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   24111828 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   24111828 |   33 | `		if( pSet->nSize <= 0 ){` |
|   20550046 |   34 | `			pSet->nSize = 4;` |
|   10275947 |   35 | `		}` |
|   24111828 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   24111828 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   24111828 |   40 | `		pSet->pBase = pNew;` |
|   24111828 |   41 | `		pSet->nSize <<= 1;` |
|   12056838 |   42 | `	}` |
|  417507672 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3296486374 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  417507672 |   45 | `	pSet->nUsed++;` |
|  417507672 |   46 | `	return SXRET_OK;` |
|  208756983 |   47 | `}` |
|   20756874 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   20756879 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   20756879 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   20756879 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   20756879 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   20756879 |   60 | `	pSet->nSize = nItem;` |
|   20756879 |   61 | `	return SXRET_OK;` |
|   10378442 |   62 | `}` |
|   30720662 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   30720667 |   65 | `	pSet->nUsed   = 0;` |
|   30720667 |   66 | `	pSet->nCursor = 0;` |
|   30720667 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69118 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69123 |   71 | `	pSet->nCursor = 0;` |
|      69123 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      69362 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      69367 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30045 |   79 | `		pSet->nCursor = 0;` |
|      30045 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      39327 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      39327 |   83 | `	if( ppEntry ){` |
|      39327 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      19661 |   85 | `	}` |
|      39327 |   86 | `	pSet->nCursor++;` |
|      39327 |   87 | `	return SXRET_OK;` |
|      34686 |   88 | `}` |
|          - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        ! 0 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|        ! 0 |   91 | `{` |
|          - |   92 | `	register unsigned char *zSrc;` |
|        ! 0 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|        ! 0 |   94 | `		return 0;` |
|          - |   95 | `	}` |
|        ! 0 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|        ! 0 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|        ! 0 |   98 | `}` |
|          - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    3279774 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3279779 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1213 |  103 | `		pSet->nUsed = nNewSize;` |
|        604 |  104 | `	}` |
|    3279779 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   63007464 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   63007469 |  109 | `	sxi32 rc = SXRET_OK;` |
|   63007469 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   34113888 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   17057868 |  112 | `	}` |
|   63007469 |  113 | `	pSet->pBase = 0;` |
|   63007469 |  114 | `	pSet->nUsed = 0;` |
|   63007469 |  115 | `	pSet->nCursor = 0;` |
|   63007469 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   74223028 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   74223033 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       4003 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   74219035 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   74219035 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   37111519 |  126 | `}` |
|    9840615 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9840620 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2237655 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7602970 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7602970 |  135 | `	pSet->nUsed--;` |
|    7602970 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7602970 |  137 | `	return pData;` |
|    4920930 |  138 | `}` |
|   37022698 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   37022703 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   37022651 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   37022651 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   18515471 |  148 | `}` |
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
|    2155580 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    2155585 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2155585 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    2155585 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2155585 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    2155585 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    2155585 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    2155585 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    2155585 |  180 | `	pHash->nEntry = 0;` |
|    2155585 |  181 | `	pHash->apBucket = apNew;` |
|    2155585 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    2155585 |  183 | `	return SXRET_OK;` |
|    1077898 |  184 | `}` |
|     520158 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     520163 |  193 | `	pEntry = pHash->pList;` |
|     275467 |  194 | `	for(;;){` |
|     550733 |  195 | `		if( pHash->nEntry == 0 ){` |
|     520163 |  196 | `			break;` |
|          - |  197 | `		}` |
|      30575 |  198 | `		pNext = pEntry->pNext;` |
|      30575 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      30575 |  200 | `		pEntry = pNext;` |
|      30575 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     520163 |  203 | `	if( pHash->apBucket ){` |
|     520163 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     260182 |  205 | `	}` |
|     520163 |  206 | `	pHash->apBucket = 0;` |
|     520163 |  207 | `	pHash->nBucketSize = 0;` |
|     520163 |  208 | `	pHash->pAllocator = 0;` |
|     520163 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   74840002 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   74840007 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   74840007 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   67850346 |  218 | `	for(;;){` |
|  135452346 |  219 | `		if( pEntry == 0 ){` |
|   27177689 |  220 | `			break;` |
|          - |  221 | `		}` |
|  132105284 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   47666318 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   47662323 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   60612344 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   27177689 |  229 | `	return 0;` |
|   37426120 |  230 | `}` |
|   83765958 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   83765963 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    8926419 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   74839549 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   74839549 |  244 | `	if( pEntry == 0 ){` |
|   27177671 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   47661883 |  247 | `	return (SyHashEntry *)pEntry;` |
|   41889201 |  248 | `}` |
|     496902 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     496907 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     407941 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     204488 |  254 | `	}else{` |
|      88971 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     496907 |  257 | `	if( pEntry->pNextCollide ){` |
|       4314 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2152 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     496907 |  261 | `	if( pHash->pLast == pEntry ){` |
|     487321 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     244276 |  263 | `	}` |
|     496907 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     496907 |  265 | `	pHash->nEntry--;` |
|     496907 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     496907 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     496907 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        458 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        463 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        463 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        445 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        445 |  288 | `	return rc;` |
|        234 |  289 | `}` |
|     496462 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     496467 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     496467 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     496467 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3366548 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3366553 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3366553 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   26022390 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   26022395 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3366287 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3366287 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   22656113 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   22656113 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   22656113 |  328 | `	return (SyHashEntry *)pEntry;` |
|   13011200 |  329 | `}` |
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
|       4833 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4819 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4819 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4819 |  348 | `		pEntry = pEntry->pNext;` |
|       2410 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|     100572 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100577 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100577 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100577 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100577 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18623585 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18523013 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18523013 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18523013 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18523013 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8943747 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4471501 |  375 | `		}` |
|   18523013 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18523013 |  378 | `		pEntry = pEntry->pNext;` |
|    9261509 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100577 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100577 |  382 | `	pHash->apBucket = apNew;` |
|     100577 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100577 |  384 | `	return SXRET_OK;` |
|      50291 |  385 | `}` |
|   22416948 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   22416953 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   22416953 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   22416953 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   14146934 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    7073640 |  393 | `	}` |
|   22416953 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   22416953 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|     879491 |  399 | `		pHash->pLast->pNext = pEntry;` |
|     879491 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|     879491 |  401 | `		pHash->pLast = pEntry;` |
|     439748 |  402 | `	}else{` |
|   21537467 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   22416953 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1192173 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1192173 |  408 | `		pHash->pLast = pEntry;` |
|     596187 |  409 | `	}` |
|   22416953 |  410 | `	pHash->nEntry++;` |
|   22416953 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   22416948 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   22416953 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100577 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100577 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50286 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   22416953 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   22416953 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   22416953 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   22416953 |  435 | `	pEntry->pHash = pHash;` |
|   22416953 |  436 | `	pEntry->pKey = pKey;` |
|   22416953 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   22416953 |  438 | `	pEntry->pUserData = pUserData;` |
|   22416953 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   22416953 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   22416953 |  442 | `	return rc;` |
|   11209097 |  443 | `}` |
|   21276336 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   21276341 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|    1140612 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  455 | `{` |
|    1140617 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          5 |  457 | `}` |
|     536872 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     536877 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
