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
|  141917800 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  141917805 |   16 | `	pSet->nSize = 0 ;` |
|  141917805 |   17 | `	pSet->nUsed = 0;` |
|  141917805 |   18 | `	pSet->nCursor = 0;` |
|  141917805 |   19 | `	pSet->eSize = ElemSize;` |
|  141917805 |   20 | `	pSet->pAllocator = pAllocator;` |
|  141917805 |   21 | `	pSet->pBase =  0;` |
|  141917805 |   22 | `	pSet->pUserData = 0;` |
|  141917805 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  318729820 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  318729825 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   18835929 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   18835929 |   33 | `		if( pSet->nSize <= 0 ){` |
|   16106469 |   34 | `			pSet->nSize = 4;` |
|    8053232 |   35 | `		}` |
|   18835929 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   18835929 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   18835929 |   40 | `		pSet->pBase = pNew;` |
|   18835929 |   41 | `		pSet->nSize <<= 1;` |
|    9417962 |   42 | `	}` |
|  318729825 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2506464549 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  318729825 |   45 | `	pSet->nUsed++;` |
|  318729825 |   46 | `	return SXRET_OK;` |
|  159364937 |   47 | `}` |
|   15559438 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   15559443 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   15559443 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   15559443 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   15559443 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   15559443 |   60 | `	pSet->nSize = nItem;` |
|   15559443 |   61 | `	return SXRET_OK;` |
|    7779724 |   62 | `}` |
|   22475862 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   22475867 |   65 | `	pSet->nUsed   = 0;` |
|   22475867 |   66 | `	pSet->nCursor = 0;` |
|   22475867 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      68988 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      68993 |   71 | `	pSet->nCursor = 0;` |
|      68993 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73080 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73085 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29857 |   79 | `		pSet->nCursor = 0;` |
|      29857 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43233 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43233 |   83 | `	if( ppEntry ){` |
|      43233 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21614 |   85 | `	}` |
|      43233 |   86 | `	pSet->nCursor++;` |
|      43233 |   87 | `	return SXRET_OK;` |
|      36545 |   88 | `}` |
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
|    2534204 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2534209 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1181 |  103 | `		pSet->nUsed = nNewSize;` |
|        588 |  104 | `	}` |
|    2534209 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   49083896 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   49083901 |  109 | `	sxi32 rc = SXRET_OK;` |
|   49083901 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   26290909 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   13145452 |  112 | `	}` |
|   49083901 |  113 | `	pSet->pBase = 0;` |
|   49083901 |  114 | `	pSet->nUsed = 0;` |
|   49083901 |  115 | `	pSet->nCursor = 0;` |
|   49083901 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   57208710 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   57208715 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15309 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   57193411 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   57193411 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   28604360 |  126 | `}` |
|    8060188 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    8060193 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2212365 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    5847833 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    5847833 |  135 | `	pSet->nUsed--;` |
|    5847833 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    5847833 |  137 | `	return pData;` |
|    4030099 |  138 | `}` |
|   29996907 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   29996912 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   29996890 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   29996890 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   14998523 |  148 | `}` |
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
|    1754852 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1754857 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1754857 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1754857 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1754857 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1754857 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1754857 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1754857 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1754857 |  180 | `	pHash->nEntry = 0;` |
|    1754857 |  181 | `	pHash->apBucket = apNew;` |
|    1754857 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1754857 |  183 | `	return SXRET_OK;` |
|     877431 |  184 | `}` |
|     405644 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     405649 |  193 | `	pEntry = pHash->pList;` |
|     215690 |  194 | `	for(;;){` |
|     431385 |  195 | `		if( pHash->nEntry == 0 ){` |
|     405649 |  196 | `			break;` |
|          - |  197 | `		}` |
|      25741 |  198 | `		pNext = pEntry->pNext;` |
|      25741 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      25741 |  200 | `		pEntry = pNext;` |
|      25741 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     405649 |  203 | `	if( pHash->apBucket ){` |
|     405649 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     202822 |  205 | `	}` |
|     405649 |  206 | `	pHash->apBucket = 0;` |
|     405649 |  207 | `	pHash->nBucketSize = 0;` |
|     405649 |  208 | `	pHash->pAllocator = 0;` |
|     405649 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   61862433 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   61862438 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   61862438 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   56407570 |  218 | `	for(;;){` |
|  113035056 |  219 | `		if( pEntry == 0 ){` |
|   23107436 |  220 | `			break;` |
|          - |  221 | `		}` |
|  109305032 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   38755078 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   38755007 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   51172623 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   23107436 |  229 | `	return 0;` |
|   30931487 |  230 | `}` |
|   68087139 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   68087144 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    6225063 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   61862086 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   61862086 |  244 | `	if( pEntry == 0 ){` |
|   23107418 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   38754673 |  247 | `	return (SyHashEntry *)pEntry;` |
|   34043840 |  248 | `}` |
|     419260 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     419265 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     344355 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     172180 |  254 | `	}else{` |
|      74915 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     419265 |  257 | `	if( pEntry->pNextCollide ){` |
|       4384 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2191 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     419265 |  261 | `	if( pHash->pLast == pEntry ){` |
|     412221 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     206108 |  263 | `	}` |
|     419265 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     419265 |  265 | `	pHash->nEntry--;` |
|     419265 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     419265 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     419265 |  272 | `	return rc;` |
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
|     418926 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     418931 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     418931 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     418931 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2808220 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2808225 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2808225 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   20864928 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   20864933 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2807959 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2807959 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18056979 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18056979 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18056979 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10432469 |  329 | `}` |
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
|       3827 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3817 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3817 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3817 |  348 | `		pEntry = pEntry->pNext;` |
|       1909 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      90866 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      90871 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      90871 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      90871 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      90871 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14291191 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14200325 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14200325 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14200325 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14200325 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    6794090 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3397016 |  375 | `		}` |
|   14200325 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14200325 |  378 | `		pEntry = pEntry->pNext;` |
|    7100165 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      90871 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      90871 |  382 | `	pHash->apBucket = apNew;` |
|      90871 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      90871 |  384 | `	return SXRET_OK;` |
|      45438 |  385 | `}` |
|   17411430 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   17411435 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   17411435 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   17411435 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   10827478 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5413644 |  393 | `	}` |
|   17411435 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   17411435 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   17411383 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   17411435 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     966827 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     966827 |  408 | `		pHash->pLast = pEntry;` |
|     483411 |  409 | `	}` |
|   17411435 |  410 | `	pHash->nEntry++;` |
|   17411435 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   17411430 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   17411435 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      90871 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      90871 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45433 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   17411435 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   17411435 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   17411435 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   17411435 |  435 | `	pEntry->pHash = pHash;` |
|   17411435 |  436 | `	pEntry->pKey = pKey;` |
|   17411435 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   17411435 |  438 | `	pEntry->pUserData = pUserData;` |
|   17411435 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   17411435 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   17411435 |  442 | `	return rc;` |
|    8705720 |  443 | `}` |
|   17411298 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   17411303 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     458366 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     458371 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
