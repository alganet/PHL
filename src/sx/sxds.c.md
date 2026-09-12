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
|  168851724 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  168851729 |   16 | `	pSet->nSize = 0 ;` |
|  168851729 |   17 | `	pSet->nUsed = 0;` |
|  168851729 |   18 | `	pSet->nCursor = 0;` |
|  168851729 |   19 | `	pSet->eSize = ElemSize;` |
|  168851729 |   20 | `	pSet->pAllocator = pAllocator;` |
|  168851729 |   21 | `	pSet->pBase =  0;` |
|  168851729 |   22 | `	pSet->pUserData = 0;` |
|  168851729 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  383517159 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  383517164 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   22112063 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   22112063 |   33 | `		if( pSet->nSize <= 0 ){` |
|   18846965 |   34 | `			pSet->nSize = 4;` |
|    9423480 |   35 | `		}` |
|   22112063 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   22112063 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   22112063 |   40 | `		pSet->pBase = pNew;` |
|   22112063 |   41 | `		pSet->nSize <<= 1;` |
|   11056029 |   42 | `	}` |
|  383517164 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3025321310 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  383517164 |   45 | `	pSet->nUsed++;` |
|  383517164 |   46 | `	return SXRET_OK;` |
|  191758608 |   47 | `}` |
|   19074904 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   19074909 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   19074909 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   19074909 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   19074909 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   19074909 |   60 | `	pSet->nSize = nItem;` |
|   19074909 |   61 | `	return SXRET_OK;` |
|    9537457 |   62 | `}` |
|   28381361 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   28381366 |   65 | `	pSet->nUsed   = 0;` |
|   28381366 |   66 | `	pSet->nCursor = 0;` |
|   28381366 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      70724 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      70729 |   71 | `	pSet->nCursor = 0;` |
|      70729 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      74910 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      74915 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30599 |   79 | `		pSet->nCursor = 0;` |
|      30599 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      44321 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      44321 |   83 | `	if( ppEntry ){` |
|      44321 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      22158 |   85 | `	}` |
|      44321 |   86 | `	pSet->nCursor++;` |
|      44321 |   87 | `	return SXRET_OK;` |
|      37460 |   88 | `}` |
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
|    2958060 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2958065 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1199 |  103 | `		pSet->nUsed = nNewSize;` |
|        597 |  104 | `	}` |
|    2958065 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   58330002 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   58330007 |  109 | `	sxi32 rc = SXRET_OK;` |
|   58330007 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   31541049 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   15770522 |  112 | `	}` |
|   58330007 |  113 | `	pSet->pBase = 0;` |
|   58330007 |  114 | `	pSet->nUsed = 0;` |
|   58330007 |  115 | `	pSet->nCursor = 0;` |
|   58330007 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   68329002 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   68329007 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19367 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   68309645 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   68309645 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   34164506 |  126 | `}` |
|    9540476 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9540481 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2233823 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7306663 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7306663 |  135 | `	pSet->nUsed--;` |
|    7306663 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7306663 |  137 | `	return pData;` |
|    4770243 |  138 | `}` |
|   35032280 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   35032285 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   35032233 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   35032233 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   17516095 |  148 | `}` |
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
|    1853324 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1853329 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1853329 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1853329 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1853329 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1853329 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1853329 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1853329 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1853329 |  180 | `	pHash->nEntry = 0;` |
|    1853329 |  181 | `	pHash->apBucket = apNew;` |
|    1853329 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1853329 |  183 | `	return SXRET_OK;` |
|     926667 |  184 | `}` |
|     415858 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     415863 |  193 | `	pEntry = pHash->pList;` |
|     222492 |  194 | `	for(;;){` |
|     444989 |  195 | `		if( pHash->nEntry == 0 ){` |
|     415863 |  196 | `			break;` |
|          - |  197 | `		}` |
|      29131 |  198 | `		pNext = pEntry->pNext;` |
|      29131 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      29131 |  200 | `		pEntry = pNext;` |
|      29131 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     415863 |  203 | `	if( pHash->apBucket ){` |
|     415863 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     207929 |  205 | `	}` |
|     415863 |  206 | `	pHash->apBucket = 0;` |
|     415863 |  207 | `	pHash->nBucketSize = 0;` |
|     415863 |  208 | `	pHash->pAllocator = 0;` |
|     415863 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   70421777 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   70421782 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   70421782 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   63232392 |  218 | `	for(;;){` |
|  126355175 |  219 | `		if( pEntry == 0 ){` |
|   25511124 |  220 | `			break;` |
|          - |  221 | `		}` |
|  123301260 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   44914690 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   44910663 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   55933398 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   25511124 |  229 | `	return 0;` |
|   35211177 |  230 | `}` |
|   77722249 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   77722254 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    7300909 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   70421350 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   70421350 |  244 | `	if( pEntry == 0 ){` |
|   25511106 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   44910249 |  247 | `	return (SyHashEntry *)pEntry;` |
|   38861413 |  248 | `}` |
|     439094 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     439099 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     358381 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     179193 |  254 | `	}else{` |
|      80723 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     439099 |  257 | `	if( pEntry->pNextCollide ){` |
|       4524 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2261 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     439099 |  261 | `	if( pHash->pLast == pEntry ){` |
|     429109 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     214552 |  263 | `	}` |
|     439099 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     439099 |  265 | `	pHash->nEntry--;` |
|     439099 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     439099 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     439099 |  272 | `	return rc;` |
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
|     438680 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     438685 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     438685 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     438685 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3058598 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3058603 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3058603 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   23551616 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   23551621 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3058337 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3058337 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   20493289 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   20493289 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   20493289 |  328 | `	return (SyHashEntry *)pEntry;` |
|   11775813 |  329 | `}` |
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
|       4221 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4207 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4207 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4207 |  348 | `		pEntry = pEntry->pNext;` |
|       2104 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|      92884 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      92889 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      92889 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      92889 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      92889 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14739417 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14646533 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14646533 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14646533 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14646533 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    7021313 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3510461 |  375 | `		}` |
|   14646533 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14646533 |  378 | `		pEntry = pEntry->pNext;` |
|    7323269 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      92889 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      92889 |  382 | `	pHash->apBucket = apNew;` |
|      92889 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      92889 |  384 | `	return SXRET_OK;` |
|      46447 |  385 | `}` |
|   19675912 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   19675917 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   19675917 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   19675917 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   12431035 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    6215566 |  393 | `	}` |
|   19675917 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   19675917 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   19675865 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   19675917 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1041647 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1041647 |  408 | `		pHash->pLast = pEntry;` |
|     520821 |  409 | `	}` |
|   19675917 |  410 | `	pHash->nEntry++;` |
|   19675917 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   19675912 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   19675917 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      92889 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      92889 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      46442 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   19675917 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   19675917 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   19675917 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   19675917 |  435 | `	pEntry->pHash = pHash;` |
|   19675917 |  436 | `	pEntry->pKey = pKey;` |
|   19675917 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   19675917 |  438 | `	pEntry->pUserData = pUserData;` |
|   19675917 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   19675917 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   19675917 |  442 | `	return rc;` |
|    9837961 |  443 | `}` |
|   19675778 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   19675783 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     478700 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     478705 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
