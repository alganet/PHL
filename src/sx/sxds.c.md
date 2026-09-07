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
|  140293256 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  140293261 |   16 | `	pSet->nSize = 0 ;` |
|  140293261 |   17 | `	pSet->nUsed = 0;` |
|  140293261 |   18 | `	pSet->nCursor = 0;` |
|  140293261 |   19 | `	pSet->eSize = ElemSize;` |
|  140293261 |   20 | `	pSet->pAllocator = pAllocator;` |
|  140293261 |   21 | `	pSet->pBase =  0;` |
|  140293261 |   22 | `	pSet->pUserData = 0;` |
|  140293261 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  313986534 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  313986539 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   18594395 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   18594395 |   33 | `		if( pSet->nSize <= 0 ){` |
|   15918923 |   34 | `			pSet->nSize = 4;` |
|    7959459 |   35 | `		}` |
|   18594395 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   18594395 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   18594395 |   40 | `		pSet->pBase = pNew;` |
|   18594395 |   41 | `		pSet->nSize <<= 1;` |
|    9297195 |   42 | `	}` |
|  313986539 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2323646987 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  313986539 |   45 | `	pSet->nUsed++;` |
|  313986539 |   46 | `	return SXRET_OK;` |
|  156993294 |   47 | `}` |
|   15406896 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   15406901 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   15406901 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   15406901 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   15406901 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   15406901 |   60 | `	pSet->nSize = nItem;` |
|   15406901 |   61 | `	return SXRET_OK;` |
|    7703453 |   62 | `}` |
|   22216198 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   22216203 |   65 | `	pSet->nUsed   = 0;` |
|   22216203 |   66 | `	pSet->nCursor = 0;` |
|   22216203 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      68968 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      68973 |   71 | `	pSet->nCursor = 0;` |
|      68973 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73076 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73081 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29839 |   79 | `		pSet->nCursor = 0;` |
|      29839 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43247 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43247 |   83 | `	if( ppEntry ){` |
|      43247 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21621 |   85 | `	}` |
|      43247 |   86 | `	pSet->nCursor++;` |
|      43247 |   87 | `	return SXRET_OK;` |
|      36543 |   88 | `}` |
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
|    2532854 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2532859 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1181 |  103 | `		pSet->nUsed = nNewSize;` |
|        588 |  104 | `	}` |
|    2532859 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   48542546 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   48542551 |  109 | `	sxi32 rc = SXRET_OK;` |
|   48542551 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   25952457 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   12976226 |  112 | `	}` |
|   48542551 |  113 | `	pSet->pBase = 0;` |
|   48542551 |  114 | `	pSet->nUsed = 0;` |
|   48542551 |  115 | `	pSet->nCursor = 0;` |
|   48542551 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   56500554 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   56500559 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15301 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   56485263 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   56485263 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   28250282 |  126 | `}` |
|    7973638 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    7973643 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2218243 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    5755405 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    5755405 |  135 | `	pSet->nUsed--;` |
|    5755405 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    5755405 |  137 | `	return pData;` |
|    3986824 |  138 | `}` |
|   29689380 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   29689385 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   29689363 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   29689363 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   14844794 |  148 | `}` |
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
|    1750698 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1750703 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1750703 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1750703 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1750703 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1750703 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1750703 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1750703 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1750703 |  180 | `	pHash->nEntry = 0;` |
|    1750703 |  181 | `	pHash->apBucket = apNew;` |
|    1750703 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1750703 |  183 | `	return SXRET_OK;` |
|     875354 |  184 | `}` |
|     402256 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     402261 |  193 | `	pEntry = pHash->pList;` |
|     213954 |  194 | `	for(;;){` |
|     427913 |  195 | `		if( pHash->nEntry == 0 ){` |
|     402261 |  196 | `			break;` |
|          - |  197 | `		}` |
|      25657 |  198 | `		pNext = pEntry->pNext;` |
|      25657 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      25657 |  200 | `		pEntry = pNext;` |
|      25657 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     402261 |  203 | `	if( pHash->apBucket ){` |
|     402261 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     201128 |  205 | `	}` |
|     402261 |  206 | `	pHash->apBucket = 0;` |
|     402261 |  207 | `	pHash->nBucketSize = 0;` |
|     402261 |  208 | `	pHash->pAllocator = 0;` |
|     402261 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   61228633 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   61228638 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   61228638 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   55985538 |  218 | `	for(;;){` |
|  111970812 |  219 | `		if( pEntry == 0 ){` |
|   23006290 |  220 | `			break;` |
|          - |  221 | `		}` |
|  108075616 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   38222442 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   38222353 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   50742179 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   23006290 |  229 | `	return 0;` |
|   30614587 |  230 | `}` |
|   67376319 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   67376324 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    6148043 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   61228286 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   61228286 |  244 | `	if( pEntry == 0 ){` |
|   23006272 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   38222019 |  247 | `	return (SyHashEntry *)pEntry;` |
|   33688430 |  248 | `}` |
|     403308 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     403313 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     330893 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     165449 |  254 | `	}else{` |
|      72425 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     403313 |  257 | `	if( pEntry->pNextCollide ){` |
|       4372 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2185 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     403313 |  261 | `	if( pHash->pLast == pEntry ){` |
|     396267 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     198131 |  263 | `	}` |
|     403313 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     403313 |  265 | `	pHash->nEntry--;` |
|     403313 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     403313 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     403313 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        352 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        357 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        357 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        339 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        339 |  288 | `	return rc;` |
|        181 |  289 | `}` |
|     402974 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     402979 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     402979 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     402979 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2807496 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2807501 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2807501 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   20861286 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   20861291 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2807235 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2807235 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18054061 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18054061 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18054061 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10430648 |  329 | `}` |
|         10 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|          1 |  331 | `{` |
|          - |  332 | `	SyHashEntry_Pr *pEntry;` |
|          - |  333 | `	sxi32 rc;` |
|          - |  334 | `	sxu32 n;` |
|          - |  335 | `#if defined(UNTRUST)` |
|          - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|          - |  337 | `		return 0;` |
|          - |  338 | `	}` |
|          - |  339 | `#endif` |
|         11 |  340 | `	pEntry = pHash->pList;` |
|       3821 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3811 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3811 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3811 |  348 | `		pEntry = pEntry->pNext;` |
|       1906 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      90834 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      90839 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      90839 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      90839 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      90839 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14286647 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14195813 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14195813 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14195813 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14195813 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    6791921 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3396229 |  375 | `		}` |
|   14195813 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14195813 |  378 | `		pEntry = pEntry->pNext;` |
|    7097909 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      90839 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      90839 |  382 | `	pHash->apBucket = apNew;` |
|      90839 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      90839 |  384 | `	return SXRET_OK;` |
|      45422 |  385 | `}` |
|   17377244 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   17377249 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   17377249 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   17377249 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   10802664 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5401349 |  393 | `	}` |
|   17377249 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   17377249 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   17377197 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   17377249 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     963097 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     963097 |  408 | `		pHash->pLast = pEntry;` |
|     481546 |  409 | `	}` |
|   17377249 |  410 | `	pHash->nEntry++;` |
|   17377249 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   17377244 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   17377249 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      90839 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      90839 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45417 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   17377249 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   17377249 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   17377249 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   17377249 |  435 | `	pEntry->pHash = pHash;` |
|   17377249 |  436 | `	pEntry->pKey = pKey;` |
|   17377249 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   17377249 |  438 | `	pEntry->pUserData = pUserData;` |
|   17377249 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   17377249 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   17377249 |  442 | `	return rc;` |
|    8688627 |  443 | `}` |
|   17377112 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   17377117 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        132 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          2 |  455 | `{` |
|        134 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          2 |  457 | `}` |
|     442402 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     442407 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
