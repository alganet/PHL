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
|  172616814 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  172616819 |   16 | `	pSet->nSize = 0 ;` |
|  172616819 |   17 | `	pSet->nUsed = 0;` |
|  172616819 |   18 | `	pSet->nCursor = 0;` |
|  172616819 |   19 | `	pSet->eSize = ElemSize;` |
|  172616819 |   20 | `	pSet->pAllocator = pAllocator;` |
|  172616819 |   21 | `	pSet->pBase =  0;` |
|  172616819 |   22 | `	pSet->pUserData = 0;` |
|  172616819 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  391115316 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  391115321 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   22538131 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   22538131 |   33 | `		if( pSet->nSize <= 0 ){` |
|   19199435 |   34 | `			pSet->nSize = 4;` |
|    9600390 |   35 | `		}` |
|   22538131 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   22538131 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   22538131 |   40 | `		pSet->pBase = pNew;` |
|   22538131 |   41 | `		pSet->nSize <<= 1;` |
|   11269738 |   42 | `	}` |
|  391115321 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3084872337 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  391115321 |   45 | `	pSet->nUsed++;` |
|  391115321 |   46 | `	return SXRET_OK;` |
|  195560126 |   47 | `}` |
|   19434974 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   19434979 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   19434979 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   19434979 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   19434979 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   19434979 |   60 | `	pSet->nSize = nItem;` |
|   19434979 |   61 | `	return SXRET_OK;` |
|    9717492 |   62 | `}` |
|   28932502 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   28932507 |   65 | `	pSet->nUsed   = 0;` |
|   28932507 |   66 | `	pSet->nCursor = 0;` |
|   28932507 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      71038 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      71043 |   71 | `	pSet->nCursor = 0;` |
|      71043 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      71456 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      71461 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30753 |   79 | `		pSet->nCursor = 0;` |
|      30753 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      40713 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      40713 |   83 | `	if( ppEntry ){` |
|      40713 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      20354 |   85 | `	}` |
|      40713 |   86 | `	pSet->nCursor++;` |
|      40713 |   87 | `	return SXRET_OK;` |
|      35733 |   88 | `}` |
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
|    3005954 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3005959 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1199 |  103 | `		pSet->nUsed = nNewSize;` |
|        597 |  104 | `	}` |
|    3005959 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   59414954 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   59414959 |  109 | `	sxi32 rc = SXRET_OK;` |
|   59414959 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   32149179 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   16075262 |  112 | `	}` |
|   59414959 |  113 | `	pSet->pBase = 0;` |
|   59414959 |  114 | `	pSet->nUsed = 0;` |
|   59414959 |  115 | `	pSet->nCursor = 0;` |
|   59414959 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   69577596 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   69577601 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19529 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   69558077 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   69558077 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   34788803 |  126 | `}` |
|    9696258 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9696263 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2236041 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7460227 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7460227 |  135 | `	pSet->nUsed--;` |
|    7460227 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7460227 |  137 | `	return pData;` |
|    4848584 |  138 | `}` |
|   35878059 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   35878064 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   35878012 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   35878012 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   17943222 |  148 | `}` |
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
|    1903180 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1903185 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1903185 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1903185 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1903185 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1903185 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1903185 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1903185 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1903185 |  180 | `	pHash->nEntry = 0;` |
|    1903185 |  181 | `	pHash->apBucket = apNew;` |
|    1903185 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1903185 |  183 | `	return SXRET_OK;` |
|     951670 |  184 | `}` |
|     442056 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     442061 |  193 | `	pEntry = pHash->pList;` |
|     235771 |  194 | `	for(;;){` |
|     471397 |  195 | `		if( pHash->nEntry == 0 ){` |
|     442061 |  196 | `			break;` |
|          - |  197 | `		}` |
|      29341 |  198 | `		pNext = pEntry->pNext;` |
|      29341 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      29341 |  200 | `		pEntry = pNext;` |
|      29341 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     442061 |  203 | `	if( pHash->apBucket ){` |
|     442061 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     221103 |  205 | `	}` |
|     442061 |  206 | `	pHash->apBucket = 0;` |
|     442061 |  207 | `	pHash->nBucketSize = 0;` |
|     442061 |  208 | `	pHash->pAllocator = 0;` |
|     442061 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   72665092 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   72665097 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   72665097 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   65594746 |  218 | `	for(;;){` |
|  130990232 |  219 | `		if( pEntry == 0 ){` |
|   26725203 |  220 | `			break;` |
|          - |  221 | `		}` |
|  127234083 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   45943942 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   45939899 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   58325140 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   26725203 |  229 | `	return 0;` |
|   36339208 |  230 | `}` |
|   80095804 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   80095809 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    7431149 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   72664665 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   72664665 |  244 | `	if( pEntry == 0 ){` |
|   26725185 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   45939485 |  247 | `	return (SyHashEntry *)pEntry;` |
|   40054639 |  248 | `}` |
|     488144 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     488149 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     399183 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     199969 |  254 | `	}else{` |
|      88971 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     488149 |  257 | `	if( pEntry->pNextCollide ){` |
|       4610 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2303 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     488149 |  261 | `	if( pHash->pLast == pEntry ){` |
|     478109 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     239502 |  263 | `	}` |
|     488149 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     488149 |  265 | `	pHash->nEntry--;` |
|     488149 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     488149 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     488149 |  272 | `	return rc;` |
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
|     487730 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     487735 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     487735 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     487735 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3105700 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3105705 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3105705 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   23881394 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   23881399 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3105439 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3105439 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   20775965 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   20775965 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   20775965 |  328 | `	return (SyHashEntry *)pEntry;` |
|   11940702 |  329 | `}` |
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
|       4557 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4543 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4543 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4543 |  348 | `		pEntry = pEntry->pNext;` |
|       2272 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|     100278 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100283 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100283 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100283 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100283 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18621371 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18521093 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18521093 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18521093 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18521093 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8878288 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4439419 |  375 | `		}` |
|   18521093 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18521093 |  378 | `		pEntry = pEntry->pNext;` |
|    9260549 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100283 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100283 |  382 | `	pHash->apBucket = apNew;` |
|     100283 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100283 |  384 | `	return SXRET_OK;` |
|      50144 |  385 | `}` |
|   20551130 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   20551135 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   20551135 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   20551135 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   13108238 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    6554300 |  393 | `	}` |
|   20551135 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   20551135 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   20551083 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   20551135 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1065607 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1065607 |  408 | `		pHash->pLast = pEntry;` |
|     532876 |  409 | `	}` |
|   20551135 |  410 | `	pHash->nEntry++;` |
|   20551135 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   20551130 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   20551135 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100283 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100283 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50139 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   20551135 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   20551135 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   20551135 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   20551135 |  435 | `	pEntry->pHash = pHash;` |
|   20551135 |  436 | `	pEntry->pKey = pKey;` |
|   20551135 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   20551135 |  438 | `	pEntry->pUserData = pUserData;` |
|   20551135 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   20551135 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   20551135 |  442 | `	return rc;` |
|   10276020 |  443 | `}` |
|   20550996 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   20551001 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     528172 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     528177 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
