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
|   84820370 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|   84820375 |   16 | `	pSet->nSize = 0 ;` |
|   84820375 |   17 | `	pSet->nUsed = 0;` |
|   84820375 |   18 | `	pSet->nCursor = 0;` |
|   84820375 |   19 | `	pSet->eSize = ElemSize;` |
|   84820375 |   20 | `	pSet->pAllocator = pAllocator;` |
|   84820375 |   21 | `	pSet->pBase =  0;` |
|   84820375 |   22 | `	pSet->pUserData = 0;` |
|   84820375 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  183314921 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  183314926 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   12306995 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   12306995 |   33 | `		if( pSet->nSize <= 0 ){` |
|   10825143 |   34 | `			pSet->nSize = 4;` |
|    5412569 |   35 | `		}` |
|   12306995 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   12306995 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   12306995 |   40 | `		pSet->pBase = pNew;` |
|   12306995 |   41 | `		pSet->nSize <<= 1;` |
|    6153495 |   42 | `	}` |
|  183314926 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 1343528466 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  183314926 |   45 | `	pSet->nUsed++;` |
|  183314926 |   46 | `	return SXRET_OK;` |
|   91657508 |   47 | `}` |
|    8938522 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|    8938527 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|    8938527 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|    8938527 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    8938527 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|    8938527 |   60 | `	pSet->nSize = nItem;` |
|    8938527 |   61 | `	return SXRET_OK;` |
|    4469266 |   62 | `}` |
|   13907537 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   13907542 |   65 | `	pSet->nUsed   = 0;` |
|   13907542 |   66 | `	pSet->nCursor = 0;` |
|   13907542 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      68956 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      68961 |   71 | `	pSet->nCursor = 0;` |
|      68961 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73140 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73145 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29671 |   79 | `		pSet->nCursor = 0;` |
|      29671 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43479 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43479 |   83 | `	if( ppEntry ){` |
|      43479 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21737 |   85 | `	}` |
|      43479 |   86 | `	pSet->nCursor++;` |
|      43479 |   87 | `	return SXRET_OK;` |
|      36575 |   88 | `}` |
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
|    1418404 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    1418409 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1179 |  103 | `		pSet->nUsed = nNewSize;` |
|        587 |  104 | `	}` |
|    1418409 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   31438678 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   31438683 |  109 | `	sxi32 rc = SXRET_OK;` |
|   31438683 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   16880043 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|    8440019 |  112 | `	}` |
|   31438683 |  113 | `	pSet->pBase = 0;` |
|   31438683 |  114 | `	pSet->nUsed = 0;` |
|   31438683 |  115 | `	pSet->nCursor = 0;` |
|   31438683 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   31945798 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   31945803 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        133 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   31945675 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   31945675 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   15972904 |  126 | `}` |
|    6254738 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    6254743 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2195423 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    4059325 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    4059325 |  135 | `	pSet->nUsed--;` |
|    4059325 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    4059325 |  137 | `	return pData;` |
|    3127374 |  138 | `}` |
|   21564342 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   21564347 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   21564325 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   21564325 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   10782512 |  148 | `}` |
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
|    1174142 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1174147 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1174147 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1174147 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1174147 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1174147 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1174147 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1174147 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1174147 |  180 | `	pHash->nEntry = 0;` |
|    1174147 |  181 | `	pHash->apBucket = apNew;` |
|    1174147 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1174147 |  183 | `	return SXRET_OK;` |
|     587076 |  184 | `}` |
|     311390 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     311395 |  193 | `	pEntry = pHash->pList;` |
|     164724 |  194 | `	for(;;){` |
|     329453 |  195 | `		if( pHash->nEntry == 0 ){` |
|     311395 |  196 | `			break;` |
|          - |  197 | `		}` |
|      18063 |  198 | `		pNext = pEntry->pNext;` |
|      18063 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      18063 |  200 | `		pEntry = pNext;` |
|      18063 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     311395 |  203 | `	if( pHash->apBucket ){` |
|     311395 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     155695 |  205 | `	}` |
|     311395 |  206 | `	pHash->apBucket = 0;` |
|     311395 |  207 | `	pHash->nBucketSize = 0;` |
|     311395 |  208 | `	pHash->pAllocator = 0;` |
|     311395 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   41052863 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   41052868 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   41052868 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   38289956 |  218 | `	for(;;){` |
|   76894461 |  219 | `		if( pEntry == 0 ){` |
|   16263970 |  220 | `			break;` |
|          - |  221 | `		}` |
|   73024711 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   24788940 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   24788903 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   35841598 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   16263970 |  229 | `	return 0;` |
|   20526948 |  230 | `}` |
|   44822469 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   44822474 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    3769933 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   41052546 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   41052546 |  244 | `	if( pEntry == 0 ){` |
|   16263970 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   24788581 |  247 | `	return (SyHashEntry *)pEntry;` |
|   22411751 |  248 | `}` |
|     212094 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     212099 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     169647 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      84826 |  254 | `	}else{` |
|      42457 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     212099 |  257 | `	if( pEntry->pNextCollide ){` |
|       4156 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2077 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     212099 |  261 | `	if( pHash->pLast == pEntry ){` |
|     205341 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     102668 |  263 | `	}` |
|     212099 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     212099 |  265 | `	pHash->nEntry--;` |
|     212099 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|        ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        ! 0 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     212099 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     212099 |  272 | `	return rc;` |
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
|     211772 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     211777 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     211777 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     211777 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    1797516 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    1797521 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    1797521 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   13496436 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   13496441 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    1797255 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    1797255 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   11699191 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   11699191 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   11699191 |  328 | `	return (SyHashEntry *)pEntry;` |
|    6748223 |  329 | `}` |
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
|       3123 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3113 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3113 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3113 |  348 | `		pEntry = pEntry->pNext;` |
|       1557 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      77794 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      77799 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      77799 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      77799 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      77799 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|    9262407 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|    9184613 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|    9184613 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|    9184613 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|    9184613 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    4414612 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    2207025 |  375 | `		}` |
|    9184613 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|    9184613 |  378 | `		pEntry = pEntry->pNext;` |
|    4592309 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      77799 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      77799 |  382 | `	pHash->apBucket = apNew;` |
|      77799 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      77799 |  384 | `	return SXRET_OK;` |
|      38902 |  385 | `}` |
|   11573384 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   11573389 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   11573389 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   11573389 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    7293711 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    3646945 |  393 | `	}` |
|   11573389 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   11573389 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   11573337 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   11573389 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     613225 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     613225 |  408 | `		pHash->pLast = pEntry;` |
|     306610 |  409 | `	}` |
|   11573389 |  410 | `	pHash->nEntry++;` |
|   11573389 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   11573384 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   11573389 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      77799 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      77799 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      38897 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   11573389 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   11573389 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   11573389 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   11573389 |  435 | `	pEntry->pHash = pHash;` |
|   11573389 |  436 | `	pEntry->pKey = pKey;` |
|   11573389 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   11573389 |  438 | `	pEntry->pUserData = pUserData;` |
|   11573389 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   11573389 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   11573389 |  442 | `	return rc;` |
|    5786697 |  443 | `}` |
|   11573256 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   11573261 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     252908 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     252913 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
