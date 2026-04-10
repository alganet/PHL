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
|  14396760 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  14396762 |   16 | `	pSet->nSize = 0 ;` |
|  14396762 |   17 | `	pSet->nUsed = 0;` |
|  14396762 |   18 | `	pSet->nCursor = 0;` |
|  14396762 |   19 | `	pSet->eSize = ElemSize;` |
|  14396762 |   20 | `	pSet->pAllocator = pAllocator;` |
|  14396762 |   21 | `	pSet->pBase =  0;` |
|  14396762 |   22 | `	pSet->pUserData = 0;` |
|  14396762 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  23830948 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  23830950 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3917348 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3917348 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3807306 |   34 | `			pSet->nSize = 4;` |
|   1903652 |   35 | `		}` |
|   3917348 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3917348 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3917348 |   40 | `		pSet->pBase = pNew;` |
|   3917348 |   41 | `		pSet->nSize <<= 1;` |
|   1958673 |   42 | `	}` |
|  23830950 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 176832866 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  23830950 |   45 | `	pSet->nUsed++;` |
|  23830950 |   46 | `	return SXRET_OK;` |
|  11915498 |   47 |  |
|    867370 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    867372 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    867372 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    867372 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    867372 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    867372 |   60 | `	pSet->nSize = nItem;` |
|    867372 |   61 | `	return SXRET_OK;` |
|    433687 |   62 |  |
|   1335984 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1335986 |   65 | `	pSet->nUsed   = 0;` |
|   1335986 |   66 | `	pSet->nCursor = 0;` |
|   1335986 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     46556 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     46558 |   71 | `	pSet->nCursor = 0;` |
|     46558 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     50638 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     50640 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     19098 |   79 | `		pSet->nCursor = 0;` |
|     19098 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     31544 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     31544 |   83 | `	if( ppEntry ){` |
|     31544 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     15771 |   85 | `	}` |
|     31544 |   86 | `	pSet->nCursor++;` |
|     31544 |   87 | `	return SXRET_OK;` |
|     25321 |   88 |  |
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
|    144030 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    144032 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|    144032 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8470504 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8470506 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8470506 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4323636 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2161817 |  112 | `	}` |
|   8470506 |  113 | `	pSet->pBase = 0;` |
|   8470506 |  114 | `	pSet->nUsed = 0;` |
|   8470506 |  115 | `	pSet->nCursor = 0;` |
|   8470506 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4605456 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4605458 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       108 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4605352 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4605352 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2302730 |  126 |  |
|   3298136 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3298138 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2143808 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1154332 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1154332 |  135 | `	pSet->nUsed--;` |
|   1154332 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1154332 |  137 | `	return pData;` |
|   1649070 |  138 |  |
|  11053255 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  11053257 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  11053257 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  11053257 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5526813 |  148 |  |
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
|    258170 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    258172 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    258172 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    258172 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    258172 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    258172 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    258172 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    258172 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    258172 |  180 | `	pHash->nEntry = 0;` |
|    258172 |  181 | `	pHash->apBucket = apNew;` |
|    258172 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    258172 |  183 | `	return SXRET_OK;` |
|    129087 |  184 |  |
|     78064 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     78066 |  193 | `	pEntry = pHash->pList;` |
|     40924 |  194 | `	for(;;){` |
|     81850 |  195 | `		if( pHash->nEntry == 0 ){` |
|     78066 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3786 |  198 | `		pNext = pEntry->pNext;` |
|      3786 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3786 |  200 | `		pEntry = pNext;` |
|      3786 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     78066 |  203 | `	if( pHash->apBucket ){` |
|     78066 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     39032 |  205 | `	}` |
|     78066 |  206 | `	pHash->apBucket = 0;` |
|     78066 |  207 | `	pHash->nBucketSize = 0;` |
|     78066 |  208 | `	pHash->pAllocator = 0;` |
|     78066 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  12031340 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  12031342 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  12031342 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  10836482 |  218 | `	for(;;){` |
|  21664933 |  219 | `		if( pEntry == 0 ){` |
|   6630458 |  220 | `			break;` |
|         - |  221 | `		}` |
|  17734789 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5400888 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5400886 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   9633593 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   6630458 |  229 | `	return 0;` |
|   6015936 |  230 |  |
|  12498294 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  12498296 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    466978 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  12031320 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  12031320 |  244 | `	if( pEntry == 0 ){` |
|   6630458 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5400864 |  247 | `	return (SyHashEntry *)pEntry;` |
|   6249413 |  248 |  |
|     90306 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     90308 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     68550 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     34276 |  254 | `	}else{` |
|     21760 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     90308 |  257 | `	if( pEntry->pNextCollide ){` |
|      4677 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2338 |  259 | `	}` |
|     90308 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     90308 |  261 | `	pHash->nEntry--;` |
|     90308 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     90308 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     90308 |  268 | `	return rc;` |
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
|     90284 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     90286 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     90286 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     90286 |  296 | `	return rc;` |
|         2 |  297 |  |
|    311084 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    311086 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    311086 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2445726 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2445728 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    310652 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    310652 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2135078 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2135078 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2135078 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1222865 |  325 |  |
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
|      1773 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1763 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1763 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1763 |  344 | `		pEntry = pEntry->pNext;` |
|       882 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     22518 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     22520 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     22520 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     22520 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     22520 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2859224 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2836706 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2836706 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2836706 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2836706 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1355583 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    677815 |  371 | `		}` |
|   2836706 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2836706 |  374 | `		pEntry = pEntry->pNext;` |
|   1418354 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     22520 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     22520 |  378 | `	pHash->apBucket = apNew;` |
|     22520 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     22520 |  380 | `	return SXRET_OK;` |
|     11261 |  381 |  |
|   2908462 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2908464 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2908464 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2908464 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1883288 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    941639 |  389 | `	}` |
|   2908464 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2908464 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2908464 |  393 | `	if( pHash->nEntry == 0 ){` |
|    129744 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     64871 |  395 | `	}` |
|   2908464 |  396 | `	pHash->nEntry++;` |
|   2908464 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2908462 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2908464 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     22520 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     22520 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     11259 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2908464 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2908464 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2908464 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2908464 |  421 | `	pEntry->pHash = pHash;` |
|   2908464 |  422 | `	pEntry->pKey = pKey;` |
|   2908464 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2908464 |  424 | `	pEntry->pUserData = pUserData;` |
|   2908464 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2908464 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2908464 |  428 | `	return rc;` |
|   1454233 |  429 |  |
|    115546 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    115548 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
