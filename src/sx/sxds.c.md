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
|  167849820 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  167849825 |   16 | `	pSet->nSize = 0 ;` |
|  167849825 |   17 | `	pSet->nUsed = 0;` |
|  167849825 |   18 | `	pSet->nCursor = 0;` |
|  167849825 |   19 | `	pSet->eSize = ElemSize;` |
|  167849825 |   20 | `	pSet->pAllocator = pAllocator;` |
|  167849825 |   21 | `	pSet->pBase =  0;` |
|  167849825 |   22 | `	pSet->pUserData = 0;` |
|  167849825 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  381212437 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  381212442 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   22001913 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   22001913 |   33 | `		if( pSet->nSize <= 0 ){` |
|   18757029 |   34 | `			pSet->nSize = 4;` |
|    9378512 |   35 | `		}` |
|   22001913 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   22001913 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   22001913 |   40 | `		pSet->pBase = pNew;` |
|   22001913 |   41 | `		pSet->nSize <<= 1;` |
|   11000954 |   42 | `	}` |
|  381212442 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3007029156 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  381212442 |   45 | `	pSet->nUsed++;` |
|  381212442 |   46 | `	return SXRET_OK;` |
|  190606247 |   47 | `}` |
|   18956574 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   18956579 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   18956579 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   18956579 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   18956579 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   18956579 |   60 | `	pSet->nSize = nItem;` |
|   18956579 |   61 | `	return SXRET_OK;` |
|    9478292 |   62 | `}` |
|   28210809 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   28210814 |   65 | `	pSet->nUsed   = 0;` |
|   28210814 |   66 | `	pSet->nCursor = 0;` |
|   28210814 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      70668 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      70673 |   71 | `	pSet->nCursor = 0;` |
|      70673 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      74854 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      74859 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30571 |   79 | `		pSet->nCursor = 0;` |
|      30571 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      44293 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      44293 |   83 | `	if( ppEntry ){` |
|      44293 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      22144 |   85 | `	}` |
|      44293 |   86 | `	pSet->nCursor++;` |
|      44293 |   87 | `	return SXRET_OK;` |
|      37432 |   88 | `}` |
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
|    2939650 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2939655 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1199 |  103 | `		pSet->nUsed = nNewSize;` |
|        597 |  104 | `	}` |
|    2939655 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   58020434 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   58020439 |  109 | `	sxi32 rc = SXRET_OK;` |
|   58020439 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   31372381 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   15686188 |  112 | `	}` |
|   58020439 |  113 | `	pSet->pBase = 0;` |
|   58020439 |  114 | `	pSet->nUsed = 0;` |
|   58020439 |  115 | `	pSet->nCursor = 0;` |
|   58020439 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   67917962 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   67917967 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19247 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   67898725 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   67898725 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   33958986 |  126 | `}` |
|    9506978 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9506983 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2233051 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7273937 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7273937 |  135 | `	pSet->nUsed--;` |
|    7273937 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7273937 |  137 | `	return pData;` |
|    4753494 |  138 | `}` |
|   34915779 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   34915784 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   34915732 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   34915732 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   17457852 |  148 | `}` |
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
|    1842676 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1842681 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1842681 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1842681 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1842681 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1842681 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1842681 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1842681 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1842681 |  180 | `	pHash->nEntry = 0;` |
|    1842681 |  181 | `	pHash->apBucket = apNew;` |
|    1842681 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1842681 |  183 | `	return SXRET_OK;` |
|     921343 |  184 | `}` |
|     414114 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     414119 |  193 | `	pEntry = pHash->pList;` |
|     221620 |  194 | `	for(;;){` |
|     443245 |  195 | `		if( pHash->nEntry == 0 ){` |
|     414119 |  196 | `			break;` |
|          - |  197 | `		}` |
|      29131 |  198 | `		pNext = pEntry->pNext;` |
|      29131 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      29131 |  200 | `		pEntry = pNext;` |
|      29131 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     414119 |  203 | `	if( pHash->apBucket ){` |
|     414119 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     207057 |  205 | `	}` |
|     414119 |  206 | `	pHash->apBucket = 0;` |
|     414119 |  207 | `	pHash->nBucketSize = 0;` |
|     414119 |  208 | `	pHash->pAllocator = 0;` |
|     414119 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   70043131 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   70043136 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   70043136 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   63033021 |  218 | `	for(;;){` |
|  125845203 |  219 | `		if( pEntry == 0 ){` |
|   25377496 |  220 | `			break;` |
|          - |  221 | `		}` |
|  122802395 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   44669648 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   44665645 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   55802072 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   25377496 |  229 | `	return 0;` |
|   35021854 |  230 | `}` |
|   77298887 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   77298892 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    7256193 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   70042704 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   70042704 |  244 | `	if( pEntry == 0 ){` |
|   25377478 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   44665231 |  247 | `	return (SyHashEntry *)pEntry;` |
|   38649732 |  248 | `}` |
|     438808 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     438813 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     358159 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     179082 |  254 | `	}else{` |
|      80659 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     438813 |  257 | `	if( pEntry->pNextCollide ){` |
|       4522 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2261 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     438813 |  261 | `	if( pHash->pLast == pEntry ){` |
|     428823 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     214409 |  263 | `	}` |
|     438813 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     438813 |  265 | `	pHash->nEntry--;` |
|     438813 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     438813 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     438813 |  272 | `	return rc;` |
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
|     438394 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     438399 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     438399 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     438399 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3040790 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3040795 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3040795 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   23417576 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   23417581 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3040529 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3040529 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   20377057 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   20377057 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   20377057 |  328 | `	return (SyHashEntry *)pEntry;` |
|   11708793 |  329 | `}` |
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
|       4213 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4199 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4199 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4199 |  348 | `		pEntry = pEntry->pNext;` |
|       2100 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|      92284 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      92289 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      92289 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      92289 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      92289 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14644353 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14552069 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14552069 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14552069 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14552069 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    6974965 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3487461 |  375 | `		}` |
|   14552069 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14552069 |  378 | `		pEntry = pEntry->pNext;` |
|    7276037 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      92289 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      92289 |  382 | `	pHash->apBucket = apNew;` |
|      92289 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      92289 |  384 | `	return SXRET_OK;` |
|      46147 |  385 | `}` |
|   19538276 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   19538281 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   19538281 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   19538281 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   12337067 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    6168420 |  393 | `	}` |
|   19538281 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   19538281 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   19538229 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   19538281 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1035751 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1035751 |  408 | `		pHash->pLast = pEntry;` |
|     517873 |  409 | `	}` |
|   19538281 |  410 | `	pHash->nEntry++;` |
|   19538281 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   19538276 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   19538281 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      92289 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      92289 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      46142 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   19538281 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   19538281 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   19538281 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   19538281 |  435 | `	pEntry->pHash = pHash;` |
|   19538281 |  436 | `	pEntry->pKey = pKey;` |
|   19538281 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   19538281 |  438 | `	pEntry->pUserData = pUserData;` |
|   19538281 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   19538281 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   19538281 |  442 | `	return rc;` |
|    9769143 |  443 | `}` |
|   19538142 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   19538147 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     478144 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     478149 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
