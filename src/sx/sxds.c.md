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
|  20750456 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 | `{` |
|  20750461 |   16 | `	pSet->nSize = 0 ;` |
|  20750461 |   17 | `	pSet->nUsed = 0;` |
|  20750461 |   18 | `	pSet->nCursor = 0;` |
|  20750461 |   19 | `	pSet->eSize = ElemSize;` |
|  20750461 |   20 | `	pSet->pAllocator = pAllocator;` |
|  20750461 |   21 | `	pSet->pBase =  0;` |
|  20750461 |   22 | `	pSet->pUserData = 0;` |
|  20750461 |   23 | `	return SXRET_OK;` |
|         5 |   24 | `}` |
|  34393579 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 | `{` |
|         - |   27 | `	unsigned char *zbase;` |
|  34393584 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4855567 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4855567 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4684595 |   34 | `			pSet->nSize = 4;` |
|   2342295 |   35 | `		}` |
|   4855567 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4855567 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4855567 |   40 | `		pSet->pBase = pNew;` |
|   4855567 |   41 | `		pSet->nSize <<= 1;` |
|   2427781 |   42 | `	}` |
|  34393584 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 257124300 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  34393584 |   45 | `	pSet->nUsed++;` |
|  34393584 |   46 | `	return SXRET_OK;` |
|  17196837 |   47 | `}` |
|   1432844 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 | `{` |
|   1432849 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1432849 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1432849 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1432849 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1432849 |   60 | `	pSet->nSize = nItem;` |
|   1432849 |   61 | `	return SXRET_OK;` |
|    716427 |   62 | `}` |
|   2310529 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 | `{` |
|   2310534 |   65 | `	pSet->nUsed   = 0;` |
|   2310534 |   66 | `	pSet->nCursor = 0;` |
|   2310534 |   67 | `	return SXRET_OK;` |
|         5 |   68 | `}` |
|     66786 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 | `{` |
|     66791 |   71 | `	pSet->nCursor = 0;` |
|     66791 |   72 | `	return SXRET_OK;` |
|         5 |   73 | `}` |
|     70980 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 | `{` |
|         - |   76 | `	register unsigned char *zSrc;` |
|     70985 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     28517 |   79 | `		pSet->nCursor = 0;` |
|     28517 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     42473 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     42473 |   83 | `	if( ppEntry ){` |
|     42473 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     21234 |   85 | `	}` |
|     42473 |   86 | `	pSet->nCursor++;` |
|     42473 |   87 | `	return SXRET_OK;` |
|     35495 |   88 | `}` |
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
|    241066 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 | `{` |
|    241071 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       673 |  103 | `		pSet->nUsed = nNewSize;` |
|       334 |  104 | `	}` |
|    241071 |  105 | `	return SXRET_OK;` |
|         5 |  106 | `}` |
|  10586512 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 | `{` |
|  10586517 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10586517 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5318323 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2659159 |  112 | `	}` |
|  10586517 |  113 | `	pSet->pBase = 0;` |
|  10586517 |  114 | `	pSet->nUsed = 0;` |
|  10586517 |  115 | `	pSet->nCursor = 0;` |
|  10586517 |  116 | `	return rc;` |
|         5 |  117 | `}` |
|   6154194 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 | `{` |
|         - |  120 | `	const char *zBase;` |
|   6154199 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       133 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   6154071 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   6154071 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   3077102 |  126 | `}` |
|   3721564 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 | `{` |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3721569 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2193097 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1528477 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1528477 |  135 | `	pSet->nUsed--;` |
|   1528477 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1528477 |  137 | `	return pData;` |
|   1860787 |  138 | `}` |
|  14144193 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	const char *zBase;` |
|  14144198 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  14144198 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  14144198 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   7072468 |  148 | `}` |
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
|    682524 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 | `{` |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    682529 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    682529 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    682529 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    682529 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    682529 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    682529 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    682529 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    682529 |  180 | `	pHash->nEntry = 0;` |
|    682529 |  181 | `	pHash->apBucket = apNew;` |
|    682529 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    682529 |  183 | `	return SXRET_OK;` |
|    341267 |  184 | `}` |
|    154870 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 | `{` |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    154875 |  193 | `	pEntry = pHash->pList;` |
|     82105 |  194 | `	for(;;){` |
|    164215 |  195 | `		if( pHash->nEntry == 0 ){` |
|    154875 |  196 | `			break;` |
|         - |  197 | `		}` |
|      9345 |  198 | `		pNext = pEntry->pNext;` |
|      9345 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      9345 |  200 | `		pEntry = pNext;` |
|      9345 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    154875 |  203 | `	if( pHash->apBucket ){` |
|    154875 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     77435 |  205 | `	}` |
|    154875 |  206 | `	pHash->apBucket = 0;` |
|    154875 |  207 | `	pHash->nBucketSize = 0;` |
|    154875 |  208 | `	pHash->pAllocator = 0;` |
|    154875 |  209 | `	return SXRET_OK;` |
|         5 |  210 | `}` |
|  19189810 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 | `{` |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  19189815 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  19189815 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  17311332 |  218 | `	for(;;){` |
|  34846406 |  219 | `		if( pEntry == 0 ){` |
|  10179243 |  220 | `			break;` |
|         - |  221 | `		}` |
|  29172204 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   9010582 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   9010577 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  15656596 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|  10179243 |  229 | `	return 0;` |
|   9595420 |  230 | `}` |
|  20160408 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 | `{` |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  20160413 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    970827 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  19189591 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  19189591 |  244 | `	if( pEntry == 0 ){` |
|  10179243 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   9010353 |  247 | `	return (SyHashEntry *)pEntry;` |
|  10080719 |  248 | `}` |
|    176370 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 | `{` |
|         - |  251 | `	sxi32 rc;` |
|    176375 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    137843 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     68924 |  254 | `	}else{` |
|     38537 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    176375 |  257 | `	if( pEntry->pNextCollide ){` |
|      5200 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2599 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    176375 |  261 | `	if( pHash->pLast == pEntry ){` |
|    169889 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     84942 |  263 | `	}` |
|    176375 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    176375 |  265 | `	pHash->nEntry--;` |
|    176375 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    176375 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    176375 |  272 | `	return rc;` |
|         5 |  273 | `}` |
|       224 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 | `{` |
|         - |  276 | `	SyHashEntry_Pr *pEntry;` |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( INVALID_HASH(pHash) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|       229 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       229 |  284 | `	if( pEntry == 0 ){` |
|       ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|         - |  286 | `	}` |
|       229 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       229 |  288 | `	return rc;` |
|       117 |  289 | `}` |
|    176146 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 | `{` |
|    176151 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    176151 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    176151 |  300 | `	return rc;` |
|         5 |  301 | `}` |
|   1336954 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 | `{` |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1336959 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1336959 |  310 | `	return SXRET_OK;` |
|         5 |  311 | `}` |
|   8392042 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 | `{` |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   8392047 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1336697 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1336697 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   7055355 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   7055355 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   7055355 |  328 | `	return (SyHashEntry *)pEntry;` |
|   4196026 |  329 | `}` |
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
|      2077 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|      2067 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      2067 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|      2067 |  348 | `		pEntry = pEntry->pNext;` |
|      1034 |  349 | `	}` |
|        11 |  350 | `	return SXRET_OK;` |
|         6 |  351 | `}` |
|     33092 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  353 | `{` |
|     33097 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  355 | `	SyHashEntry_Pr *pEntry;` |
|         - |  356 | `	SyHashEntry_Pr **apNew;` |
|         - |  357 | `	sxu32 n,iBucket;` |
|         - |  358 |  |
|         - |  359 | `	/* Allocate a new larger table */` |
|     33097 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     33097 |  361 | `	if( apNew == 0 ){` |
|         - |  362 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  363 | `		return SXRET_OK;` |
|         - |  364 | `	}` |
|         - |  365 | `	/* Zero the new table */` |
|     33097 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  367 | `	/* Rehash all entries */` |
|   4171561 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   4138469 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  370 | `		/* Install in the new bucket */` |
|   4138469 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   4138469 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   4138469 |  373 | `		if( apNew[iBucket] != 0 ){` |
|   1986492 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    993241 |  375 | `		}` |
|   4138469 |  376 | `		apNew[iBucket] = pEntry;` |
|         - |  377 | `		/* Point to the next entry */` |
|   4138469 |  378 | `		pEntry = pEntry->pNext;` |
|   2069237 |  379 | `	}` |
|         - |  380 | `	/* Release the old table and reflect the change */` |
|     33097 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     33097 |  382 | `	pHash->apBucket = apNew;` |
|     33097 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     33097 |  384 | `	return SXRET_OK;` |
|     16551 |  385 | `}` |
|   5589114 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 | `{` |
|   5589119 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   5589119 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   5589119 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   3126633 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1563286 |  393 | `	}` |
|   5589119 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   5589119 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        51 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        51 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        51 |  401 | `		pHash->pLast = pEntry;` |
|        26 |  402 | `	}else{` |
|   5589069 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   5589119 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    360585 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    360585 |  408 | `		pHash->pLast = pEntry;` |
|    180290 |  409 | `	}` |
|   5589119 |  410 | `	pHash->nEntry++;` |
|   5589119 |  411 | `	return SXRET_OK;` |
|         5 |  412 | `}` |
|   5589114 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 | `{` |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   5589119 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     33097 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     33097 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     16546 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   5589119 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   5589119 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   5589119 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   5589119 |  435 | `	pEntry->pHash = pHash;` |
|   5589119 |  436 | `	pEntry->pKey = pKey;` |
|   5589119 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   5589119 |  438 | `	pEntry->pUserData = pUserData;` |
|   5589119 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   5589119 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   5589119 |  442 | `	return rc;` |
|   2794562 |  443 | `}` |
|   5588998 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 | `{` |
|   5589003 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|    216240 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 | `{` |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    216245 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 | `}` |
|         - |  468 |  |
