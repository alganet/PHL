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
|  184025078 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  184025083 |   16 | `	pSet->nSize = 0 ;` |
|  184025083 |   17 | `	pSet->nUsed = 0;` |
|  184025083 |   18 | `	pSet->nCursor = 0;` |
|  184025083 |   19 | `	pSet->eSize = ElemSize;` |
|  184025083 |   20 | `	pSet->pAllocator = pAllocator;` |
|  184025083 |   21 | `	pSet->pBase =  0;` |
|  184025083 |   22 | `	pSet->pUserData = 0;` |
|  184025083 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  416990025 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  416990030 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   24099010 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   24099010 |   33 | `		if( pSet->nSize <= 0 ){` |
|   20542638 |   34 | `			pSet->nSize = 4;` |
|   10272234 |   35 | `		}` |
|   24099010 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   24099010 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   24099010 |   40 | `		pSet->pBase = pNew;` |
|   24099010 |   41 | `		pSet->nSize <<= 1;` |
|   12050420 |   42 | `	}` |
|  416990030 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3292136672 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  416990030 |   45 | `	pSet->nUsed++;` |
|  416990030 |   46 | `	return SXRET_OK;` |
|  208498130 |   47 | `}` |
|   20724958 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   20724963 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   20724963 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   20724963 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   20724963 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   20724963 |   60 | `	pSet->nSize = nItem;` |
|   20724963 |   61 | `	return SXRET_OK;` |
|   10362484 |   62 | `}` |
|   30690353 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   30690358 |   65 | `	pSet->nUsed   = 0;` |
|   30690358 |   66 | `	pSet->nCursor = 0;` |
|   30690358 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69116 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69121 |   71 | `	pSet->nCursor = 0;` |
|      69121 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      69380 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      69385 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30061 |   79 | `		pSet->nCursor = 0;` |
|      30061 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      39329 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      39329 |   83 | `	if( ppEntry ){` |
|      39329 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      19662 |   85 | `	}` |
|      39329 |   86 | `	pSet->nCursor++;` |
|      39329 |   87 | `	return SXRET_OK;` |
|      34695 |   88 | `}` |
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
|    3274714 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3274719 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1213 |  103 | `		pSet->nUsed = nNewSize;` |
|        604 |  104 | `	}` |
|    3274719 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   62951490 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   62951495 |  109 | `	sxi32 rc = SXRET_OK;` |
|   62951495 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   34077710 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   17039770 |  112 | `	}` |
|   62951495 |  113 | `	pSet->pBase = 0;` |
|   62951495 |  114 | `	pSet->nUsed = 0;` |
|   62951495 |  115 | `	pSet->nCursor = 0;` |
|   62951495 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   74113518 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   74113523 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       3997 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   74109531 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   74109531 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   37056764 |  126 | `}` |
|    9833875 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9833880 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2237561 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7596324 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7596324 |  135 | `	pSet->nUsed--;` |
|    7596324 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7596324 |  137 | `	return pData;` |
|    4917554 |  138 | `}` |
|   36976584 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   36976589 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   36976537 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   36976537 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   18492374 |  148 | `}` |
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
|    2175668 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    2175673 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2175673 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    2175673 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2175673 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    2175673 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    2175673 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    2175673 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    2175673 |  180 | `	pHash->nEntry = 0;` |
|    2175673 |  181 | `	pHash->apBucket = apNew;` |
|    2175673 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    2175673 |  183 | `	return SXRET_OK;` |
|    1087941 |  184 | `}` |
|     519512 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     519517 |  193 | `	pEntry = pHash->pList;` |
|     275251 |  194 | `	for(;;){` |
|     550303 |  195 | `		if( pHash->nEntry == 0 ){` |
|     519517 |  196 | `			break;` |
|          - |  197 | `		}` |
|      30791 |  198 | `		pNext = pEntry->pNext;` |
|      30791 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      30791 |  200 | `		pEntry = pNext;` |
|      30791 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     519517 |  203 | `	if( pHash->apBucket ){` |
|     519517 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     259858 |  205 | `	}` |
|     519517 |  206 | `	pHash->apBucket = 0;` |
|     519517 |  207 | `	pHash->nBucketSize = 0;` |
|     519517 |  208 | `	pHash->pAllocator = 0;` |
|     519517 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   74878704 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   74878709 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   74878709 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   67519759 |  218 | `	for(;;){` |
|  135371265 |  219 | `		if( pEntry == 0 ){` |
|   27192628 |  220 | `			break;` |
|          - |  221 | `		}` |
|  132021171 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   47690077 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   47686086 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   60492561 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   27192628 |  229 | `	return 0;` |
|   37445406 |  230 | `}` |
|   83853322 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   83853327 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    8975081 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   74878251 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   74878251 |  244 | `	if( pEntry == 0 ){` |
|   27192610 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   47685646 |  247 | `	return (SyHashEntry *)pEntry;` |
|   41932817 |  248 | `}` |
|     496082 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     496087 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     407281 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     204153 |  254 | `	}else{` |
|      88811 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     496087 |  257 | `	if( pEntry->pNextCollide ){` |
|       4291 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2142 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     496087 |  261 | `	if( pHash->pLast == pEntry ){` |
|     486585 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     243902 |  263 | `	}` |
|     496087 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     496087 |  265 | `	pHash->nEntry--;` |
|     496087 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     496087 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     496087 |  272 | `	return rc;` |
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
|     495642 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     495647 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     495647 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     495647 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3421082 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3421087 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3421087 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   26366384 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   26366389 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3420821 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3420821 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   22945573 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   22945573 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   22945573 |  328 | `	return (SyHashEntry *)pEntry;` |
|   13183197 |  329 | `}` |
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
|       4835 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4821 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4821 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4821 |  348 | `		pEntry = pEntry->pNext;` |
|       2411 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|     100410 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100415 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100415 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100415 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100415 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18592895 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18492485 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18492485 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18492485 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18492485 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8924998 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4462708 |  375 | `		}` |
|   18492485 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18492485 |  378 | `		pEntry = pEntry->pNext;` |
|    9246245 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100415 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100415 |  382 | `	pHash->apBucket = apNew;` |
|     100415 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100415 |  384 | `	return SXRET_OK;` |
|      50210 |  385 | `}` |
|   22513082 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   22513087 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   22513087 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   22513087 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   14143493 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    7071705 |  393 | `	}` |
|   22513087 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   22513087 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|     878377 |  399 | `		pHash->pLast->pNext = pEntry;` |
|     878377 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|     878377 |  401 | `		pHash->pLast = pEntry;` |
|     439191 |  402 | `	}else{` |
|   21634715 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   22513087 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1209839 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1209839 |  408 | `		pHash->pLast = pEntry;` |
|     605019 |  409 | `	}` |
|   22513087 |  410 | `	pHash->nEntry++;` |
|   22513087 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   22513082 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   22513087 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100415 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100415 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50205 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   22513087 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   22513087 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   22513087 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   22513087 |  435 | `	pEntry->pHash = pHash;` |
|   22513087 |  436 | `	pEntry->pKey = pKey;` |
|   22513087 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   22513087 |  438 | `	pEntry->pUserData = pUserData;` |
|   22513087 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   22513087 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   22513087 |  442 | `	return rc;` |
|   11257158 |  443 | `}` |
|   21373934 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   21373939 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|    1139148 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  455 | `{` |
|    1139153 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          5 |  457 | `}` |
|     535994 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     535999 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
