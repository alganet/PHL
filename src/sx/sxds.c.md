# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 272/287 lines (94.77%)

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
|  12075580 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  12075582 |   16 | `	pSet->nSize = 0 ;` |
|  12075582 |   17 | `	pSet->nUsed = 0;` |
|  12075582 |   18 | `	pSet->nCursor = 0;` |
|  12075582 |   19 | `	pSet->eSize = ElemSize;` |
|  12075582 |   20 | `	pSet->pAllocator = pAllocator;` |
|  12075582 |   21 | `	pSet->pBase =  0;` |
|  12075582 |   22 | `	pSet->pUserData = 0;` |
|  12075582 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  19691974 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  19691976 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3605648 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3605648 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3514108 |   34 | `			pSet->nSize = 4;` |
|   1757053 |   35 | `		}` |
|   3605648 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3605648 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3605648 |   40 | `		pSet->pBase = pNew;` |
|   3605648 |   41 | `		pSet->nSize <<= 1;` |
|   1802823 |   42 | `	}` |
|  19691976 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 146407048 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  19691976 |   45 | `	pSet->nUsed++;` |
|  19691976 |   46 | `	return SXRET_OK;` |
|   9846011 |   47 |  |
|    623936 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    623938 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    623938 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    623938 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    623938 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    623938 |   60 | `	pSet->nSize = nItem;` |
|    623938 |   61 | `	return SXRET_OK;` |
|    311970 |   62 |  |
|   1109046 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1109048 |   65 | `	pSet->nUsed   = 0;` |
|   1109048 |   66 | `	pSet->nCursor = 0;` |
|   1109048 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     40312 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     40314 |   71 | `	pSet->nCursor = 0;` |
|     40314 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     44200 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     44202 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     16414 |   79 | `		pSet->nCursor = 0;` |
|     16414 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     27790 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     27790 |   83 | `	if( ppEntry ){` |
|     27790 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     13894 |   85 | `	}` |
|     27790 |   86 | `	pSet->nCursor++;` |
|     27790 |   87 | `	return SXRET_OK;` |
|     22102 |   88 |  |
|         - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         8 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|         1 |   91 |  |
|         - |   92 | `	register unsigned char *zSrc;` |
|         9 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         3 |   94 | `		return 0;` |
|         - |   95 | `	}` |
|         7 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|         7 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|         5 |   98 |  |
|         - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     76428 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     76430 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     76430 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   7556598 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   7556600 |  109 | `	sxi32 rc = SXRET_OK;` |
|   7556600 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3912276 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1956137 |  112 | `	}` |
|   7556600 |  113 | `	pSet->pBase = 0;` |
|   7556600 |  114 | `	pSet->nUsed = 0;` |
|   7556600 |  115 | `	pSet->nCursor = 0;` |
|   7556600 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3883406 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3883408 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3883318 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3883318 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1941705 |  126 |  |
|   3155100 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3155102 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2135888 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1019216 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1019216 |  135 | `	pSet->nUsed--;` |
|   1019216 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1019216 |  137 | `	return pData;` |
|   1577552 |  138 |  |
|   9968620 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   9968622 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   9968622 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   9968622 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4984506 |  148 |  |
|         - |  149 | `/* Private hash entry */` |
|         - |  150 | `struct SyHashEntry_Pr` |
|         - |  151 |  |
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
|    113778 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    113780 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    113780 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    113780 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    113780 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    113780 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    113780 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    113780 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    113780 |  180 | `	pHash->nEntry = 0;` |
|    113780 |  181 | `	pHash->apBucket = apNew;` |
|    113780 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    113780 |  183 | `	return SXRET_OK;` |
|     56891 |  184 |  |
|     12436 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     12438 |  193 | `	pEntry = pHash->pList;` |
|      7834 |  194 | `	for(;;){` |
|     15670 |  195 | `		if( pHash->nEntry == 0 ){` |
|     12438 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3234 |  198 | `		pNext = pEntry->pNext;` |
|      3234 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3234 |  200 | `		pEntry = pNext;` |
|      3234 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     12438 |  203 | `	if( pHash->apBucket ){` |
|     12438 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      6218 |  205 | `	}` |
|     12438 |  206 | `	pHash->apBucket = 0;` |
|     12438 |  207 | `	pHash->nBucketSize = 0;` |
|     12438 |  208 | `	pHash->pAllocator = 0;` |
|     12438 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  10429442 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  10429444 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  10429444 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   8911666 |  218 | `	for(;;){` |
|  17923542 |  219 | `		if( pEntry == 0 ){` |
|   5677040 |  220 | `			break;` |
|         - |  221 | `		}` |
|  14622576 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   4752408 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   4752406 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   7494100 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   5677040 |  229 | `	return 0;` |
|   5214987 |  230 |  |
|  10493818 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  10493820 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     64384 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  10429438 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  10429438 |  244 | `	if( pEntry == 0 ){` |
|   5677040 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   4752400 |  247 | `	return (SyHashEntry *)pEntry;` |
|   5247175 |  248 |  |
|     77026 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     77028 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     58222 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     29112 |  254 | `	}else{` |
|     18808 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     77028 |  257 | `	if( pEntry->pNextCollide ){` |
|      4129 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2064 |  259 | `	}` |
|     77028 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     77028 |  261 | `	pHash->nEntry--;` |
|     77028 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     77028 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     77028 |  268 | `	return rc;` |
|         2 |  269 |  |
|         6 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         1 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|         7 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|         7 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|         7 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|         7 |  284 | `	return rc;` |
|         4 |  285 |  |
|     77020 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     77022 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     77022 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     77022 |  296 | `	return rc;` |
|         2 |  297 |  |
|    161818 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    161820 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    161820 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   1158254 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   1158256 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    161386 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    161386 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    996872 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    996872 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    996872 |  324 | `	return (SyHashEntry *)pEntry;` |
|    579129 |  325 |  |
|        10 |  326 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         1 |  327 |  |
|         - |  328 | `	SyHashEntry_Pr *pEntry;` |
|         - |  329 | `	sxi32 rc;` |
|         - |  330 | `	sxu32 n;` |
|         - |  331 | `#if defined(UNTRUST)` |
|         - |  332 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  333 | `		return 0;` |
|         - |  334 | `	}` |
|         - |  335 | `#endif` |
|        11 |  336 | `	pEntry = pHash->pList;` |
|      1533 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1523 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1523 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1523 |  344 | `		pEntry = pEntry->pNext;` |
|       762 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     16200 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     16202 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     16202 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     16202 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     16202 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2230922 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2214722 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2214722 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2214722 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2214722 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1063427 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    531717 |  371 | `		}` |
|   2214722 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2214722 |  374 | `		pEntry = pEntry->pNext;` |
|   1107362 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     16202 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     16202 |  378 | `	pHash->apBucket = apNew;` |
|     16202 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     16202 |  380 | `	return SXRET_OK;` |
|      8102 |  381 |  |
|   2027760 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2027762 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2027762 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2027762 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1352454 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    676223 |  389 | `	}` |
|   2027762 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2027762 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2027762 |  393 | `	if( pHash->nEntry == 0 ){` |
|     81378 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     40688 |  395 | `	}` |
|   2027762 |  396 | `	pHash->nEntry++;` |
|   2027762 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2027760 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2027762 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     16202 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     16202 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      8100 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2027762 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2027762 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2027762 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2027762 |  421 | `	pEntry->pHash = pHash;` |
|   2027762 |  422 | `	pEntry->pKey = pKey;` |
|   2027762 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2027762 |  424 | `	pEntry->pUserData = pUserData;` |
|   2027762 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2027762 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2027762 |  428 | `	return rc;` |
|   1013882 |  429 |  |
|     97594 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     97596 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
