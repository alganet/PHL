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
|  19608132 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 |  |
|  19608137 |   16 | `	pSet->nSize = 0 ;` |
|  19608137 |   17 | `	pSet->nUsed = 0;` |
|  19608137 |   18 | `	pSet->nCursor = 0;` |
|  19608137 |   19 | `	pSet->eSize = ElemSize;` |
|  19608137 |   20 | `	pSet->pAllocator = pAllocator;` |
|  19608137 |   21 | `	pSet->pBase =  0;` |
|  19608137 |   22 | `	pSet->pUserData = 0;` |
|  19608137 |   23 | `	return SXRET_OK;` |
|         5 |   24 |  |
|  32385347 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  32385352 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4643313 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4643313 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4483669 |   34 | `			pSet->nSize = 4;` |
|   2241832 |   35 | `		}` |
|   4643313 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4643313 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4643313 |   40 | `		pSet->pBase = pNew;` |
|   4643313 |   41 | `		pSet->nSize <<= 1;` |
|   2321654 |   42 | `	}` |
|  32385352 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 242693828 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  32385352 |   45 | `	pSet->nUsed++;` |
|  32385352 |   46 | `	return SXRET_OK;` |
|  16192721 |   47 |  |
|   1330206 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 |  |
|   1330211 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1330211 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1330211 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1330211 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1330211 |   60 | `	pSet->nSize = nItem;` |
|   1330211 |   61 | `	return SXRET_OK;` |
|    665108 |   62 |  |
|   1838971 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 |  |
|   1838976 |   65 | `	pSet->nUsed   = 0;` |
|   1838976 |   66 | `	pSet->nCursor = 0;` |
|   1838976 |   67 | `	return SXRET_OK;` |
|         5 |   68 |  |
|     58260 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 |  |
|     58265 |   71 | `	pSet->nCursor = 0;` |
|     58265 |   72 | `	return SXRET_OK;` |
|         5 |   73 |  |
|     62464 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     62469 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     24133 |   79 | `		pSet->nCursor = 0;` |
|     24133 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     38341 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     38341 |   83 | `	if( ppEntry ){` |
|     38341 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     19168 |   85 | `	}` |
|     38341 |   86 | `	pSet->nCursor++;` |
|     38341 |   87 | `	return SXRET_OK;` |
|     31237 |   88 |  |
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
|    223758 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 |  |
|    223763 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       141 |  103 | `		pSet->nUsed = nNewSize;` |
|        68 |  104 | `	}` |
|    223763 |  105 | `	return SXRET_OK;` |
|         5 |  106 |  |
|  10122452 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 |  |
|  10122457 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10122457 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5078845 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2539420 |  112 | `	}` |
|  10122457 |  113 | `	pSet->pBase = 0;` |
|  10122457 |  114 | `	pSet->nUsed = 0;` |
|  10122457 |  115 | `	pSet->nCursor = 0;` |
|  10122457 |  116 | `	return rc;` |
|         5 |  117 |  |
|   5869886 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5869891 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       133 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5869763 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5869763 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2934948 |  126 |  |
|   3615212 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3615217 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2183021 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1432201 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1432201 |  135 | `	pSet->nUsed--;` |
|   1432201 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1432201 |  137 | `	return pData;` |
|   1807611 |  138 |  |
|  13569307 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  13569312 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13569312 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13569312 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6785025 |  148 |  |
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
|    586146 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    586151 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    586151 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    586151 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    586151 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    586151 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    586151 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    586151 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    586151 |  180 | `	pHash->nEntry = 0;` |
|    586151 |  181 | `	pHash->apBucket = apNew;` |
|    586151 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    586151 |  183 | `	return SXRET_OK;` |
|    293078 |  184 |  |
|    105750 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    105755 |  193 | `	pEntry = pHash->pList;` |
|     56724 |  194 | `	for(;;){` |
|    113453 |  195 | `		if( pHash->nEntry == 0 ){` |
|    105755 |  196 | `			break;` |
|         - |  197 | `		}` |
|      7703 |  198 | `		pNext = pEntry->pNext;` |
|      7703 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      7703 |  200 | `		pEntry = pNext;` |
|      7703 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    105755 |  203 | `	if( pHash->apBucket ){` |
|    105755 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     52875 |  205 | `	}` |
|    105755 |  206 | `	pHash->apBucket = 0;` |
|    105755 |  207 | `	pHash->nBucketSize = 0;` |
|    105755 |  208 | `	pHash->pAllocator = 0;` |
|    105755 |  209 | `	return SXRET_OK;` |
|         5 |  210 |  |
|  17770250 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  17770255 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  17770255 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  16035541 |  218 | `	for(;;){` |
|  31973532 |  219 | `		if( pEntry == 0 ){` |
|   9456951 |  220 | `			break;` |
|         - |  221 | `		}` |
|  26672988 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   8313314 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   8313309 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  14203282 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   9456951 |  229 | `	return 0;` |
|   8885640 |  230 |  |
|  18654228 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  18654233 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    884193 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  17770045 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  17770045 |  244 | `	if( pEntry == 0 ){` |
|   9456951 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   8313099 |  247 | `	return (SyHashEntry *)pEntry;` |
|   9327629 |  248 |  |
|    131518 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    131523 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    101833 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     50919 |  254 | `	}else{` |
|     29695 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    131523 |  257 | `	if( pEntry->pNextCollide ){` |
|      5097 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2548 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    131523 |  261 | `	if( pHash->pLast == pEntry ){` |
|    125215 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     62605 |  263 | `	}` |
|    131523 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    131523 |  265 | `	pHash->nEntry--;` |
|    131523 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    131523 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    131523 |  272 | `	return rc;` |
|         5 |  273 |  |
|       210 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 |  |
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
|       110 |  289 |  |
|    131308 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 |  |
|    131313 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    131313 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    131313 |  300 | `	return rc;` |
|         5 |  301 |  |
|   1179768 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 |  |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1179773 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1179773 |  310 | `	return SXRET_OK;` |
|         5 |  311 |  |
|   7488154 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 |  |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   7488159 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1179511 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1179511 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   6308653 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   6308653 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   6308653 |  328 | `	return (SyHashEntry *)pEntry;` |
|   3744082 |  329 |  |
|        10 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         1 |  331 |  |
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
|         6 |  351 |  |
|     30704 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  353 |  |
|     30709 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  355 | `	SyHashEntry_Pr *pEntry;` |
|         - |  356 | `	SyHashEntry_Pr **apNew;` |
|         - |  357 | `	sxu32 n,iBucket;` |
|         - |  358 |  |
|         - |  359 | `	/* Allocate a new larger table */` |
|     30709 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     30709 |  361 | `	if( apNew == 0 ){` |
|         - |  362 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  363 | `		return SXRET_OK;` |
|         - |  364 | `	}` |
|         - |  365 | `	/* Zero the new table */` |
|     30709 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  367 | `	/* Rehash all entries */` |
|   3872533 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3841829 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  370 | `		/* Install in the new bucket */` |
|   3841829 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3841829 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3841829 |  373 | `		if( apNew[iBucket] != 0 ){` |
|   1843793 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    921916 |  375 | `		}` |
|   3841829 |  376 | `		apNew[iBucket] = pEntry;` |
|         - |  377 | `		/* Point to the next entry */` |
|   3841829 |  378 | `		pEntry = pEntry->pNext;` |
|   1920917 |  379 | `	}` |
|         - |  380 | `	/* Release the old table and reflect the change */` |
|     30709 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     30709 |  382 | `	pHash->apBucket = apNew;` |
|     30709 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     30709 |  384 | `	return SXRET_OK;` |
|     15357 |  385 |  |
|   5060358 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 |  |
|   5060363 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   5060363 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   5060363 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2863598 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1431814 |  393 | `	}` |
|   5060363 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   5060363 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        51 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        51 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        51 |  401 | `		pHash->pLast = pEntry;` |
|        26 |  402 | `	}else{` |
|   5060313 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   5060363 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    315823 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    315823 |  408 | `		pHash->pLast = pEntry;` |
|    157909 |  409 | `	}` |
|   5060363 |  410 | `	pHash->nEntry++;` |
|   5060363 |  411 | `	return SXRET_OK;` |
|         5 |  412 |  |
|   5060358 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 |  |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   5060363 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     30709 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     30709 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     15352 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   5060363 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   5060363 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   5060363 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   5060363 |  435 | `	pEntry->pHash = pHash;` |
|   5060363 |  436 | `	pEntry->pKey = pKey;` |
|   5060363 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   5060363 |  438 | `	pEntry->pUserData = pUserData;` |
|   5060363 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   5060363 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   5060363 |  442 | `	return rc;` |
|   2530184 |  443 |  |
|   5060242 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 |  |
|   5060247 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|         5 |  447 |  |
|         - |  448 | `/*` |
|         - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|         - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|         - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|         - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|         - |  453 | ` */` |
|       116 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  455 |  |
|       118 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|         2 |  457 |  |
|    168332 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 |  |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    168337 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 |  |
|         - |  468 |  |
