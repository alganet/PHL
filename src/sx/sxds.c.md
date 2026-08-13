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
|   82249204 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|   82249209 |   16 | `	pSet->nSize = 0 ;` |
|   82249209 |   17 | `	pSet->nUsed = 0;` |
|   82249209 |   18 | `	pSet->nCursor = 0;` |
|   82249209 |   19 | `	pSet->eSize = ElemSize;` |
|   82249209 |   20 | `	pSet->pAllocator = pAllocator;` |
|   82249209 |   21 | `	pSet->pBase =  0;` |
|   82249209 |   22 | `	pSet->pUserData = 0;` |
|   82249209 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  178950117 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  178950122 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   12007393 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   12007393 |   33 | `		if( pSet->nSize <= 0 ){` |
|   10592267 |   34 | `			pSet->nSize = 4;` |
|    5296131 |   35 | `		}` |
|   12007393 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   12007393 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   12007393 |   40 | `		pSet->pBase = pNew;` |
|   12007393 |   41 | `		pSet->nSize <<= 1;` |
|    6003694 |   42 | `	}` |
|  178950122 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 1313906558 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  178950122 |   45 | `	pSet->nUsed++;` |
|  178950122 |   46 | `	return SXRET_OK;` |
|   89475106 |   47 | `}` |
|    8808944 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|    8808949 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|    8808949 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|    8808949 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    8808949 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|    8808949 |   60 | `	pSet->nSize = nItem;` |
|    8808949 |   61 | `	return SXRET_OK;` |
|    4404477 |   62 | `}` |
|   13751517 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   13751522 |   65 | `	pSet->nUsed   = 0;` |
|   13751522 |   66 | `	pSet->nCursor = 0;` |
|   13751522 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      68830 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      68835 |   71 | `	pSet->nCursor = 0;` |
|      68835 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73010 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73015 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29615 |   79 | `		pSet->nCursor = 0;` |
|      29615 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43405 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43405 |   83 | `	if( ppEntry ){` |
|      43405 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21700 |   85 | `	}` |
|      43405 |   86 | `	pSet->nCursor++;` |
|      43405 |   87 | `	return SXRET_OK;` |
|      36510 |   88 | `}` |
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
|    1406638 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    1406643 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1179 |  103 | `		pSet->nUsed = nNewSize;` |
|        587 |  104 | `	}` |
|    1406643 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   30975620 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   30975625 |  109 | `	sxi32 rc = SXRET_OK;` |
|   30975625 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   16553529 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|    8276762 |  112 | `	}` |
|   30975625 |  113 | `	pSet->pBase = 0;` |
|   30975625 |  114 | `	pSet->nUsed = 0;` |
|   30975625 |  115 | `	pSet->nCursor = 0;` |
|   30975625 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   31201374 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   31201379 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        133 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   31201251 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   31201251 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   15600692 |  126 | `}` |
|    6174204 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    6174209 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2195233 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    3978981 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    3978981 |  135 | `	pSet->nUsed--;` |
|    3978981 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    3978981 |  137 | `	return pData;` |
|    3087107 |  138 | `}` |
|   21293685 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   21293690 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         22 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   21293670 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   21293670 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   10647117 |  148 | `}` |
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
|    1161176 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1161181 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1161181 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1161181 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1161181 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1161181 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1161181 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1161181 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1161181 |  180 | `	pHash->nEntry = 0;` |
|    1161181 |  181 | `	pHash->apBucket = apNew;` |
|    1161181 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1161181 |  183 | `	return SXRET_OK;` |
|     580593 |  184 | `}` |
|     310570 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     310575 |  193 | `	pEntry = pHash->pList;` |
|     164055 |  194 | `	for(;;){` |
|     328115 |  195 | `		if( pHash->nEntry == 0 ){` |
|     310575 |  196 | `			break;` |
|          - |  197 | `		}` |
|      17545 |  198 | `		pNext = pEntry->pNext;` |
|      17545 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      17545 |  200 | `		pEntry = pNext;` |
|      17545 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     310575 |  203 | `	if( pHash->apBucket ){` |
|     310575 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     155285 |  205 | `	}` |
|     310575 |  206 | `	pHash->apBucket = 0;` |
|     310575 |  207 | `	pHash->nBucketSize = 0;` |
|     310575 |  208 | `	pHash->pAllocator = 0;` |
|     310575 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   40433265 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   40433270 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   40433270 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   37933933 |  218 | `	for(;;){` |
|   75902245 |  219 | `		if( pEntry == 0 ){` |
|   16132480 |  220 | `			break;` |
|          - |  221 | `		}` |
|   71919929 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   24300828 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   24300795 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   35468980 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   16132480 |  229 | `	return 0;` |
|   20217148 |  230 | `}` |
|   44061709 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   44061714 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    3628731 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   40432988 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   40432988 |  244 | `	if( pEntry == 0 ){` |
|   16132480 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   24300513 |  247 | `	return (SyHashEntry *)pEntry;` |
|   22031370 |  248 | `}` |
|     210698 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     210703 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     168401 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      84203 |  254 | `	}else{` |
|      42307 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     210703 |  257 | `	if( pEntry->pNextCollide ){` |
|       3950 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       1974 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     210703 |  261 | `	if( pHash->pLast == pEntry ){` |
|     203985 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     101990 |  263 | `	}` |
|     210703 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     210703 |  265 | `	pHash->nEntry--;` |
|     210703 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|        ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        ! 0 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     210703 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     210703 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        282 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        287 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        287 |  284 | `	if( pEntry == 0 ){` |
|        ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        287 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        287 |  288 | `	return rc;` |
|        146 |  289 | `}` |
|     210416 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     210421 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     210421 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     210421 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    1757288 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    1757293 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    1757293 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   13216638 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   13216643 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    1757029 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    1757029 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   11459619 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   11459619 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   11459619 |  328 | `	return (SyHashEntry *)pEntry;` |
|    6608324 |  329 | `}` |
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
|       3051 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3041 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3041 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3041 |  348 | `		pEntry = pEntry->pNext;` |
|       1521 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      77788 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      77793 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      77793 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      77793 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      77793 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|    9260769 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|    9182981 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|    9182981 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|    9182981 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|    9182981 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    4414132 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    2207000 |  375 | `		}` |
|    9182981 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|    9182981 |  378 | `		pEntry = pEntry->pNext;` |
|    4591493 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      77793 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      77793 |  382 | `	pHash->apBucket = apNew;` |
|      77793 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      77793 |  384 | `	return SXRET_OK;` |
|      38899 |  385 | `}` |
|   11462376 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   11462381 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   11462381 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   11462381 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    7220278 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    3610298 |  393 | `	}` |
|   11462381 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   11462381 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   11462329 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   11462381 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     604445 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     604445 |  408 | `		pHash->pLast = pEntry;` |
|     302220 |  409 | `	}` |
|   11462381 |  410 | `	pHash->nEntry++;` |
|   11462381 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   11462376 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   11462381 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      77793 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      77793 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      38894 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   11462381 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   11462381 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   11462381 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   11462381 |  435 | `	pEntry->pHash = pHash;` |
|   11462381 |  436 | `	pEntry->pKey = pKey;` |
|   11462381 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   11462381 |  438 | `	pEntry->pUserData = pUserData;` |
|   11462381 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   11462381 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   11462381 |  442 | `	return rc;` |
|    5731193 |  443 | `}` |
|   11462248 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   11462253 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     251420 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     251425 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
