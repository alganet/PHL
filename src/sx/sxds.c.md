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
|  20739168 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 | `{` |
|  20739173 |   16 | `	pSet->nSize = 0 ;` |
|  20739173 |   17 | `	pSet->nUsed = 0;` |
|  20739173 |   18 | `	pSet->nCursor = 0;` |
|  20739173 |   19 | `	pSet->eSize = ElemSize;` |
|  20739173 |   20 | `	pSet->pAllocator = pAllocator;` |
|  20739173 |   21 | `	pSet->pBase =  0;` |
|  20739173 |   22 | `	pSet->pUserData = 0;` |
|  20739173 |   23 | `	return SXRET_OK;` |
|         5 |   24 | `}` |
|  34365925 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 | `{` |
|         - |   27 | `	unsigned char *zbase;` |
|  34365930 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4859373 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4859373 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4688535 |   34 | `			pSet->nSize = 4;` |
|   2344265 |   35 | `		}` |
|   4859373 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4859373 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4859373 |   40 | `		pSet->pBase = pNew;` |
|   4859373 |   41 | `		pSet->nSize <<= 1;` |
|   2429684 |   42 | `	}` |
|  34365930 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 256904856 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  34365930 |   45 | `	pSet->nUsed++;` |
|  34365930 |   46 | `	return SXRET_OK;` |
|  17183010 |   47 | `}` |
|   1429768 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 | `{` |
|   1429773 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1429773 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1429773 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1429773 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1429773 |   60 | `	pSet->nSize = nItem;` |
|   1429773 |   61 | `	return SXRET_OK;` |
|    714889 |   62 | `}` |
|   2300875 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 | `{` |
|   2300880 |   65 | `	pSet->nUsed   = 0;` |
|   2300880 |   66 | `	pSet->nCursor = 0;` |
|   2300880 |   67 | `	return SXRET_OK;` |
|         5 |   68 | `}` |
|     66460 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 | `{` |
|     66465 |   71 | `	pSet->nCursor = 0;` |
|     66465 |   72 | `	return SXRET_OK;` |
|         5 |   73 | `}` |
|     70518 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 | `{` |
|         - |   76 | `	register unsigned char *zSrc;` |
|     70523 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     28561 |   79 | `		pSet->nCursor = 0;` |
|     28561 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     41967 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     41967 |   83 | `	if( ppEntry ){` |
|     41967 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     20981 |   85 | `	}` |
|     41967 |   86 | `	pSet->nCursor++;` |
|     41967 |   87 | `	return SXRET_OK;` |
|     35264 |   88 | `}` |
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
|  10594750 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 | `{` |
|  10594755 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10594755 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5319221 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2659608 |  112 | `	}` |
|  10594755 |  113 | `	pSet->pBase = 0;` |
|  10594755 |  114 | `	pSet->nUsed = 0;` |
|  10594755 |  115 | `	pSet->nCursor = 0;` |
|  10594755 |  116 | `	return rc;` |
|         5 |  117 | `}` |
|   6150610 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 | `{` |
|         - |  120 | `	const char *zBase;` |
|   6150615 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       133 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   6150487 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   6150487 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   3075310 |  126 | `}` |
|   3725924 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 | `{` |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3725929 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2193101 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1532833 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1532833 |  135 | `	pSet->nUsed--;` |
|   1532833 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1532833 |  137 | `	return pData;` |
|   1862967 |  138 | `}` |
|  14130296 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	const char *zBase;` |
|  14130301 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  14130301 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  14130301 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   7065518 |  148 | `}` |
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
|    681474 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 | `{` |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    681479 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    681479 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    681479 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    681479 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    681479 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    681479 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    681479 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    681479 |  180 | `	pHash->nEntry = 0;` |
|    681479 |  181 | `	pHash->apBucket = apNew;` |
|    681479 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    681479 |  183 | `	return SXRET_OK;` |
|    340742 |  184 | `}` |
|    154356 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 | `{` |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    154361 |  193 | `	pEntry = pHash->pList;` |
|     82562 |  194 | `	for(;;){` |
|    165129 |  195 | `		if( pHash->nEntry == 0 ){` |
|    154361 |  196 | `			break;` |
|         - |  197 | `		}` |
|     10773 |  198 | `		pNext = pEntry->pNext;` |
|     10773 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     10773 |  200 | `		pEntry = pNext;` |
|     10773 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    154361 |  203 | `	if( pHash->apBucket ){` |
|    154361 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     77178 |  205 | `	}` |
|    154361 |  206 | `	pHash->apBucket = 0;` |
|    154361 |  207 | `	pHash->nBucketSize = 0;` |
|    154361 |  208 | `	pHash->pAllocator = 0;` |
|    154361 |  209 | `	return SXRET_OK;` |
|         5 |  210 | `}` |
|  19961814 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 | `{` |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  19961819 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  19961819 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  18067234 |  218 | `	for(;;){` |
|  36162877 |  219 | `		if( pEntry == 0 ){` |
|  10159489 |  220 | `			break;` |
|         - |  221 | `		}` |
|  30904309 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   9802342 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   9802335 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  16201063 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|  10159489 |  229 | `	return 0;` |
|   9981422 |  230 | `}` |
|  20933156 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 | `{` |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  20933161 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    971571 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  19961595 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  19961595 |  244 | `	if( pEntry == 0 ){` |
|  10159489 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   9802111 |  247 | `	return (SyHashEntry *)pEntry;` |
|  10467093 |  248 | `}` |
|    177568 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 | `{` |
|         - |  251 | `	sxi32 rc;` |
|    177573 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    139003 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     69504 |  254 | `	}else{` |
|     38575 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    177573 |  257 | `	if( pEntry->pNextCollide ){` |
|      5144 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2571 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    177573 |  261 | `	if( pHash->pLast == pEntry ){` |
|    171171 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     85583 |  263 | `	}` |
|    177573 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    177573 |  265 | `	pHash->nEntry--;` |
|    177573 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    177573 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    177573 |  272 | `	return rc;` |
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
|    177344 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 | `{` |
|    177349 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    177349 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    177349 |  300 | `	return rc;` |
|         5 |  301 | `}` |
|   1330704 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 | `{` |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1330709 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1330709 |  310 | `	return SXRET_OK;` |
|         5 |  311 | `}` |
|   8354044 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 | `{` |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   8354049 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1330447 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1330447 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   7023607 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   7023607 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   7023607 |  328 | `	return (SyHashEntry *)pEntry;` |
|   4177027 |  329 | `}` |
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
|   1983985 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    991980 |  375 | `		}` |
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
|   5585872 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 | `{` |
|   5585877 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   5585877 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   5585877 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   3123350 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1561651 |  393 | `	}` |
|   5585877 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   5585877 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        51 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        51 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        51 |  401 | `		pHash->pLast = pEntry;` |
|        26 |  402 | `	}else{` |
|   5585827 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   5585877 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    360997 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    360997 |  408 | `		pHash->pLast = pEntry;` |
|    180496 |  409 | `	}` |
|   5585877 |  410 | `	pHash->nEntry++;` |
|   5585877 |  411 | `	return SXRET_OK;` |
|         5 |  412 | `}` |
|   5585872 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 | `{` |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   5585877 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     33059 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     33059 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     16527 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   5585877 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   5585877 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   5585877 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   5585877 |  435 | `	pEntry->pHash = pHash;` |
|   5585877 |  436 | `	pEntry->pKey = pKey;` |
|   5585877 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   5585877 |  438 | `	pEntry->pUserData = pUserData;` |
|   5585877 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   5585877 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   5585877 |  442 | `	return rc;` |
|   2792941 |  443 | `}` |
|   5585756 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 | `{` |
|   5585761 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|    217404 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 | `{` |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    217409 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 | `}` |
|         - |  468 |  |
