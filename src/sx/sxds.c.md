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
|  14604632 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  14604634 |   16 | `	pSet->nSize = 0 ;` |
|  14604634 |   17 | `	pSet->nUsed = 0;` |
|  14604634 |   18 | `	pSet->nCursor = 0;` |
|  14604634 |   19 | `	pSet->eSize = ElemSize;` |
|  14604634 |   20 | `	pSet->pAllocator = pAllocator;` |
|  14604634 |   21 | `	pSet->pBase =  0;` |
|  14604634 |   22 | `	pSet->pUserData = 0;` |
|  14604634 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  24411030 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  24411032 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3903164 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3903164 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3787978 |   34 | `			pSet->nSize = 4;` |
|   1893988 |   35 | `		}` |
|   3903164 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3903164 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3903164 |   40 | `		pSet->pBase = pNew;` |
|   3903164 |   41 | `		pSet->nSize <<= 1;` |
|   1951581 |   42 | `	}` |
|  24411032 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 181779236 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  24411032 |   45 | `	pSet->nUsed++;` |
|  24411032 |   46 | `	return SXRET_OK;` |
|  12205539 |   47 |  |
|    911242 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    911244 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    911244 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    911244 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    911244 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    911244 |   60 | `	pSet->nSize = nItem;` |
|    911244 |   61 | `	return SXRET_OK;` |
|    455623 |   62 |  |
|   1337972 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1337974 |   65 | `	pSet->nUsed   = 0;` |
|   1337974 |   66 | `	pSet->nCursor = 0;` |
|   1337974 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     43398 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     43400 |   71 | `	pSet->nCursor = 0;` |
|     43400 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     47330 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     47332 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     17898 |   79 | `		pSet->nCursor = 0;` |
|     17898 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     29436 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     29436 |   83 | `	if( ppEntry ){` |
|     29436 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     14717 |   85 | `	}` |
|     29436 |   86 | `	pSet->nCursor++;` |
|     29436 |   87 | `	return SXRET_OK;` |
|     23667 |   88 |  |
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
|    152728 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    152730 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|    152730 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8473152 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8473154 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8473154 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4326842 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2163420 |  112 | `	}` |
|   8473154 |  113 | `	pSet->pBase = 0;` |
|   8473154 |  114 | `	pSet->nUsed = 0;` |
|   8473154 |  115 | `	pSet->nCursor = 0;` |
|   8473154 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4738936 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4738938 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4738848 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4738848 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2369470 |  126 |  |
|   3255910 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3255912 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2146382 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1109532 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1109532 |  135 | `	pSet->nUsed--;` |
|   1109532 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1109532 |  137 | `	return pData;` |
|   1627957 |  138 |  |
|  10313878 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  10313880 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  10313880 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  10313880 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5157159 |  148 |  |
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
|    195600 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    195602 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    195602 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    195602 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    195602 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    195602 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    195602 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    195602 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    195602 |  180 | `	pHash->nEntry = 0;` |
|    195602 |  181 | `	pHash->apBucket = apNew;` |
|    195602 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    195602 |  183 | `	return SXRET_OK;` |
|     97802 |  184 |  |
|     30580 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     30582 |  193 | `	pEntry = pHash->pList;` |
|     17029 |  194 | `	for(;;){` |
|     34060 |  195 | `		if( pHash->nEntry == 0 ){` |
|     30582 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3480 |  198 | `		pNext = pEntry->pNext;` |
|      3480 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3480 |  200 | `		pEntry = pNext;` |
|      3480 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     30582 |  203 | `	if( pHash->apBucket ){` |
|     30582 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     15290 |  205 | `	}` |
|     30582 |  206 | `	pHash->apBucket = 0;` |
|     30582 |  207 | `	pHash->nBucketSize = 0;` |
|     30582 |  208 | `	pHash->pAllocator = 0;` |
|     30582 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  11743950 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  11743952 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  11743952 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  10714447 |  218 | `	for(;;){` |
|  21391606 |  219 | `		if( pEntry == 0 ){` |
|   6543880 |  220 | `			break;` |
|         - |  221 | `		}` |
|  17447634 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5200076 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5200074 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   9647656 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   6543880 |  229 | `	return 0;` |
|   5872241 |  230 |  |
|  11860218 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  11860220 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    116292 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  11743930 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  11743930 |  244 | `	if( pEntry == 0 ){` |
|   6543880 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5200052 |  247 | `	return (SyHashEntry *)pEntry;` |
|   5930375 |  248 |  |
|     86364 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     86366 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     66024 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     33013 |  254 | `	}else{` |
|     20344 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     86366 |  257 | `	if( pEntry->pNextCollide ){` |
|      4325 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2162 |  259 | `	}` |
|     86366 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     86366 |  261 | `	pHash->nEntry--;` |
|     86366 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     86366 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     86366 |  268 | `	return rc;` |
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
|     86342 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     86344 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     86344 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     86344 |  296 | `	return rc;` |
|         2 |  297 |  |
|    280272 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    280274 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    280274 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2114228 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2114230 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    279840 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    279840 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   1834392 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   1834392 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   1834392 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1057116 |  325 |  |
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
|   1453667 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    726865 |  371 | `		}` |
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
|   2994674 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2994676 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2994676 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2994676 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1999321 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    999692 |  389 | `	}` |
|   2994676 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2994676 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2994676 |  393 | `	if( pHash->nEntry == 0 ){` |
|    121654 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     60826 |  395 | `	}` |
|   2994676 |  396 | `	pHash->nEntry++;` |
|   2994676 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2994674 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2994676 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     24132 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     24132 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     12065 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2994676 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2994676 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2994676 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2994676 |  421 | `	pEntry->pHash = pHash;` |
|   2994676 |  422 | `	pEntry->pKey = pKey;` |
|   2994676 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2994676 |  424 | `	pEntry->pUserData = pUserData;` |
|   2994676 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2994676 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2994676 |  428 | `	return rc;` |
|   1497339 |  429 |  |
|    113622 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    113624 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
