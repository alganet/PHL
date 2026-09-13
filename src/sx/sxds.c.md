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
|  172112168 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  172112173 |   16 | `	pSet->nSize = 0 ;` |
|  172112173 |   17 | `	pSet->nUsed = 0;` |
|  172112173 |   18 | `	pSet->nCursor = 0;` |
|  172112173 |   19 | `	pSet->eSize = ElemSize;` |
|  172112173 |   20 | `	pSet->pAllocator = pAllocator;` |
|  172112173 |   21 | `	pSet->pBase =  0;` |
|  172112173 |   22 | `	pSet->pUserData = 0;` |
|  172112173 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  389823876 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  389823881 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   22470641 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   22470641 |   33 | `		if( pSet->nSize <= 0 ){` |
|   19138791 |   34 | `			pSet->nSize = 4;` |
|    9570068 |   35 | `		}` |
|   22470641 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   22470641 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   22470641 |   40 | `		pSet->pBase = pNew;` |
|   22470641 |   41 | `		pSet->nSize <<= 1;` |
|   11235993 |   42 | `	}` |
|  389823881 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3075197921 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  389823881 |   45 | `	pSet->nUsed++;` |
|  389823881 |   46 | `	return SXRET_OK;` |
|  194914406 |   47 | `}` |
|   19395058 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   19395063 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   19395063 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   19395063 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   19395063 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   19395063 |   60 | `	pSet->nSize = nItem;` |
|   19395063 |   61 | `	return SXRET_OK;` |
|    9697534 |   62 | `}` |
|   28874402 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   28874407 |   65 | `	pSet->nUsed   = 0;` |
|   28874407 |   66 | `	pSet->nCursor = 0;` |
|   28874407 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      70972 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      70977 |   71 | `	pSet->nCursor = 0;` |
|      70977 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      75160 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      75165 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30723 |   79 | `		pSet->nCursor = 0;` |
|      30723 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      44447 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      44447 |   83 | `	if( ppEntry ){` |
|      44447 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      22221 |   85 | `	}` |
|      44447 |   86 | `	pSet->nCursor++;` |
|      44447 |   87 | `	return SXRET_OK;` |
|      37585 |   88 | `}` |
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
|    2999770 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2999775 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1199 |  103 | `		pSet->nUsed = nNewSize;` |
|        597 |  104 | `	}` |
|    2999775 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   59282084 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   59282089 |  109 | `	sxi32 rc = SXRET_OK;` |
|   59282089 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   32062459 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   16031902 |  112 | `	}` |
|   59282089 |  113 | `	pSet->pBase = 0;` |
|   59282089 |  114 | `	pSet->nUsed = 0;` |
|   59282089 |  115 | `	pSet->nCursor = 0;` |
|   59282089 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   69361882 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   69361887 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19487 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   69342405 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   69342405 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   34680946 |  126 | `}` |
|    9685726 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9685731 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2234953 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7450783 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7450783 |  135 | `	pSet->nUsed--;` |
|    7450783 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7450783 |  137 | `	return pData;` |
|    4843318 |  138 | `}` |
|   35810432 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   35810437 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   35810385 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   35810385 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   17909404 |  148 | `}` |
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
|    1899540 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1899545 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1899545 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1899545 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1899545 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1899545 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1899545 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1899545 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1899545 |  180 | `	pHash->nEntry = 0;` |
|    1899545 |  181 | `	pHash->apBucket = apNew;` |
|    1899545 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1899545 |  183 | `	return SXRET_OK;` |
|     949850 |  184 | `}` |
|     441406 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     441411 |  193 | `	pEntry = pHash->pList;` |
|     235440 |  194 | `	for(;;){` |
|     470735 |  195 | `		if( pHash->nEntry == 0 ){` |
|     441411 |  196 | `			break;` |
|          - |  197 | `		}` |
|      29329 |  198 | `		pNext = pEntry->pNext;` |
|      29329 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      29329 |  200 | `		pEntry = pNext;` |
|      29329 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     441411 |  203 | `	if( pHash->apBucket ){` |
|     441411 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     220778 |  205 | `	}` |
|     441411 |  206 | `	pHash->apBucket = 0;` |
|     441411 |  207 | `	pHash->nBucketSize = 0;` |
|     441411 |  208 | `	pHash->pAllocator = 0;` |
|     441411 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   72481228 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   72481233 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   72481233 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   65359507 |  218 | `	for(;;){` |
|  130719555 |  219 | `		if( pEntry == 0 ){` |
|   26676897 |  220 | `			break;` |
|          - |  221 | `		}` |
|  126943929 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   45808376 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   45804341 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   58238327 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   26676897 |  229 | `	return 0;` |
|   36247276 |  230 | `}` |
|   79889096 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   79889101 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    7408305 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   72480801 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   72480801 |  244 | `	if( pEntry == 0 ){` |
|   26676879 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   45803927 |  247 | `	return (SyHashEntry *)pEntry;` |
|   39951285 |  248 | `}` |
|     487918 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     487923 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     399025 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     199890 |  254 | `	}else{` |
|      88903 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     487923 |  257 | `	if( pEntry->pNextCollide ){` |
|       4631 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2314 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     487923 |  261 | `	if( pHash->pLast == pEntry ){` |
|     477883 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     239389 |  263 | `	}` |
|     487923 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     487923 |  265 | `	pHash->nEntry--;` |
|     487923 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     487923 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     487923 |  272 | `	return rc;` |
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
|     487504 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     487509 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     487509 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     487509 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3099630 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3099635 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3099635 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   23835860 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   23835865 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3099369 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3099369 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   20736501 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   20736501 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   20736501 |  328 | `	return (SyHashEntry *)pEntry;` |
|   11917935 |  329 | `}` |
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
|     100062 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100067 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100067 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100067 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100067 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18580451 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18480389 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18480389 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18480389 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18480389 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8855789 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4427628 |  375 | `		}` |
|   18480389 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18480389 |  378 | `		pEntry = pEntry->pNext;` |
|    9240197 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100067 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100067 |  382 | `	pHash->apBucket = apNew;` |
|     100067 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100067 |  384 | `	return SXRET_OK;` |
|      50036 |  385 | `}` |
|   20504200 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   20504205 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   20504205 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   20504205 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   13076455 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    6538423 |  393 | `	}` |
|   20504205 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   20504205 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   20504153 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   20504205 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1063607 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1063607 |  408 | `		pHash->pLast = pEntry;` |
|     531876 |  409 | `	}` |
|   20504205 |  410 | `	pHash->nEntry++;` |
|   20504205 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   20504200 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   20504205 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100067 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100067 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50031 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   20504205 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   20504205 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   20504205 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   20504205 |  435 | `	pEntry->pHash = pHash;` |
|   20504205 |  436 | `	pEntry->pKey = pKey;` |
|   20504205 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   20504205 |  438 | `	pEntry->pUserData = pUserData;` |
|   20504205 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   20504205 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   20504205 |  442 | `	return rc;` |
|   10252555 |  443 | `}` |
|   20504066 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   20504071 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     527850 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     527855 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
