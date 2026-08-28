# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 290/304 lines (95.39%)

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
|   84597642 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|   84597647 |   16 | `	pSet->nSize = 0 ;` |
|   84597647 |   17 | `	pSet->nUsed = 0;` |
|   84597647 |   18 | `	pSet->nCursor = 0;` |
|   84597647 |   19 | `	pSet->eSize = ElemSize;` |
|   84597647 |   20 | `	pSet->pAllocator = pAllocator;` |
|   84597647 |   21 | `	pSet->pBase =  0;` |
|   84597647 |   22 | `	pSet->pUserData = 0;` |
|   84597647 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  182811393 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  182811398 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   12287303 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   12287303 |   33 | `		if( pSet->nSize <= 0 ){` |
|   10809803 |   34 | `			pSet->nSize = 4;` |
|    5404899 |   35 | `		}` |
|   12287303 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   12287303 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   12287303 |   40 | `		pSet->pBase = pNew;` |
|   12287303 |   41 | `		pSet->nSize <<= 1;` |
|    6143649 |   42 | `	}` |
|  182811398 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 1339756714 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  182811398 |   45 | `	pSet->nUsed++;` |
|  182811398 |   46 | `	return SXRET_OK;` |
|   91405744 |   47 | `}` |
|    8911428 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|    8911433 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|    8911433 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|    8911433 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    8911433 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|    8911433 |   60 | `	pSet->nSize = nItem;` |
|    8911433 |   61 | `	return SXRET_OK;` |
|    4455719 |   62 | `}` |
|   13870053 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   13870058 |   65 | `	pSet->nUsed   = 0;` |
|   13870058 |   66 | `	pSet->nCursor = 0;` |
|   13870058 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69004 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69009 |   71 | `	pSet->nCursor = 0;` |
|      69009 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73220 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73225 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29705 |   79 | `		pSet->nCursor = 0;` |
|      29705 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43525 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43525 |   83 | `	if( ppEntry ){` |
|      43525 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21760 |   85 | `	}` |
|      43525 |   86 | `	pSet->nCursor++;` |
|      43525 |   87 | `	return SXRET_OK;` |
|      36615 |   88 | `}` |
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
|    1414066 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    1414071 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1179 |  103 | `		pSet->nUsed = nNewSize;` |
|        587 |  104 | `	}` |
|    1414071 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   31376394 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   31376399 |  109 | `	sxi32 rc = SXRET_OK;` |
|   31376399 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   16846471 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|    8423233 |  112 | `	}` |
|   31376399 |  113 | `	pSet->pBase = 0;` |
|   31376399 |  114 | `	pSet->nUsed = 0;` |
|   31376399 |  115 | `	pSet->nCursor = 0;` |
|   31376399 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   31871156 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   31871161 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15605 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   31855561 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   31855561 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   15935583 |  126 | `}` |
|    6251372 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    6251377 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2195379 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    4056003 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    4056003 |  135 | `	pSet->nUsed--;` |
|    4056003 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    4056003 |  137 | `	return pData;` |
|    3125691 |  138 | `}` |
|   21603384 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   21603389 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   21603367 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   21603367 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   10802054 |  148 | `}` |
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
|    1171398 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1171403 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1171403 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1171403 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1171403 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1171403 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1171403 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1171403 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1171403 |  180 | `	pHash->nEntry = 0;` |
|    1171403 |  181 | `	pHash->apBucket = apNew;` |
|    1171403 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1171403 |  183 | `	return SXRET_OK;` |
|     585704 |  184 | `}` |
|     311222 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     311227 |  193 | `	pEntry = pHash->pList;` |
|     164668 |  194 | `	for(;;){` |
|     329341 |  195 | `		if( pHash->nEntry == 0 ){` |
|     311227 |  196 | `			break;` |
|          - |  197 | `		}` |
|      18119 |  198 | `		pNext = pEntry->pNext;` |
|      18119 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      18119 |  200 | `		pEntry = pNext;` |
|      18119 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     311227 |  203 | `	if( pHash->apBucket ){` |
|     311227 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     155611 |  205 | `	}` |
|     311227 |  206 | `	pHash->apBucket = 0;` |
|     311227 |  207 | `	pHash->nBucketSize = 0;` |
|     311227 |  208 | `	pHash->pAllocator = 0;` |
|     311227 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   41015783 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   41015788 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   41015788 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   38338616 |  218 | `	for(;;){` |
|   76786293 |  219 | `		if( pEntry == 0 ){` |
|   16262048 |  220 | `			break;` |
|          - |  221 | `		}` |
|   72900886 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   24753782 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   24753745 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   35770510 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   16262048 |  229 | `	return 0;` |
|   20508408 |  230 | `}` |
|   44774777 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   44774782 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    3759321 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   41015466 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   41015466 |  244 | `	if( pEntry == 0 ){` |
|   16262048 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   24753423 |  247 | `	return (SyHashEntry *)pEntry;` |
|   22387905 |  248 | `}` |
|     215252 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     215257 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     172215 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      86110 |  254 | `	}else{` |
|      43047 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     215257 |  257 | `	if( pEntry->pNextCollide ){` |
|       4156 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2077 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     215257 |  261 | `	if( pHash->pLast == pEntry ){` |
|     208495 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     104245 |  263 | `	}` |
|     215257 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     215257 |  265 | `	pHash->nEntry--;` |
|     215257 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|        ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        ! 0 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     215257 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     215257 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        322 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        327 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        327 |  284 | `	if( pEntry == 0 ){` |
|        ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        327 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        327 |  288 | `	return rc;` |
|        166 |  289 | `}` |
|     214930 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     214935 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     214935 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     214935 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    1791782 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    1791787 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    1791787 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   13451826 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   13451831 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    1791521 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    1791521 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   11660315 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   11660315 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   11660315 |  328 | `	return (SyHashEntry *)pEntry;` |
|    6725918 |  329 | `}` |
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
|       3131 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3121 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3121 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3121 |  348 | `		pEntry = pEntry->pNext;` |
|       1561 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      77462 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      77467 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      77467 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      77467 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      77467 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|    9220987 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|    9143525 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|    9143525 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|    9143525 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|    9143525 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    4395686 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    2197704 |  375 | `		}` |
|    9143525 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|    9143525 |  378 | `		pEntry = pEntry->pNext;` |
|    4571765 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      77467 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      77467 |  382 | `	pHash->apBucket = apNew;` |
|      77467 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      77467 |  384 | `	return SXRET_OK;` |
|      38736 |  385 | `}` |
|   11531662 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   11531667 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   11531667 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   11531667 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    7262721 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    3631497 |  393 | `	}` |
|   11531667 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   11531667 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   11531615 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   11531667 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     612135 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     612135 |  408 | `		pHash->pLast = pEntry;` |
|     306065 |  409 | `	}` |
|   11531667 |  410 | `	pHash->nEntry++;` |
|   11531667 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   11531662 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   11531667 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      77467 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      77467 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      38731 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   11531667 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   11531667 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   11531667 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   11531667 |  435 | `	pEntry->pHash = pHash;` |
|   11531667 |  436 | `	pEntry->pKey = pKey;` |
|   11531667 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   11531667 |  438 | `	pEntry->pUserData = pUserData;` |
|   11531667 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   11531667 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   11531667 |  442 | `	return rc;` |
|    5765836 |  443 | `}` |
|   11531534 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   11531539 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        128 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          2 |  455 | `{` |
|        130 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          2 |  457 | `}` |
|     255864 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     255869 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
