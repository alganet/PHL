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
|  19670800 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 | `{` |
|  19670805 |   16 | `	pSet->nSize = 0 ;` |
|  19670805 |   17 | `	pSet->nUsed = 0;` |
|  19670805 |   18 | `	pSet->nCursor = 0;` |
|  19670805 |   19 | `	pSet->eSize = ElemSize;` |
|  19670805 |   20 | `	pSet->pAllocator = pAllocator;` |
|  19670805 |   21 | `	pSet->pBase =  0;` |
|  19670805 |   22 | `	pSet->pUserData = 0;` |
|  19670805 |   23 | `	return SXRET_OK;` |
|         5 |   24 | `}` |
|  32500861 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 | `{` |
|         - |   27 | `	unsigned char *zbase;` |
|  32500866 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4651233 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4651233 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4490847 |   34 | `			pSet->nSize = 4;` |
|   2245421 |   35 | `		}` |
|   4651233 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4651233 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4651233 |   40 | `		pSet->pBase = pNew;` |
|   4651233 |   41 | `		pSet->nSize <<= 1;` |
|   2325614 |   42 | `	}` |
|  32500866 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 243558282 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  32500866 |   45 | `	pSet->nUsed++;` |
|  32500866 |   46 | `	return SXRET_OK;` |
|  16250478 |   47 | `}` |
|   1336566 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 | `{` |
|   1336571 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1336571 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1336571 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1336571 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1336571 |   60 | `	pSet->nSize = nItem;` |
|   1336571 |   61 | `	return SXRET_OK;` |
|    668288 |   62 | `}` |
|   1845863 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 | `{` |
|   1845868 |   65 | `	pSet->nUsed   = 0;` |
|   1845868 |   66 | `	pSet->nCursor = 0;` |
|   1845868 |   67 | `	return SXRET_OK;` |
|         5 |   68 | `}` |
|     58326 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 | `{` |
|     58331 |   71 | `	pSet->nCursor = 0;` |
|     58331 |   72 | `	return SXRET_OK;` |
|         5 |   73 | `}` |
|     62530 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 | `{` |
|         - |   76 | `	register unsigned char *zSrc;` |
|     62535 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     24165 |   79 | `		pSet->nCursor = 0;` |
|     24165 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     38375 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     38375 |   83 | `	if( ppEntry ){` |
|     38375 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     19185 |   85 | `	}` |
|     38375 |   86 | `	pSet->nCursor++;` |
|     38375 |   87 | `	return SXRET_OK;` |
|     31270 |   88 | `}` |
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
|    224874 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 | `{` |
|    224879 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       141 |  103 | `		pSet->nUsed = nNewSize;` |
|        68 |  104 | `	}` |
|    224879 |  105 | `	return SXRET_OK;` |
|         5 |  106 | `}` |
|  10142704 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 | `{` |
|  10142709 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10142709 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5088825 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2544410 |  112 | `	}` |
|  10142709 |  113 | `	pSet->pBase = 0;` |
|  10142709 |  114 | `	pSet->nUsed = 0;` |
|  10142709 |  115 | `	pSet->nCursor = 0;` |
|  10142709 |  116 | `	return rc;` |
|         5 |  117 | `}` |
|   5888320 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 | `{` |
|         - |  120 | `	const char *zBase;` |
|   5888325 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       133 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5888197 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5888197 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2944165 |  126 | `}` |
|   3618238 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 | `{` |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3618243 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2183317 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1434931 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1434931 |  135 | `	pSet->nUsed--;` |
|   1434931 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1434931 |  137 | `	return pData;` |
|   1809124 |  138 | `}` |
|  13608919 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	const char *zBase;` |
|  13608924 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13608924 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13608924 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6804791 |  148 | `}` |
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
|    588812 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 | `{` |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    588817 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    588817 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    588817 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    588817 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    588817 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    588817 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    588817 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    588817 |  180 | `	pHash->nEntry = 0;` |
|    588817 |  181 | `	pHash->apBucket = apNew;` |
|    588817 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    588817 |  183 | `	return SXRET_OK;` |
|    294411 |  184 | `}` |
|    106008 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 | `{` |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    106013 |  193 | `	pEntry = pHash->pList;` |
|     56853 |  194 | `	for(;;){` |
|    113711 |  195 | `		if( pHash->nEntry == 0 ){` |
|    106013 |  196 | `			break;` |
|         - |  197 | `		}` |
|      7703 |  198 | `		pNext = pEntry->pNext;` |
|      7703 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      7703 |  200 | `		pEntry = pNext;` |
|      7703 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    106013 |  203 | `	if( pHash->apBucket ){` |
|    106013 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     53004 |  205 | `	}` |
|    106013 |  206 | `	pHash->apBucket = 0;` |
|    106013 |  207 | `	pHash->nBucketSize = 0;` |
|    106013 |  208 | `	pHash->pAllocator = 0;` |
|    106013 |  209 | `	return SXRET_OK;` |
|         5 |  210 | `}` |
|  17855704 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 | `{` |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  17855709 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  17855709 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  16130443 |  218 | `	for(;;){` |
|  32253323 |  219 | `		if( pEntry == 0 ){` |
|   9501867 |  220 | `			break;` |
|         - |  221 | `		}` |
|  26928132 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   8353852 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   8353847 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  14397619 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   9501867 |  229 | `	return 0;` |
|   8928367 |  230 | `}` |
|  18743920 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 | `{` |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  18743925 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    888431 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  17855499 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  17855499 |  244 | `	if( pEntry == 0 ){` |
|   9501867 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   8353637 |  247 | `	return (SyHashEntry *)pEntry;` |
|   9372475 |  248 | `}` |
|    131846 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 | `{` |
|         - |  251 | `	sxi32 rc;` |
|    131851 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    102089 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     51047 |  254 | `	}else{` |
|     29767 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    131851 |  257 | `	if( pEntry->pNextCollide ){` |
|      5095 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2547 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    131851 |  261 | `	if( pHash->pLast == pEntry ){` |
|    125543 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     62769 |  263 | `	}` |
|    131851 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    131851 |  265 | `	pHash->nEntry--;` |
|    131851 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    131851 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    131851 |  272 | `	return rc;` |
|         5 |  273 | `}` |
|       210 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 | `{` |
|         - |  276 | `	SyHashEntry_Pr *pEntry;` |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( INVALID_HASH(pHash) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|       215 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       215 |  284 | `	if( pEntry == 0 ){` |
|       ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|         - |  286 | `	}` |
|       215 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       215 |  288 | `	return rc;` |
|       110 |  289 | `}` |
|    131636 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 | `{` |
|    131641 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    131641 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    131641 |  300 | `	return rc;` |
|         5 |  301 | `}` |
|   1184764 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 | `{` |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1184769 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1184769 |  310 | `	return SXRET_OK;` |
|         5 |  311 | `}` |
|   7518704 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 | `{` |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   7518709 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1184507 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1184507 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   6334207 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   6334207 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   6334207 |  328 | `	return (SyHashEntry *)pEntry;` |
|   3759357 |  329 | `}` |
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
|      2001 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|      1991 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1991 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|      1991 |  348 | `		pEntry = pEntry->pNext;` |
|       996 |  349 | `	}` |
|        11 |  350 | `	return SXRET_OK;` |
|         6 |  351 | `}` |
|     30846 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  353 | `{` |
|     30851 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  355 | `	SyHashEntry_Pr *pEntry;` |
|         - |  356 | `	SyHashEntry_Pr **apNew;` |
|         - |  357 | `	sxu32 n,iBucket;` |
|         - |  358 |  |
|         - |  359 | `	/* Allocate a new larger table */` |
|     30851 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     30851 |  361 | `	if( apNew == 0 ){` |
|         - |  362 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  363 | `		return SXRET_OK;` |
|         - |  364 | `	}` |
|         - |  365 | `	/* Zero the new table */` |
|     30851 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  367 | `	/* Rehash all entries */` |
|   3889859 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3859013 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  370 | `		/* Install in the new bucket */` |
|   3859013 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3859013 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3859013 |  373 | `		if( apNew[iBucket] != 0 ){` |
|   1852219 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    925971 |  375 | `		}` |
|   3859013 |  376 | `		apNew[iBucket] = pEntry;` |
|         - |  377 | `		/* Point to the next entry */` |
|   3859013 |  378 | `		pEntry = pEntry->pNext;` |
|   1929509 |  379 | `	}` |
|         - |  380 | `	/* Release the old table and reflect the change */` |
|     30851 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     30851 |  382 | `	pHash->apBucket = apNew;` |
|     30851 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     30851 |  384 | `	return SXRET_OK;` |
|     15428 |  385 | `}` |
|   5083780 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 | `{` |
|   5083785 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   5083785 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   5083785 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2876480 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1438288 |  393 | `	}` |
|   5083785 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   5083785 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        51 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        51 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        51 |  401 | `		pHash->pLast = pEntry;` |
|        26 |  402 | `	}else{` |
|   5083735 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   5083785 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    317339 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    317339 |  408 | `		pHash->pLast = pEntry;` |
|    158667 |  409 | `	}` |
|   5083785 |  410 | `	pHash->nEntry++;` |
|   5083785 |  411 | `	return SXRET_OK;` |
|         5 |  412 | `}` |
|   5083780 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 | `{` |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   5083785 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     30851 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     30851 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     15423 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   5083785 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   5083785 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   5083785 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   5083785 |  435 | `	pEntry->pHash = pHash;` |
|   5083785 |  436 | `	pEntry->pKey = pKey;` |
|   5083785 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   5083785 |  438 | `	pEntry->pUserData = pUserData;` |
|   5083785 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   5083785 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   5083785 |  442 | `	return rc;` |
|   2541895 |  443 | `}` |
|   5083664 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 | `{` |
|   5083669 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|    168816 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 | `{` |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    168821 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 | `}` |
|         - |  468 |  |
