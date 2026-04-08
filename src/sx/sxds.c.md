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
|  14614246 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  14614248 |   16 | `	pSet->nSize = 0 ;` |
|  14614248 |   17 | `	pSet->nUsed = 0;` |
|  14614248 |   18 | `	pSet->nCursor = 0;` |
|  14614248 |   19 | `	pSet->eSize = ElemSize;` |
|  14614248 |   20 | `	pSet->pAllocator = pAllocator;` |
|  14614248 |   21 | `	pSet->pBase =  0;` |
|  14614248 |   22 | `	pSet->pUserData = 0;` |
|  14614248 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  24424672 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  24424674 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3905364 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3905364 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3790108 |   34 | `			pSet->nSize = 4;` |
|   1895053 |   35 | `		}` |
|   3905364 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3905364 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3905364 |   40 | `		pSet->pBase = pNew;` |
|   3905364 |   41 | `		pSet->nSize <<= 1;` |
|   1952681 |   42 | `	}` |
|  24424674 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 181852738 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  24424674 |   45 | `	pSet->nUsed++;` |
|  24424674 |   46 | `	return SXRET_OK;` |
|  12212360 |   47 |  |
|    911650 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    911652 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    911652 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    911652 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    911652 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    911652 |   60 | `	pSet->nSize = nItem;` |
|    911652 |   61 | `	return SXRET_OK;` |
|    455827 |   62 |  |
|   1340440 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1340442 |   65 | `	pSet->nUsed   = 0;` |
|   1340442 |   66 | `	pSet->nCursor = 0;` |
|   1340442 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     43576 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     43578 |   71 | `	pSet->nCursor = 0;` |
|     43578 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     47508 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     47510 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     17970 |   79 | `		pSet->nCursor = 0;` |
|     17970 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     29542 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     29542 |   83 | `	if( ppEntry ){` |
|     29542 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     14770 |   85 | `	}` |
|     29542 |   86 | `	pSet->nCursor++;` |
|     29542 |   87 | `	return SXRET_OK;` |
|     23756 |   88 |  |
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
|    152734 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    152736 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|    152736 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8477466 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8477468 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8477468 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4329330 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2164664 |  112 | `	}` |
|   8477468 |  113 | `	pSet->pBase = 0;` |
|   8477468 |  114 | `	pSet->nUsed = 0;` |
|   8477468 |  115 | `	pSet->nCursor = 0;` |
|   8477468 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4740266 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4740268 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4740178 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4740178 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2370135 |  126 |  |
|   3257800 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3257802 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2146400 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1111404 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1111404 |  135 | `	pSet->nUsed--;` |
|   1111404 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1111404 |  137 | `	return pData;` |
|   1628902 |  138 |  |
|  10334090 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  10334092 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  10334092 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  10334092 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5167266 |  148 |  |
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
|    195664 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    195666 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    195666 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    195666 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    195666 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    195666 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    195666 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    195666 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    195666 |  180 | `	pHash->nEntry = 0;` |
|    195666 |  181 | `	pHash->apBucket = apNew;` |
|    195666 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    195666 |  183 | `	return SXRET_OK;` |
|     97834 |  184 |  |
|     30644 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     30646 |  193 | `	pEntry = pHash->pList;` |
|     17061 |  194 | `	for(;;){` |
|     34124 |  195 | `		if( pHash->nEntry == 0 ){` |
|     30646 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3480 |  198 | `		pNext = pEntry->pNext;` |
|      3480 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3480 |  200 | `		pEntry = pNext;` |
|      3480 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     30646 |  203 | `	if( pHash->apBucket ){` |
|     30646 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     15322 |  205 | `	}` |
|     30646 |  206 | `	pHash->apBucket = 0;` |
|     30646 |  207 | `	pHash->nBucketSize = 0;` |
|     30646 |  208 | `	pHash->pAllocator = 0;` |
|     30646 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  11761500 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  11761502 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  11761502 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  10818405 |  218 | `	for(;;){` |
|  21528531 |  219 | `		if( pEntry == 0 ){` |
|   6552724 |  220 | `			break;` |
|         - |  221 | `		}` |
|  17580068 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5208782 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5208780 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   9767031 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   6552724 |  229 | `	return 0;` |
|   5881016 |  230 |  |
|  11877796 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  11877798 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    116320 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  11761480 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  11761480 |  244 | `	if( pEntry == 0 ){` |
|   6552724 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5208758 |  247 | `	return (SyHashEntry *)pEntry;` |
|   5939164 |  248 |  |
|     86544 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     86546 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     66136 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     33069 |  254 | `	}else{` |
|     20412 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     86546 |  257 | `	if( pEntry->pNextCollide ){` |
|      4365 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2182 |  259 | `	}` |
|     86546 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     86546 |  261 | `	pHash->nEntry--;` |
|     86546 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     86546 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     86546 |  268 | `	return rc;` |
|         2 |  269 |  |
|        22 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|        24 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        24 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|        24 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        24 |  284 | `	return rc;` |
|        13 |  285 |  |
|     86522 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     86524 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     86524 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     86524 |  296 | `	return rc;` |
|         2 |  297 |  |
|    280476 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    280478 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    280478 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2115724 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2115726 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    280044 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    280044 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   1835684 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   1835684 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   1835684 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1057864 |  325 |  |
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
|      1761 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1751 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1751 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1751 |  344 | `		pEntry = pEntry->pNext;` |
|       876 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     24130 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     24132 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     24132 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     24132 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     24132 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3065892 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3041762 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3041762 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3041762 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3041762 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1453528 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    726742 |  371 | `		}` |
|   3041762 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3041762 |  374 | `		pEntry = pEntry->pNext;` |
|   1520882 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     24132 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     24132 |  378 | `	pHash->apBucket = apNew;` |
|     24132 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     24132 |  380 | `	return SXRET_OK;` |
|     12067 |  381 |  |
|   2994868 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2994870 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2994870 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2994870 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1999379 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    999674 |  389 | `	}` |
|   2994870 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2994870 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2994870 |  393 | `	if( pHash->nEntry == 0 ){` |
|    121682 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     60840 |  395 | `	}` |
|   2994870 |  396 | `	pHash->nEntry++;` |
|   2994870 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2994868 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2994870 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     24132 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     24132 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     12065 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2994870 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2994870 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2994870 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2994870 |  421 | `	pEntry->pHash = pHash;` |
|   2994870 |  422 | `	pEntry->pKey = pKey;` |
|   2994870 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2994870 |  424 | `	pEntry->pUserData = pUserData;` |
|   2994870 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2994870 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2994870 |  428 | `	return rc;` |
|   1497436 |  429 |  |
|    113804 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    113806 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
