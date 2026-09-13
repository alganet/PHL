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
|  172358136 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  172358141 |   16 | `	pSet->nSize = 0 ;` |
|  172358141 |   17 | `	pSet->nUsed = 0;` |
|  172358141 |   18 | `	pSet->nCursor = 0;` |
|  172358141 |   19 | `	pSet->eSize = ElemSize;` |
|  172358141 |   20 | `	pSet->pAllocator = pAllocator;` |
|  172358141 |   21 | `	pSet->pBase =  0;` |
|  172358141 |   22 | `	pSet->pUserData = 0;` |
|  172358141 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  390392560 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  390392565 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   22495759 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   22495759 |   33 | `		if( pSet->nSize <= 0 ){` |
|   19158795 |   34 | `			pSet->nSize = 4;` |
|    9580070 |   35 | `		}` |
|   22495759 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   22495759 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   22495759 |   40 | `		pSet->pBase = pNew;` |
|   22495759 |   41 | `		pSet->nSize <<= 1;` |
|   11248552 |   42 | `	}` |
|  390392565 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3079758419 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  390392565 |   45 | `	pSet->nUsed++;` |
|  390392565 |   46 | `	return SXRET_OK;` |
|  195198747 |   47 | `}` |
|   19425020 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   19425025 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   19425025 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   19425025 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   19425025 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   19425025 |   60 | `	pSet->nSize = nItem;` |
|   19425025 |   61 | `	return SXRET_OK;` |
|    9712515 |   62 | `}` |
|   28917952 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   28917957 |   65 | `	pSet->nUsed   = 0;` |
|   28917957 |   66 | `	pSet->nCursor = 0;` |
|   28917957 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      71022 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      71027 |   71 | `	pSet->nCursor = 0;` |
|      71027 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      71438 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      71443 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30745 |   79 | `		pSet->nCursor = 0;` |
|      30745 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      40703 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      40703 |   83 | `	if( ppEntry ){` |
|      40703 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      20349 |   85 | `	}` |
|      40703 |   86 | `	pSet->nCursor++;` |
|      40703 |   87 | `	return SXRET_OK;` |
|      35724 |   88 | `}` |
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
|    3004408 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3004413 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1199 |  103 | `		pSet->nUsed = nNewSize;` |
|        597 |  104 | `	}` |
|    3004413 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   59354838 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   59354843 |  109 | `	sxi32 rc = SXRET_OK;` |
|   59354843 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   32102429 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   16051887 |  112 | `	}` |
|   59354843 |  113 | `	pSet->pBase = 0;` |
|   59354843 |  114 | `	pSet->nUsed = 0;` |
|   59354843 |  115 | `	pSet->nCursor = 0;` |
|   59354843 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   69465834 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   69465839 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19519 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   69446325 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   69446325 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   34732922 |  126 | `}` |
|    9691434 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9691439 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2235151 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7456293 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7456293 |  135 | `	pSet->nUsed--;` |
|    7456293 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7456293 |  137 | `	return pData;` |
|    4846172 |  138 | `}` |
|   35838073 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   35838078 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   35838026 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   35838026 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   17923224 |  148 | `}` |
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
|    1902272 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1902277 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1902277 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1902277 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1902277 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1902277 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1902277 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1902277 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1902277 |  180 | `	pHash->nEntry = 0;` |
|    1902277 |  181 | `	pHash->apBucket = apNew;` |
|    1902277 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1902277 |  183 | `	return SXRET_OK;` |
|     951216 |  184 | `}` |
|     441894 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     441899 |  193 | `	pEntry = pHash->pList;` |
|     235684 |  194 | `	for(;;){` |
|     471223 |  195 | `		if( pHash->nEntry == 0 ){` |
|     441899 |  196 | `			break;` |
|          - |  197 | `		}` |
|      29329 |  198 | `		pNext = pEntry->pNext;` |
|      29329 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      29329 |  200 | `		pEntry = pNext;` |
|      29329 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     441899 |  203 | `	if( pHash->apBucket ){` |
|     441899 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     221022 |  205 | `	}` |
|     441899 |  206 | `	pHash->apBucket = 0;` |
|     441899 |  207 | `	pHash->nBucketSize = 0;` |
|     441899 |  208 | `	pHash->pAllocator = 0;` |
|     441899 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   72577680 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   72577685 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   72577685 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   65434694 |  218 | `	for(;;){` |
|  130896362 |  219 | `		if( pEntry == 0 ){` |
|   26709861 |  220 | `			break;` |
|          - |  221 | `		}` |
|  127119525 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   45871870 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   45867829 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   58318682 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   26709861 |  229 | `	return 0;` |
|   36295490 |  230 | `}` |
|   79996864 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   79996869 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    7419621 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   72577253 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   72577253 |  244 | `	if( pEntry == 0 ){` |
|   26709843 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   45867415 |  247 | `	return (SyHashEntry *)pEntry;` |
|   40005157 |  248 | `}` |
|     488072 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     488077 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     399141 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     199948 |  254 | `	}else{` |
|      88941 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     488077 |  257 | `	if( pEntry->pNextCollide ){` |
|       4631 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2314 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     488077 |  261 | `	if( pHash->pLast == pEntry ){` |
|     478037 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     239466 |  263 | `	}` |
|     488077 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     488077 |  265 | `	pHash->nEntry--;` |
|     488077 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     488077 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     488077 |  272 | `	return rc;` |
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
|     487658 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     487663 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     487663 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     487663 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3104202 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3104207 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3104207 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   23870160 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   23870165 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3103941 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3103941 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   20766229 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   20766229 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   20766229 |  328 | `	return (SyHashEntry *)pEntry;` |
|   11935085 |  329 | `}` |
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
|     100224 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100229 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100229 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100229 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100229 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18611141 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18510917 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18510917 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18510917 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18510917 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8868715 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4434376 |  375 | `		}` |
|   18510917 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18510917 |  378 | `		pEntry = pEntry->pNext;` |
|    9255461 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100229 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100229 |  382 | `	pHash->apBucket = apNew;` |
|     100229 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100229 |  384 | `	return SXRET_OK;` |
|      50117 |  385 | `}` |
|   20536492 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   20536497 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   20536497 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   20536497 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   13097899 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    6548767 |  393 | `	}` |
|   20536497 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   20536497 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   20536445 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   20536497 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1065105 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1065105 |  408 | `		pHash->pLast = pEntry;` |
|     532625 |  409 | `	}` |
|   20536497 |  410 | `	pHash->nEntry++;` |
|   20536497 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   20536492 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   20536497 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100229 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100229 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50112 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   20536497 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   20536497 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   20536497 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   20536497 |  435 | `	pEntry->pHash = pHash;` |
|   20536497 |  436 | `	pEntry->pKey = pKey;` |
|   20536497 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   20536497 |  438 | `	pEntry->pUserData = pUserData;` |
|   20536497 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   20536497 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   20536497 |  442 | `	return rc;` |
|   10268701 |  443 | `}` |
|   20536358 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   20536363 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     528078 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     528083 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
