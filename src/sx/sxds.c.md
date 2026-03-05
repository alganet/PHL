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
|   9928874 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|   9928876 |   16 | `	pSet->nSize = 0 ;` |
|   9928876 |   17 | `	pSet->nUsed = 0;` |
|   9928876 |   18 | `	pSet->nCursor = 0;` |
|   9928876 |   19 | `	pSet->eSize = ElemSize;` |
|   9928876 |   20 | `	pSet->pAllocator = pAllocator;` |
|   9928876 |   21 | `	pSet->pBase =  0;` |
|   9928876 |   22 | `	pSet->pUserData = 0;` |
|   9928876 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  15662114 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  15662116 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3240524 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3240524 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3180688 |   34 | `			pSet->nSize = 4;` |
|   1590343 |   35 | `		}` |
|   3240524 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3240524 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3240524 |   40 | `		pSet->pBase = pNew;` |
|   3240524 |   41 | `		pSet->nSize <<= 1;` |
|   1620261 |   42 | `	}` |
|  15662116 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 118312312 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  15662116 |   45 | `	pSet->nUsed++;` |
|  15662116 |   46 | `	return SXRET_OK;` |
|   7831081 |   47 |  |
|    428460 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    428462 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    428462 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    428462 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    428462 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    428462 |   60 | `	pSet->nSize = nItem;` |
|    428462 |   61 | `	return SXRET_OK;` |
|    214232 |   62 |  |
|    846288 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|    846290 |   65 | `	pSet->nUsed   = 0;` |
|    846290 |   66 | `	pSet->nCursor = 0;` |
|    846290 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     34438 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     34440 |   71 | `	pSet->nCursor = 0;` |
|     34440 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     37738 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     37740 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     13798 |   79 | `		pSet->nCursor = 0;` |
|     13798 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     23944 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     23944 |   83 | `	if( ppEntry ){` |
|     23944 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     11971 |   85 | `	}` |
|     23944 |   86 | `	pSet->nCursor++;` |
|     23944 |   87 | `	return SXRET_OK;` |
|     18871 |   88 |  |
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
|     54260 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     54262 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     54262 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   6717754 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   6717756 |  109 | `	sxi32 rc = SXRET_OK;` |
|   6717756 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3449188 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1724593 |  112 | `	}` |
|   6717756 |  113 | `	pSet->pBase = 0;` |
|   6717756 |  114 | `	pSet->nUsed = 0;` |
|   6717756 |  115 | `	pSet->nCursor = 0;` |
|   6717756 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3278156 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3278158 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3278068 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3278068 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1639080 |  126 |  |
|   2931282 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   2931284 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2121562 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    809724 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    809724 |  135 | `	pSet->nUsed--;` |
|    809724 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    809724 |  137 | `	return pData;` |
|   1465643 |  138 |  |
|   8412818 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   8412820 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   8412820 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   8412820 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4206625 |  148 |  |
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
|     77116 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     77118 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     77118 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     77118 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     77118 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     77118 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     77118 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     77118 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     77118 |  180 | `	pHash->nEntry = 0;` |
|     77118 |  181 | `	pHash->apBucket = apNew;` |
|     77118 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     77118 |  183 | `	return SXRET_OK;` |
|     38560 |  184 |  |
|      9798 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|      9800 |  193 | `	pEntry = pHash->pList;` |
|      5691 |  194 | `	for(;;){` |
|     11384 |  195 | `		if( pHash->nEntry == 0 ){` |
|      9800 |  196 | `			break;` |
|         - |  197 | `		}` |
|      1586 |  198 | `		pNext = pEntry->pNext;` |
|      1586 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      1586 |  200 | `		pEntry = pNext;` |
|      1586 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|      9800 |  203 | `	if( pHash->apBucket ){` |
|      9800 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      4899 |  205 | `	}` |
|      9800 |  206 | `	pHash->apBucket = 0;` |
|      9800 |  207 | `	pHash->nBucketSize = 0;` |
|      9800 |  208 | `	pHash->pAllocator = 0;` |
|      9800 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   7819104 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   7819106 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   7819106 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   6825313 |  218 | `	for(;;){` |
|  13570091 |  219 | `		if( pEntry == 0 ){` |
|   4234132 |  220 | `			break;` |
|         - |  221 | `		}` |
|  11128318 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   3584978 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   3584976 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   5750987 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   4234132 |  229 | `	return 0;` |
|   3909818 |  230 |  |
|   7862694 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   7862696 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     43598 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   7819100 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   7819100 |  244 | `	if( pEntry == 0 ){` |
|   4234132 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   3584970 |  247 | `	return (SyHashEntry *)pEntry;` |
|   3931613 |  248 |  |
|     62820 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     62822 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     47034 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     23518 |  254 | `	}else{` |
|     15790 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     62822 |  257 | `	if( pEntry->pNextCollide ){` |
|      3685 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      1842 |  259 | `	}` |
|     62822 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     62822 |  261 | `	pHash->nEntry--;` |
|     62822 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     62822 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     62822 |  268 | `	return rc;` |
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
|     62814 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     62816 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     62816 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     62816 |  296 | `	return rc;` |
|         2 |  297 |  |
|    112796 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    112798 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    112798 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    786196 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    786198 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    112364 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    112364 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    673836 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    673836 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    673836 |  324 | `	return (SyHashEntry *)pEntry;` |
|    393100 |  325 |  |
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
|      1579 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1569 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1569 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1569 |  344 | `		pEntry = pEntry->pNext;` |
|       785 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     10860 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     10862 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     10862 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     10862 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     10862 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1486862 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1476002 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1476002 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1476002 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1476002 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    708798 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    354393 |  371 | `		}` |
|   1476002 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1476002 |  374 | `		pEntry = pEntry->pNext;` |
|    738002 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     10862 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     10862 |  378 | `	pHash->apBucket = apNew;` |
|     10862 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     10862 |  380 | `	return SXRET_OK;` |
|      5432 |  381 |  |
|   1342030 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1342032 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1342032 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1342032 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    893912 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    446993 |  389 | `	}` |
|   1342032 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1342032 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1342032 |  393 | `	if( pHash->nEntry == 0 ){` |
|     55278 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     27638 |  395 | `	}` |
|   1342032 |  396 | `	pHash->nEntry++;` |
|   1342032 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1342030 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1342032 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     10862 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     10862 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      5430 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1342032 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1342032 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1342032 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1342032 |  421 | `	pEntry->pHash = pHash;` |
|   1342032 |  422 | `	pEntry->pKey = pKey;` |
|   1342032 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1342032 |  424 | `	pEntry->pUserData = pUserData;` |
|   1342032 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1342032 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1342032 |  428 | `	return rc;` |
|    671017 |  429 |  |
|     76508 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     76510 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
