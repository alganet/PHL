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
|  16257040 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  16257042 |   16 | `	pSet->nSize = 0 ;` |
|  16257042 |   17 | `	pSet->nUsed = 0;` |
|  16257042 |   18 | `	pSet->nCursor = 0;` |
|  16257042 |   19 | `	pSet->eSize = ElemSize;` |
|  16257042 |   20 | `	pSet->pAllocator = pAllocator;` |
|  16257042 |   21 | `	pSet->pBase =  0;` |
|  16257042 |   22 | `	pSet->pUserData = 0;` |
|  16257042 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  26697294 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  26697296 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4076498 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4076498 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3945416 |   34 | `			pSet->nSize = 4;` |
|   1972707 |   35 | `		}` |
|   4076498 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4076498 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4076498 |   40 | `		pSet->pBase = pNew;` |
|   4076498 |   41 | `		pSet->nSize <<= 1;` |
|   2038248 |   42 | `	}` |
|  26697296 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 199439898 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  26697296 |   45 | `	pSet->nUsed++;` |
|  26697296 |   46 | `	return SXRET_OK;` |
|  13348671 |   47 |  |
|   1051850 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|   1051852 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1051852 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1051852 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1051852 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1051852 |   60 | `	pSet->nSize = nItem;` |
|   1051852 |   61 | `	return SXRET_OK;` |
|    525927 |   62 |  |
|   1527562 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1527564 |   65 | `	pSet->nUsed   = 0;` |
|   1527564 |   66 | `	pSet->nCursor = 0;` |
|   1527564 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     49950 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     49952 |   71 | `	pSet->nCursor = 0;` |
|     49952 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     54032 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     54034 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     20554 |   79 | `		pSet->nCursor = 0;` |
|     20554 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     33482 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     33482 |   83 | `	if( ppEntry ){` |
|     33482 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     16740 |   85 | `	}` |
|     33482 |   86 | `	pSet->nCursor++;` |
|     33482 |   87 | `	return SXRET_OK;` |
|     27018 |   88 |  |
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
|    182848 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    182850 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       110 |  103 | `		pSet->nUsed = nNewSize;` |
|        54 |  104 | `	}` |
|    182850 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8991904 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8991906 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8991906 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4542174 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2271086 |  112 | `	}` |
|   8991906 |  113 | `	pSet->pBase = 0;` |
|   8991906 |  114 | `	pSet->nUsed = 0;` |
|   8991906 |  115 | `	pSet->nCursor = 0;` |
|   8991906 |  116 | `	return rc;` |
|         2 |  117 |  |
|   5072200 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5072202 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       108 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5072096 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5072096 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2536102 |  126 |  |
|   3350474 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3350476 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2147644 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1202834 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1202834 |  135 | `	pSet->nUsed--;` |
|   1202834 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1202834 |  137 | `	return pData;` |
|   1675239 |  138 |  |
|  11691538 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  11691540 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  11691540 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  11691540 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5845918 |  148 |  |
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
|    292894 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    292896 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    292896 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    292896 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    292896 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    292896 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    292896 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    292896 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    292896 |  180 | `	pHash->nEntry = 0;` |
|    292896 |  181 | `	pHash->apBucket = apNew;` |
|    292896 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    292896 |  183 | `	return SXRET_OK;` |
|    146449 |  184 |  |
|     84994 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     84996 |  193 | `	pEntry = pHash->pList;` |
|     44979 |  194 | `	for(;;){` |
|     89960 |  195 | `		if( pHash->nEntry == 0 ){` |
|     84996 |  196 | `			break;` |
|         - |  197 | `		}` |
|      4966 |  198 | `		pNext = pEntry->pNext;` |
|      4966 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      4966 |  200 | `		pEntry = pNext;` |
|      4966 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     84996 |  203 | `	if( pHash->apBucket ){` |
|     84996 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     42497 |  205 | `	}` |
|     84996 |  206 | `	pHash->apBucket = 0;` |
|     84996 |  207 | `	pHash->nBucketSize = 0;` |
|     84996 |  208 | `	pHash->pAllocator = 0;` |
|     84996 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  13119132 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  13119134 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  13119134 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  11769254 |  218 | `	for(;;){` |
|  23452865 |  219 | `		if( pEntry == 0 ){` |
|   7157038 |  220 | `			break;` |
|         - |  221 | `		}` |
|  19276747 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5962100 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5962098 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  10333733 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   7157038 |  229 | `	return 0;` |
|   6559832 |  230 |  |
|  13674534 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  13674536 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    555546 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  13118992 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  13118992 |  244 | `	if( pEntry == 0 ){` |
|   7157038 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5961956 |  247 | `	return (SyHashEntry *)pEntry;` |
|   6837533 |  248 |  |
|    101072 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    101074 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     77166 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     38584 |  254 | `	}else{` |
|     23910 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    101074 |  257 | `	if( pEntry->pNextCollide ){` |
|      4939 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2469 |  259 | `	}` |
|    101074 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    101074 |  261 | `	pHash->nEntry--;` |
|    101074 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    101074 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    101074 |  268 | `	return rc;` |
|         2 |  269 |  |
|       142 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       144 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       144 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       144 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       144 |  284 | `	return rc;` |
|        73 |  285 |  |
|    100930 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|    100932 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    100932 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    100932 |  296 | `	return rc;` |
|         2 |  297 |  |
|    366412 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    366414 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    366414 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2835186 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2835188 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    365978 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    365978 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2469212 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2469212 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2469212 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1417595 |  325 |  |
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
|      1791 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1781 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1781 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1781 |  344 | `		pEntry = pEntry->pNext;` |
|       891 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     24266 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     24268 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     24268 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     24268 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     24268 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3080428 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3056162 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3056162 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3056162 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3056162 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1463386 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    731698 |  371 | `		}` |
|   3056162 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3056162 |  374 | `		pEntry = pEntry->pNext;` |
|   1528082 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     24268 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     24268 |  378 | `	pHash->apBucket = apNew;` |
|     24268 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     24268 |  380 | `	return SXRET_OK;` |
|     12135 |  381 |  |
|   3197010 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   3197012 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   3197012 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   3197012 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2056599 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1028241 |  389 | `	}` |
|   3197012 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   3197012 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   3197012 |  393 | `	if( pHash->nEntry == 0 ){` |
|    145182 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     72590 |  395 | `	}` |
|   3197012 |  396 | `	pHash->nEntry++;` |
|   3197012 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   3197010 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   3197012 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     24268 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     24268 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     12133 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   3197012 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   3197012 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   3197012 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   3197012 |  421 | `	pEntry->pHash = pHash;` |
|   3197012 |  422 | `	pEntry->pKey = pKey;` |
|   3197012 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   3197012 |  424 | `	pEntry->pUserData = pUserData;` |
|   3197012 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   3197012 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   3197012 |  428 | `	return rc;` |
|   1598507 |  429 |  |
|    128186 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    128188 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
