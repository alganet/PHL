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
|  10994584 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  10994586 |   16 | `	pSet->nSize = 0 ;` |
|  10994586 |   17 | `	pSet->nUsed = 0;` |
|  10994586 |   18 | `	pSet->nCursor = 0;` |
|  10994586 |   19 | `	pSet->eSize = ElemSize;` |
|  10994586 |   20 | `	pSet->pAllocator = pAllocator;` |
|  10994586 |   21 | `	pSet->pBase =  0;` |
|  10994586 |   22 | `	pSet->pUserData = 0;` |
|  10994586 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  17457148 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  17457150 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3448046 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3448046 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3374920 |   34 | `			pSet->nSize = 4;` |
|   1687459 |   35 | `		}` |
|   3448046 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3448046 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3448046 |   40 | `		pSet->pBase = pNew;` |
|   3448046 |   41 | `		pSet->nSize <<= 1;` |
|   1724022 |   42 | `	}` |
|  17457150 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 130628818 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  17457150 |   45 | `	pSet->nUsed++;` |
|  17457150 |   46 | `	return SXRET_OK;` |
|   8728598 |   47 |  |
|    512616 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    512618 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    512618 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    512618 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    512618 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    512618 |   60 | `	pSet->nSize = nItem;` |
|    512618 |   61 | `	return SXRET_OK;` |
|    256310 |   62 |  |
|    976116 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|    976118 |   65 | `	pSet->nUsed   = 0;` |
|    976118 |   66 | `	pSet->nCursor = 0;` |
|    976118 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     38340 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     38342 |   71 | `	pSet->nCursor = 0;` |
|     38342 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     42132 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     42134 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     15494 |   79 | `		pSet->nCursor = 0;` |
|     15494 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     26642 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     26642 |   83 | `	if( ppEntry ){` |
|     26642 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     13320 |   85 | `	}` |
|     26642 |   86 | `	pSet->nCursor++;` |
|     26642 |   87 | `	return SXRET_OK;` |
|     21068 |   88 |  |
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
|     65052 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     65054 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     65054 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   7173652 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   7173654 |  109 | `	sxi32 rc = SXRET_OK;` |
|   7173654 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3694164 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1847081 |  112 | `	}` |
|   7173654 |  113 | `	pSet->pBase = 0;` |
|   7173654 |  114 | `	pSet->nUsed = 0;` |
|   7173654 |  115 | `	pSet->nCursor = 0;` |
|   7173654 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3533170 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3533172 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3533082 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3533082 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1766587 |  126 |  |
|   3073476 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3073478 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2129246 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    944234 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    944234 |  135 | `	pSet->nUsed--;` |
|    944234 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    944234 |  137 | `	return pData;` |
|   1536740 |  138 |  |
|   9390769 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   9390771 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   9390771 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   9390771 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4695627 |  148 |  |
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
|     91916 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     91918 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     91918 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     91918 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     91918 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     91918 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     91918 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     91918 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     91918 |  180 | `	pHash->nEntry = 0;` |
|     91918 |  181 | `	pHash->apBucket = apNew;` |
|     91918 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     91918 |  183 | `	return SXRET_OK;` |
|     45960 |  184 |  |
|     11310 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     11312 |  193 | `	pEntry = pHash->pList;` |
|      6941 |  194 | `	for(;;){` |
|     13884 |  195 | `		if( pHash->nEntry == 0 ){` |
|     11312 |  196 | `			break;` |
|         - |  197 | `		}` |
|      2574 |  198 | `		pNext = pEntry->pNext;` |
|      2574 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      2574 |  200 | `		pEntry = pNext;` |
|      2574 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     11312 |  203 | `	if( pHash->apBucket ){` |
|     11312 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      5655 |  205 | `	}` |
|     11312 |  206 | `	pHash->apBucket = 0;` |
|     11312 |  207 | `	pHash->nBucketSize = 0;` |
|     11312 |  208 | `	pHash->pAllocator = 0;` |
|     11312 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   9201556 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   9201558 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   9201558 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   7902925 |  218 | `	for(;;){` |
|  15810401 |  219 | `		if( pEntry == 0 ){` |
|   5013254 |  220 | `			break;` |
|         - |  221 | `		}` |
|  12891171 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   4188308 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   4188306 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   6608845 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   5013254 |  229 | `	return 0;` |
|   4601044 |  230 |  |
|   9253304 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   9253306 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     51756 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   9201552 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   9201552 |  244 | `	if( pEntry == 0 ){` |
|   5013254 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   4188300 |  247 | `	return (SyHashEntry *)pEntry;` |
|   4626918 |  248 |  |
|     71210 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     71212 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     53504 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     26753 |  254 | `	}else{` |
|     17710 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     71212 |  257 | `	if( pEntry->pNextCollide ){` |
|      4091 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2045 |  259 | `	}` |
|     71212 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     71212 |  261 | `	pHash->nEntry--;` |
|     71212 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     71212 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     71212 |  268 | `	return rc;` |
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
|     71204 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     71206 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     71206 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     71206 |  296 | `	return rc;` |
|         2 |  297 |  |
|    131400 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    131402 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    131402 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    913042 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    913044 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    130968 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    130968 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    782078 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    782078 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    782078 |  324 | `	return (SyHashEntry *)pEntry;` |
|    456523 |  325 |  |
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
|      1609 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1599 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1599 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1599 |  344 | `		pEntry = pEntry->pNext;` |
|       800 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     13528 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     13530 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     13530 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     13530 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     13530 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1859514 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1845986 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1845986 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1845986 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1845986 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    886416 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    443207 |  371 | `		}` |
|   1845986 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1845986 |  374 | `		pEntry = pEntry->pNext;` |
|    922994 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     13530 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     13530 |  378 | `	pHash->apBucket = apNew;` |
|     13530 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     13530 |  380 | `	return SXRET_OK;` |
|      6766 |  381 |  |
|   1663658 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1663660 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1663660 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1663660 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1120332 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    560170 |  389 | `	}` |
|   1663660 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1663660 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1663660 |  393 | `	if( pHash->nEntry == 0 ){` |
|     66056 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     33027 |  395 | `	}` |
|   1663660 |  396 | `	pHash->nEntry++;` |
|   1663660 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1663658 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1663660 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     13530 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     13530 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      6764 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1663660 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1663660 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1663660 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1663660 |  421 | `	pEntry->pHash = pHash;` |
|   1663660 |  422 | `	pEntry->pKey = pKey;` |
|   1663660 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1663660 |  424 | `	pEntry->pUserData = pUserData;` |
|   1663660 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1663660 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1663660 |  428 | `	return rc;` |
|    831831 |  429 |  |
|     88318 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     88320 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
