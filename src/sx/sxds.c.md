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
|  10583234 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  10583236 |   16 | `	pSet->nSize = 0 ;` |
|  10583236 |   17 | `	pSet->nUsed = 0;` |
|  10583236 |   18 | `	pSet->nCursor = 0;` |
|  10583236 |   19 | `	pSet->eSize = ElemSize;` |
|  10583236 |   20 | `	pSet->pAllocator = pAllocator;` |
|  10583236 |   21 | `	pSet->pBase =  0;` |
|  10583236 |   22 | `	pSet->pUserData = 0;` |
|  10583236 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  16776140 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  16776142 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3374712 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3374712 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3306406 |   34 | `			pSet->nSize = 4;` |
|   1653202 |   35 | `		}` |
|   3374712 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3374712 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3374712 |   40 | `		pSet->pBase = pNew;` |
|   3374712 |   41 | `		pSet->nSize <<= 1;` |
|   1687355 |   42 | `	}` |
|  16776142 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 125923434 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  16776142 |   45 | `	pSet->nUsed++;` |
|  16776142 |   46 | `	return SXRET_OK;` |
|   8388094 |   47 |  |
|    479416 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    479418 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    479418 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    479418 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    479418 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    479418 |   60 | `	pSet->nSize = nItem;` |
|    479418 |   61 | `	return SXRET_OK;` |
|    239710 |   62 |  |
|    926466 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|    926468 |   65 | `	pSet->nUsed   = 0;` |
|    926468 |   66 | `	pSet->nCursor = 0;` |
|    926468 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     36956 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     36958 |   71 | `	pSet->nCursor = 0;` |
|     36958 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     40580 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     40582 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     14886 |   79 | `		pSet->nCursor = 0;` |
|     14886 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     25698 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     25698 |   83 | `	if( ppEntry ){` |
|     25698 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     12848 |   85 | `	}` |
|     25698 |   86 | `	pSet->nCursor++;` |
|     25698 |   87 | `	return SXRET_OK;` |
|     20292 |   88 |  |
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
|     60534 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     60536 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     60536 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   7008388 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   7008390 |  109 | `	sxi32 rc = SXRET_OK;` |
|   7008390 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3606728 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1803363 |  112 | `	}` |
|   7008390 |  113 | `	pSet->pBase = 0;` |
|   7008390 |  114 | `	pSet->nUsed = 0;` |
|   7008390 |  115 | `	pSet->nCursor = 0;` |
|   7008390 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3437276 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3437278 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3437188 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3437188 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1718640 |  126 |  |
|   3024556 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3024558 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2125894 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    898666 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    898666 |  135 | `	pSet->nUsed--;` |
|    898666 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    898666 |  137 | `	return pData;` |
|   1512280 |  138 |  |
|   9021190 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   9021192 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   9021192 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   9021192 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4510810 |  148 |  |
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
|     85752 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     85754 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     85754 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     85754 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     85754 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     85754 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     85754 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     85754 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     85754 |  180 | `	pHash->nEntry = 0;` |
|     85754 |  181 | `	pHash->apBucket = apNew;` |
|     85754 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     85754 |  183 | `	return SXRET_OK;` |
|     42878 |  184 |  |
|     10706 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     10708 |  193 | `	pEntry = pHash->pList;` |
|      6429 |  194 | `	for(;;){` |
|     12860 |  195 | `		if( pHash->nEntry == 0 ){` |
|     10708 |  196 | `			break;` |
|         - |  197 | `		}` |
|      2154 |  198 | `		pNext = pEntry->pNext;` |
|      2154 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      2154 |  200 | `		pEntry = pNext;` |
|      2154 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     10708 |  203 | `	if( pHash->apBucket ){` |
|     10708 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      5353 |  205 | `	}` |
|     10708 |  206 | `	pHash->apBucket = 0;` |
|     10708 |  207 | `	pHash->nBucketSize = 0;` |
|     10708 |  208 | `	pHash->pAllocator = 0;` |
|     10708 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   8634972 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   8634974 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   8634974 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   7512498 |  218 | `	for(;;){` |
|  14922957 |  219 | `		if( pEntry == 0 ){` |
|   4691546 |  220 | `			break;` |
|         - |  221 | `		}` |
|  12202997 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   3943432 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   3943430 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   6287985 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   4691546 |  229 | `	return 0;` |
|   4317752 |  230 |  |
|   8683246 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   8683248 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     48282 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   8634968 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   8634968 |  244 | `	if( pEntry == 0 ){` |
|   4691546 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   3943424 |  247 | `	return (SyHashEntry *)pEntry;` |
|   4341889 |  248 |  |
|     68082 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     68084 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     51062 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     25532 |  254 | `	}else{` |
|     17024 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     68084 |  257 | `	if( pEntry->pNextCollide ){` |
|      4047 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2023 |  259 | `	}` |
|     68084 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     68084 |  261 | `	pHash->nEntry--;` |
|     68084 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     68084 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     68084 |  268 | `	return rc;` |
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
|     68076 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     68078 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     68078 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     68078 |  296 | `	return rc;` |
|         2 |  297 |  |
|    123584 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    123586 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    123586 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    859358 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    859360 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    123152 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    123152 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    736210 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    736210 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    736210 |  324 | `	return (SyHashEntry *)pEntry;` |
|    429681 |  325 |  |
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
|      1589 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1579 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1579 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1579 |  344 | `		pEntry = pEntry->pNext;` |
|       790 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     12332 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     12334 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     12334 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     12334 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     12334 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1691470 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1679138 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1679138 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1679138 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1679138 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    806325 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    403158 |  371 | `		}` |
|   1679138 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1679138 |  374 | `		pEntry = pEntry->pNext;` |
|    839570 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     12334 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     12334 |  378 | `	pHash->apBucket = apNew;` |
|     12334 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     12334 |  380 | `	return SXRET_OK;` |
|      6168 |  381 |  |
|   1526028 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1526030 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1526030 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1526030 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1023677 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    511833 |  389 | `	}` |
|   1526030 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1526030 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1526030 |  393 | `	if( pHash->nEntry == 0 ){` |
|     61524 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     30761 |  395 | `	}` |
|   1526030 |  396 | `	pHash->nEntry++;` |
|   1526030 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1526028 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1526030 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     12334 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     12334 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      6166 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1526030 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1526030 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1526030 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1526030 |  421 | `	pEntry->pHash = pHash;` |
|   1526030 |  422 | `	pEntry->pKey = pKey;` |
|   1526030 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1526030 |  424 | `	pEntry->pUserData = pUserData;` |
|   1526030 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1526030 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1526030 |  428 | `	return rc;` |
|    763016 |  429 |  |
|     83612 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     83614 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
