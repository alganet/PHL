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
|  20737868 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 | `{` |
|  20737873 |   16 | `	pSet->nSize = 0 ;` |
|  20737873 |   17 | `	pSet->nUsed = 0;` |
|  20737873 |   18 | `	pSet->nCursor = 0;` |
|  20737873 |   19 | `	pSet->eSize = ElemSize;` |
|  20737873 |   20 | `	pSet->pAllocator = pAllocator;` |
|  20737873 |   21 | `	pSet->pBase =  0;` |
|  20737873 |   22 | `	pSet->pUserData = 0;` |
|  20737873 |   23 | `	return SXRET_OK;` |
|         5 |   24 | `}` |
|  34364513 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 | `{` |
|         - |   27 | `	unsigned char *zbase;` |
|  34364518 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4858941 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4858941 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4688107 |   34 | `			pSet->nSize = 4;` |
|   2344051 |   35 | `		}` |
|   4858941 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4858941 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4858941 |   40 | `		pSet->pBase = pNew;` |
|   4858941 |   41 | `		pSet->nSize <<= 1;` |
|   2429468 |   42 | `	}` |
|  34364518 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 256898892 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  34364518 |   45 | `	pSet->nUsed++;` |
|  34364518 |   46 | `	return SXRET_OK;` |
|  17182305 |   47 | `}` |
|   1429748 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 | `{` |
|   1429753 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1429753 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1429753 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1429753 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1429753 |   60 | `	pSet->nSize = nItem;` |
|   1429753 |   61 | `	return SXRET_OK;` |
|    714879 |   62 | `}` |
|   2300627 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 | `{` |
|   2300632 |   65 | `	pSet->nUsed   = 0;` |
|   2300632 |   66 | `	pSet->nCursor = 0;` |
|   2300632 |   67 | `	return SXRET_OK;` |
|         5 |   68 | `}` |
|     66448 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 | `{` |
|     66453 |   71 | `	pSet->nCursor = 0;` |
|     66453 |   72 | `	return SXRET_OK;` |
|         5 |   73 | `}` |
|     70506 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 | `{` |
|         - |   76 | `	register unsigned char *zSrc;` |
|     70511 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     28557 |   79 | `		pSet->nCursor = 0;` |
|     28557 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     41959 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     41959 |   83 | `	if( ppEntry ){` |
|     41959 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     20977 |   85 | `	}` |
|     41959 |   86 | `	pSet->nCursor++;` |
|     41959 |   87 | `	return SXRET_OK;` |
|     35258 |   88 | `}` |
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
|    240824 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 | `{` |
|    240829 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       677 |  103 | `		pSet->nUsed = nNewSize;` |
|       336 |  104 | `	}` |
|    240829 |  105 | `	return SXRET_OK;` |
|         5 |  106 | `}` |
|  10593894 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 | `{` |
|  10593899 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10593899 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5318775 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2659385 |  112 | `	}` |
|  10593899 |  113 | `	pSet->pBase = 0;` |
|  10593899 |  114 | `	pSet->nUsed = 0;` |
|  10593899 |  115 | `	pSet->nCursor = 0;` |
|  10593899 |  116 | `	return rc;` |
|         5 |  117 | `}` |
|   6150580 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 | `{` |
|         - |  120 | `	const char *zBase;` |
|   6150585 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       133 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   6150457 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   6150457 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   3075295 |  126 | `}` |
|   3725514 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 | `{` |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3725519 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2193099 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1532425 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1532425 |  135 | `	pSet->nUsed--;` |
|   1532425 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1532425 |  137 | `	return pData;` |
|   1862762 |  138 | `}` |
|  14127745 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	const char *zBase;` |
|  14127750 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  14127750 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  14127750 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   7064251 |  148 | `}` |
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
|    681456 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 | `{` |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    681461 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    681461 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    681461 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    681461 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    681461 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    681461 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    681461 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    681461 |  180 | `	pHash->nEntry = 0;` |
|    681461 |  181 | `	pHash->apBucket = apNew;` |
|    681461 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    681461 |  183 | `	return SXRET_OK;` |
|    340733 |  184 | `}` |
|    154338 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 | `{` |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    154343 |  193 | `	pEntry = pHash->pList;` |
|     82553 |  194 | `	for(;;){` |
|    165111 |  195 | `		if( pHash->nEntry == 0 ){` |
|    154343 |  196 | `			break;` |
|         - |  197 | `		}` |
|     10773 |  198 | `		pNext = pEntry->pNext;` |
|     10773 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     10773 |  200 | `		pEntry = pNext;` |
|     10773 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    154343 |  203 | `	if( pHash->apBucket ){` |
|    154343 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     77169 |  205 | `	}` |
|    154343 |  206 | `	pHash->apBucket = 0;` |
|    154343 |  207 | `	pHash->nBucketSize = 0;` |
|    154343 |  208 | `	pHash->pAllocator = 0;` |
|    154343 |  209 | `	return SXRET_OK;` |
|         5 |  210 | `}` |
|  19960218 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 | `{` |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  19960223 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  19960223 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  18137162 |  218 | `	for(;;){` |
|  36215373 |  219 | `		if( pEntry == 0 ){` |
|  10158679 |  220 | `			break;` |
|         - |  221 | `		}` |
|  30957216 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   9801556 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   9801549 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  16255155 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|  10158679 |  229 | `	return 0;` |
|   9980636 |  230 | `}` |
|  20931550 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 | `{` |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  20931555 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    971561 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  19959999 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  19959999 |  244 | `	if( pEntry == 0 ){` |
|  10158679 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   9801325 |  247 | `	return (SyHashEntry *)pEntry;` |
|  10466302 |  248 | `}` |
|    177550 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 | `{` |
|         - |  251 | `	sxi32 rc;` |
|    177555 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    138993 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     69499 |  254 | `	}else{` |
|     38567 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    177555 |  257 | `	if( pEntry->pNextCollide ){` |
|      5144 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2571 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    177555 |  261 | `	if( pHash->pLast == pEntry ){` |
|    171153 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     85574 |  263 | `	}` |
|    177555 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    177555 |  265 | `	pHash->nEntry--;` |
|    177555 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    177555 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    177555 |  272 | `	return rc;` |
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
|    177326 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 | `{` |
|    177331 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    177331 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    177331 |  300 | `	return rc;` |
|         5 |  301 | `}` |
|   1330648 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 | `{` |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1330653 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1330653 |  310 | `	return SXRET_OK;` |
|         5 |  311 | `}` |
|   8353696 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 | `{` |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   8353701 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1330391 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1330391 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   7023315 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   7023315 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   7023315 |  328 | `	return (SyHashEntry *)pEntry;` |
|   4176853 |  329 | `}` |
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
|      2083 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|      2073 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      2073 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|      2073 |  348 | `		pEntry = pEntry->pNext;` |
|      1037 |  349 | `	}` |
|        11 |  350 | `	return SXRET_OK;` |
|         6 |  351 | `}` |
|     33054 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  353 | `{` |
|     33059 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  355 | `	SyHashEntry_Pr *pEntry;` |
|         - |  356 | `	SyHashEntry_Pr **apNew;` |
|         - |  357 | `	sxu32 n,iBucket;` |
|         - |  358 |  |
|         - |  359 | `	/* Allocate a new larger table */` |
|     33059 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     33059 |  361 | `	if( apNew == 0 ){` |
|         - |  362 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  363 | `		return SXRET_OK;` |
|         - |  364 | `	}` |
|         - |  365 | `	/* Zero the new table */` |
|     33059 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  367 | `	/* Rehash all entries */` |
|   4166723 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   4133669 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  370 | `		/* Install in the new bucket */` |
|   4133669 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   4133669 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   4133669 |  373 | `		if( apNew[iBucket] != 0 ){` |
|   1984004 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    991944 |  375 | `		}` |
|   4133669 |  376 | `		apNew[iBucket] = pEntry;` |
|         - |  377 | `		/* Point to the next entry */` |
|   4133669 |  378 | `		pEntry = pEntry->pNext;` |
|   2066837 |  379 | `	}` |
|         - |  380 | `	/* Release the old table and reflect the change */` |
|     33059 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     33059 |  382 | `	pHash->apBucket = apNew;` |
|     33059 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     33059 |  384 | `	return SXRET_OK;` |
|     16532 |  385 | `}` |
|   5585854 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 | `{` |
|   5585859 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   5585859 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   5585859 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   3123034 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1561531 |  393 | `	}` |
|   5585859 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   5585859 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        51 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        51 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        51 |  401 | `		pHash->pLast = pEntry;` |
|        26 |  402 | `	}else{` |
|   5585809 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   5585859 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    360995 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    360995 |  408 | `		pHash->pLast = pEntry;` |
|    180495 |  409 | `	}` |
|   5585859 |  410 | `	pHash->nEntry++;` |
|   5585859 |  411 | `	return SXRET_OK;` |
|         5 |  412 | `}` |
|   5585854 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 | `{` |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   5585859 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     33059 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     33059 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     16527 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   5585859 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   5585859 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   5585859 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   5585859 |  435 | `	pEntry->pHash = pHash;` |
|   5585859 |  436 | `	pEntry->pKey = pKey;` |
|   5585859 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   5585859 |  438 | `	pEntry->pUserData = pUserData;` |
|   5585859 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   5585859 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   5585859 |  442 | `	return rc;` |
|   2792932 |  443 | `}` |
|   5585738 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 | `{` |
|   5585743 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|    217386 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 | `{` |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    217391 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 | `}` |
|         - |  468 |  |
