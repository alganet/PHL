# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 289/304 lines (95.07%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits | Line | Source |
| --------: | ---: | :--- |
|         - |    1 | `/**` |
|         - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|         - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |    5 | ` */` |
|         - |    6 | `#include "sxtypes.h"` |
|         - |    7 | `#include "sxmacros.h"` |
|         - |    8 | `#include "sxset.h"` |
|         - |    9 | `#include "sxmem.h"` |
|         - |   10 | `#include "sxhashtable.h"` |
|         - |   11 | `#include "sxhash.h"` |
|         - |   12 | `#include "sxstr.h"` |
|         - |   13 |  |
|  21438028 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 | `{` |
|  21438033 |   16 | `	pSet->nSize = 0 ;` |
|  21438033 |   17 | `	pSet->nUsed = 0;` |
|  21438033 |   18 | `	pSet->nCursor = 0;` |
|  21438033 |   19 | `	pSet->eSize = ElemSize;` |
|  21438033 |   20 | `	pSet->pAllocator = pAllocator;` |
|  21438033 |   21 | `	pSet->pBase =  0;` |
|  21438033 |   22 | `	pSet->pUserData = 0;` |
|  21438033 |   23 | `	return SXRET_OK;` |
|         5 |   24 | `}` |
|  35980881 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 | `{` |
|         - |   27 | `	unsigned char *zbase;` |
|  35980886 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4998499 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4998499 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4815191 |   34 | `			pSet->nSize = 4;` |
|   2407593 |   35 | `		}` |
|   4998499 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4998499 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4998499 |   40 | `		pSet->pBase = pNew;` |
|   4998499 |   41 | `		pSet->nSize <<= 1;` |
|   2499247 |   42 | `	}` |
|  35980886 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 269321966 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  35980886 |   45 | `	pSet->nUsed++;` |
|  35980886 |   46 | `	return SXRET_OK;` |
|  17990488 |   47 | `}` |
|   1529542 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 | `{` |
|   1529547 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1529547 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1529547 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1529547 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1529547 |   60 | `	pSet->nSize = nItem;` |
|   1529547 |   61 | `	return SXRET_OK;` |
|    764776 |   62 | `}` |
|   2414783 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 | `{` |
|   2414788 |   65 | `	pSet->nUsed   = 0;` |
|   2414788 |   66 | `	pSet->nCursor = 0;` |
|   2414788 |   67 | `	return SXRET_OK;` |
|         5 |   68 | `}` |
|     67300 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 | `{` |
|     67305 |   71 | `	pSet->nCursor = 0;` |
|     67305 |   72 | `	return SXRET_OK;` |
|         5 |   73 | `}` |
|     71458 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 | `{` |
|         - |   76 | `	register unsigned char *zSrc;` |
|     71463 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     28943 |   79 | `		pSet->nCursor = 0;` |
|     28943 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     42525 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     42525 |   83 | `	if( ppEntry ){` |
|     42525 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     21260 |   85 | `	}` |
|     42525 |   86 | `	pSet->nCursor++;` |
|     42525 |   87 | `	return SXRET_OK;` |
|     35734 |   88 | `}` |
|         - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         8 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|         1 |   91 | `{` |
|         - |   92 | `	register unsigned char *zSrc;` |
|         9 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         3 |   94 | `		return 0;` |
|         - |   95 | `	}` |
|         7 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|         7 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|         5 |   98 | `}` |
|         - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    245594 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 | `{` |
|    245599 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       679 |  103 | `		pSet->nUsed = nNewSize;` |
|       337 |  104 | `	}` |
|    245599 |  105 | `	return SXRET_OK;` |
|         5 |  106 | `}` |
|  10972480 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 | `{` |
|  10972485 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10972485 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5534035 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2767015 |  112 | `	}` |
|  10972485 |  113 | `	pSet->pBase = 0;` |
|  10972485 |  114 | `	pSet->nUsed = 0;` |
|  10972485 |  115 | `	pSet->nCursor = 0;` |
|  10972485 |  116 | `	return rc;` |
|         5 |  117 | `}` |
|   6425212 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 | `{` |
|         - |  120 | `	const char *zBase;` |
|   6425217 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       133 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   6425089 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   6425089 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   3212611 |  126 | `}` |
|   3801920 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 | `{` |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3801925 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2193853 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1608077 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1608077 |  135 | `	pSet->nUsed--;` |
|   1608077 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1608077 |  137 | `	return pData;` |
|   1900965 |  138 | `}` |
|  14486502 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	const char *zBase;` |
|  14486507 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  14486507 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  14486507 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   7243585 |  148 | `}` |
|         - |  149 | `/* Private hash entry */` |
|         - |  150 | `struct SyHashEntry_Pr` |
|         - |  151 | `{` |
|         - |  152 | `	const void *pKey; /* Hash key */` |
|         - |  153 | `	sxu32 nKeyLen;    /* Key length */` |
|         - |  154 | `	void *pUserData;  /* User private data */` |
|         - |  155 | `	/* Private fields */` |
|         - |  156 | `	sxu32 nHash;` |
|         - |  157 | `	SyHash *pHash;` |
|         - |  158 | `	SyHashEntry_Pr *pNext,*pPrev; /* Next and previous entry in the list */` |
|         - |  159 | `	SyHashEntry_Pr *pNextCollide,*pPrevCollide; /* Collision list */` |
|         - |  160 | `};` |
|         - |  161 | `#define INVALID_HASH(H) ((H)->apBucket == 0)` |
|    688514 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 | `{` |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    688519 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    688519 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    688519 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    688519 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    688519 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    688519 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    688519 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    688519 |  180 | `	pHash->nEntry = 0;` |
|    688519 |  181 | `	pHash->apBucket = apNew;` |
|    688519 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    688519 |  183 | `	return SXRET_OK;` |
|    344262 |  184 | `}` |
|    159366 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 | `{` |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    159371 |  193 | `	pEntry = pHash->pList;` |
|     85974 |  194 | `	for(;;){` |
|    171953 |  195 | `		if( pHash->nEntry == 0 ){` |
|    159371 |  196 | `			break;` |
|         - |  197 | `		}` |
|     12587 |  198 | `		pNext = pEntry->pNext;` |
|     12587 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     12587 |  200 | `		pEntry = pNext;` |
|     12587 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    159371 |  203 | `	if( pHash->apBucket ){` |
|    159371 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     79683 |  205 | `	}` |
|    159371 |  206 | `	pHash->apBucket = 0;` |
|    159371 |  207 | `	pHash->nBucketSize = 0;` |
|    159371 |  208 | `	pHash->pAllocator = 0;` |
|    159371 |  209 | `	return SXRET_OK;` |
|         5 |  210 | `}` |
|  20337508 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 | `{` |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  20337513 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  20337513 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  18648828 |  218 | `	for(;;){` |
|  37088397 |  219 | `		if( pEntry == 0 ){` |
|  10375755 |  220 | `			break;` |
|         - |  221 | `		}` |
|  31693277 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   9961770 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   9961763 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  16750889 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|  10375755 |  229 | `	return 0;` |
|  10169269 |  230 | `}` |
|  21375742 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 | `{` |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  21375747 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|   1038503 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  20337249 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  20337249 |  244 | `	if( pEntry == 0 ){` |
|  10375755 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   9961499 |  247 | `	return (SyHashEntry *)pEntry;` |
|  10688386 |  248 | `}` |
|    190044 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 | `{` |
|         - |  251 | `	sxi32 rc;` |
|    190049 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    150261 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     75133 |  254 | `	}else{` |
|     39793 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    190049 |  257 | `	if( pEntry->pNextCollide ){` |
|      3530 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      1764 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    190049 |  261 | `	if( pHash->pLast == pEntry ){` |
|    183479 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     91737 |  263 | `	}` |
|    190049 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    190049 |  265 | `	pHash->nEntry--;` |
|    190049 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    190049 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    190049 |  272 | `	return rc;` |
|         5 |  273 | `}` |
|       264 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 | `{` |
|         - |  276 | `	SyHashEntry_Pr *pEntry;` |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( INVALID_HASH(pHash) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|       269 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       269 |  284 | `	if( pEntry == 0 ){` |
|       ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|         - |  286 | `	}` |
|       269 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       269 |  288 | `	return rc;` |
|       137 |  289 | `}` |
|    189780 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 | `{` |
|    189785 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    189785 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    189785 |  300 | `	return rc;` |
|         5 |  301 | `}` |
|   1337610 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 | `{` |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1337615 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1337615 |  310 | `	return SXRET_OK;` |
|         5 |  311 | `}` |
|   8405526 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 | `{` |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   8405531 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1337353 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1337353 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   7068183 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   7068183 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   7068183 |  328 | `	return (SyHashEntry *)pEntry;` |
|   4202768 |  329 | `}` |
|        10 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         1 |  331 | `{` |
|         - |  332 | `	SyHashEntry_Pr *pEntry;` |
|         - |  333 | `	sxi32 rc;` |
|         - |  334 | `	sxu32 n;` |
|         - |  335 | `#if defined(UNTRUST)` |
|         - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  337 | `		return 0;` |
|         - |  338 | `	}` |
|         - |  339 | `#endif` |
|        11 |  340 | `	pEntry = pHash->pList;` |
|      2139 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|      2129 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      2129 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|      2129 |  348 | `		pEntry = pEntry->pNext;` |
|      1065 |  349 | `	}` |
|        11 |  350 | `	return SXRET_OK;` |
|         6 |  351 | `}` |
|     36374 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  353 | `{` |
|     36379 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  355 | `	SyHashEntry_Pr *pEntry;` |
|         - |  356 | `	SyHashEntry_Pr **apNew;` |
|         - |  357 | `	sxu32 n,iBucket;` |
|         - |  358 |  |
|         - |  359 | `	/* Allocate a new larger table */` |
|     36379 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     36379 |  361 | `	if( apNew == 0 ){` |
|         - |  362 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  363 | `		return SXRET_OK;` |
|         - |  364 | `	}` |
|         - |  365 | `	/* Zero the new table */` |
|     36379 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  367 | `	/* Rehash all entries */` |
|   4478875 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   4442501 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  370 | `		/* Install in the new bucket */` |
|   4442501 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   4442501 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   4442501 |  373 | `		if( apNew[iBucket] != 0 ){` |
|   2156676 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|   1078306 |  375 | `		}` |
|   4442501 |  376 | `		apNew[iBucket] = pEntry;` |
|         - |  377 | `		/* Point to the next entry */` |
|   4442501 |  378 | `		pEntry = pEntry->pNext;` |
|   2221253 |  379 | `	}` |
|         - |  380 | `	/* Release the old table and reflect the change */` |
|     36379 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     36379 |  382 | `	pHash->apBucket = apNew;` |
|     36379 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     36379 |  384 | `	return SXRET_OK;` |
|     18192 |  385 | `}` |
|   5730152 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 | `{` |
|   5730157 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   5730157 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   5730157 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   3221292 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1610694 |  393 | `	}` |
|   5730157 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   5730157 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        51 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        51 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        51 |  401 | `		pHash->pLast = pEntry;` |
|        26 |  402 | `	}else{` |
|   5730107 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   5730157 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    365899 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    365899 |  408 | `		pHash->pLast = pEntry;` |
|    182947 |  409 | `	}` |
|   5730157 |  410 | `	pHash->nEntry++;` |
|   5730157 |  411 | `	return SXRET_OK;` |
|         5 |  412 | `}` |
|   5730152 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 | `{` |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   5730157 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     36379 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     36379 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     18187 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   5730157 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   5730157 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   5730157 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   5730157 |  435 | `	pEntry->pHash = pHash;` |
|   5730157 |  436 | `	pEntry->pKey = pKey;` |
|   5730157 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   5730157 |  438 | `	pEntry->pUserData = pUserData;` |
|   5730157 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   5730157 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   5730157 |  442 | `	return rc;` |
|   2865081 |  443 | `}` |
|   5730036 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 | `{` |
|   5730041 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|         5 |  447 | `}` |
|         - |  448 | `/*` |
|         - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|         - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|         - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|         - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|         - |  453 | ` */` |
|       116 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  455 | `{` |
|       118 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|         2 |  457 | `}` |
|    230002 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 | `{` |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    230007 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 | `}` |
|         - |  468 |  |
