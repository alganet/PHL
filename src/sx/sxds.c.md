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
|  162189704 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  162189709 |   16 | `	pSet->nSize = 0 ;` |
|  162189709 |   17 | `	pSet->nUsed = 0;` |
|  162189709 |   18 | `	pSet->nCursor = 0;` |
|  162189709 |   19 | `	pSet->eSize = ElemSize;` |
|  162189709 |   20 | `	pSet->pAllocator = pAllocator;` |
|  162189709 |   21 | `	pSet->pBase =  0;` |
|  162189709 |   22 | `	pSet->pUserData = 0;` |
|  162189709 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  369257462 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  369257467 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   21543317 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   21543317 |   33 | `		if( pSet->nSize <= 0 ){` |
|   18380675 |   34 | `			pSet->nSize = 4;` |
|    9190335 |   35 | `		}` |
|   21543317 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   21543317 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   21543317 |   40 | `		pSet->pBase = pNew;` |
|   21543317 |   41 | `		pSet->nSize <<= 1;` |
|   10771656 |   42 | `	}` |
|  369257467 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2912828183 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  369257467 |   45 | `	pSet->nUsed++;` |
|  369257467 |   46 | `	return SXRET_OK;` |
|  184628758 |   47 | `}` |
|   18318812 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   18318817 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   18318817 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   18318817 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   18318817 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   18318817 |   60 | `	pSet->nSize = nItem;` |
|   18318817 |   61 | `	return SXRET_OK;` |
|    9159411 |   62 | `}` |
|   27249913 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   27249918 |   65 | `	pSet->nUsed   = 0;` |
|   27249918 |   66 | `	pSet->nCursor = 0;` |
|   27249918 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      70488 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      70493 |   71 | `	pSet->nCursor = 0;` |
|      70493 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      74670 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      74675 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30491 |   79 | `		pSet->nCursor = 0;` |
|      30491 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      44189 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      44189 |   83 | `	if( ppEntry ){` |
|      44189 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      22092 |   85 | `	}` |
|      44189 |   86 | `	pSet->nCursor++;` |
|      44189 |   87 | `	return SXRET_OK;` |
|      37340 |   88 | `}` |
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
|   56324658 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   56324663 |  109 | `	sxi32 rc = SXRET_OK;` |
|   56324663 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   30675525 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   15337760 |  112 | `	}` |
|   56324663 |  113 | `	pSet->pBase = 0;` |
|   56324663 |  114 | `	pSet->nUsed = 0;` |
|   56324663 |  115 | `	pSet->nCursor = 0;` |
|   56324663 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   65560180 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   65560185 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19237 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   65540953 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   65540953 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   32780095 |  126 | `}` |
|    9411730 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9411735 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2232945 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7178795 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7178795 |  135 | `	pSet->nUsed--;` |
|    7178795 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7178795 |  137 | `	return pData;` |
|    4705870 |  138 | `}` |
|   34437776 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   34437781 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   34437729 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   34437729 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   17219053 |  148 | `}` |
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
|    1772218 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1772223 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1772223 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1772223 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1772223 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1772223 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1772223 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1772223 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1772223 |  180 | `	pHash->nEntry = 0;` |
|    1772223 |  181 | `	pHash->apBucket = apNew;` |
|    1772223 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1772223 |  183 | `	return SXRET_OK;` |
|     886114 |  184 | `}` |
|     413286 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     413291 |  193 | `	pEntry = pHash->pList;` |
|     221166 |  194 | `	for(;;){` |
|     442337 |  195 | `		if( pHash->nEntry == 0 ){` |
|     413291 |  196 | `			break;` |
|          - |  197 | `		}` |
|      29051 |  198 | `		pNext = pEntry->pNext;` |
|      29051 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      29051 |  200 | `		pEntry = pNext;` |
|      29051 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     413291 |  203 | `	if( pHash->apBucket ){` |
|     413291 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     206643 |  205 | `	}` |
|     413291 |  206 | `	pHash->apBucket = 0;` |
|     413291 |  207 | `	pHash->nBucketSize = 0;` |
|     413291 |  208 | `	pHash->pAllocator = 0;` |
|     413291 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   67376079 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   67376084 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   67376084 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   60955896 |  218 | `	for(;;){` |
|  122180185 |  219 | `		if( pEntry == 0 ){` |
|   24356177 |  220 | `			break;` |
|          - |  221 | `		}` |
|  119335842 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   43023935 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   43019912 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   54804106 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   24356177 |  229 | `	return 0;` |
|   33688323 |  230 | `}` |
|   74390017 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   74390022 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    7014375 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   67375652 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   67375652 |  244 | `	if( pEntry == 0 ){` |
|   24356159 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   43019498 |  247 | `	return (SyHashEntry *)pEntry;` |
|   37195292 |  248 | `}` |
|     437732 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     437737 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     357205 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     178605 |  254 | `	}else{` |
|      80537 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     437737 |  257 | `	if( pEntry->pNextCollide ){` |
|       4446 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2222 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     437737 |  261 | `	if( pHash->pLast == pEntry ){` |
|     427747 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     213871 |  263 | `	}` |
|     437737 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     437737 |  265 | `	pHash->nEntry--;` |
|     437737 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     437737 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     437737 |  272 | `	return rc;` |
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
|     437318 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     437323 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     437323 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     437323 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2842272 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2842277 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2842277 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   21263138 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   21263143 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2842011 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2842011 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18421137 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18421137 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18421137 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10631574 |  329 | `}` |
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
|       4105 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4091 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4091 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4091 |  348 | `		pEntry = pEntry->pNext;` |
|       2046 |  349 | `	}` |
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
|    6864005 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3431747 |  375 | `		}` |
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
|   18441368 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   18441373 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   18441373 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   18441373 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11688954 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5844498 |  393 | `	}` |
|   18441373 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   18441373 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   18441321 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   18441373 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     977281 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     977281 |  408 | `		pHash->pLast = pEntry;` |
|     488638 |  409 | `	}` |
|   18441373 |  410 | `	pHash->nEntry++;` |
|   18441373 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   18441368 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   18441373 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      91985 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      91985 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45990 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   18441373 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   18441373 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   18441373 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   18441373 |  435 | `	pEntry->pHash = pHash;` |
|   18441373 |  436 | `	pEntry->pKey = pKey;` |
|   18441373 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   18441373 |  438 | `	pEntry->pUserData = pUserData;` |
|   18441373 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   18441373 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   18441373 |  442 | `	return rc;` |
|    9220689 |  443 | `}` |
|   18441234 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   18441239 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     476992 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     476997 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
