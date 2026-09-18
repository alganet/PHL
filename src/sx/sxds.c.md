# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 286/304 lines (94.08%)

[Root index](../../index.md) | [Directory index](index.md)

|       Hits | Line | Source |
| ---------: | ---: | :--- |
|          - |    1 | `/**` |
|          - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|          - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|          - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|          - |    5 | ` */` |
|          - |    6 | `#include "sxtypes.h"` |
|          - |    7 | `#include "sxmacros.h"` |
|          - |    8 | `#include "sxset.h"` |
|          - |    9 | `#include "sxmem.h"` |
|          - |   10 | `#include "sxhashtable.h"` |
|          - |   11 | `#include "sxhash.h"` |
|          - |   12 | `#include "sxstr.h"` |
|          - |   13 |  |
|  183908395 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  183908400 |   16 | `	pSet->nSize = 0 ;` |
|  183908400 |   17 | `	pSet->nUsed = 0;` |
|  183908400 |   18 | `	pSet->nCursor = 0;` |
|  183908400 |   19 | `	pSet->eSize = ElemSize;` |
|  183908400 |   20 | `	pSet->pAllocator = pAllocator;` |
|  183908400 |   21 | `	pSet->pBase =  0;` |
|  183908400 |   22 | `	pSet->pUserData = 0;` |
|  183908400 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  416699341 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  416699346 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   24110309 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   24110309 |   33 | `		if( pSet->nSize <= 0 ){` |
|   20553527 |   34 | `			pSet->nSize = 4;` |
|   10277701 |   35 | `		}` |
|   24110309 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   24110309 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   24110309 |   40 | `		pSet->pBase = pNew;` |
|   24110309 |   41 | `		pSet->nSize <<= 1;` |
|   12056092 |   42 | `	}` |
|  416699346 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3289437268 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  416699346 |   45 | `	pSet->nUsed++;` |
|  416699346 |   46 | `	return SXRET_OK;` |
|  208352866 |   47 | `}` |
|   20704416 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   20704421 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   20704421 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   20704421 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   20704421 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   20704421 |   60 | `	pSet->nSize = nItem;` |
|   20704421 |   61 | `	return SXRET_OK;` |
|   10352213 |   62 | `}` |
|   30666645 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   30666650 |   65 | `	pSet->nUsed   = 0;` |
|   30666650 |   66 | `	pSet->nCursor = 0;` |
|   30666650 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69250 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69255 |   71 | `	pSet->nCursor = 0;` |
|      69255 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      69504 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      69509 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30195 |   79 | `		pSet->nCursor = 0;` |
|      30195 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      39319 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      39319 |   83 | `	if( ppEntry ){` |
|      39319 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      19657 |   85 | `	}` |
|      39319 |   86 | `	pSet->nCursor++;` |
|      39319 |   87 | `	return SXRET_OK;` |
|      34757 |   88 | `}` |
|          - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        ! 0 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|        ! 0 |   91 | `{` |
|          - |   92 | `	register unsigned char *zSrc;` |
|        ! 0 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|        ! 0 |   94 | `		return 0;` |
|          - |   95 | `	}` |
|        ! 0 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|        ! 0 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|        ! 0 |   98 | `}` |
|          - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    3271450 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3271455 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1253 |  103 | `		pSet->nUsed = nNewSize;` |
|        624 |  104 | `	}` |
|    3271455 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   62951277 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   62951282 |  109 | `	sxi32 rc = SXRET_OK;` |
|   62951282 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   34075375 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   17038625 |  112 | `	}` |
|   62951282 |  113 | `	pSet->pBase = 0;` |
|   62951282 |  114 | `	pSet->nUsed = 0;` |
|   62951282 |  115 | `	pSet->nCursor = 0;` |
|   62951282 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   74044384 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   74044389 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       3997 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   74040397 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   74040397 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   37022197 |  126 | `}` |
|    9850641 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9850646 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2237449 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7613202 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7613202 |  135 | `	pSet->nUsed--;` |
|    7613202 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7613202 |  137 | `	return pData;` |
|    4925952 |  138 | `}` |
|   37265269 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   37265274 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   37265222 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   37265222 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   18636773 |  148 | `}` |
|          - |  149 | `/* Private hash entry */` |
|          - |  150 | `struct SyHashEntry_Pr` |
|          - |  151 | `{` |
|          - |  152 | `	const void *pKey; /* Hash key */` |
|          - |  153 | `	sxu32 nKeyLen;    /* Key length */` |
|          - |  154 | `	void *pUserData;  /* User private data */` |
|          - |  155 | `	/* Private fields */` |
|          - |  156 | `	sxu32 nHash;` |
|          - |  157 | `	SyHash *pHash;` |
|          - |  158 | `	SyHashEntry_Pr *pNext,*pPrev; /* Next and previous entry in the list */` |
|          - |  159 | `	SyHashEntry_Pr *pNextCollide,*pPrevCollide; /* Collision list */` |
|          - |  160 | `};` |
|          - |  161 | `#define INVALID_HASH(H) ((H)->apBucket == 0)` |
|    2178857 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    2178862 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2178862 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    2178862 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2178862 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    2178862 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    2178862 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    2178862 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    2178862 |  180 | `	pHash->nEntry = 0;` |
|    2178862 |  181 | `	pHash->apBucket = apNew;` |
|    2178862 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    2178862 |  183 | `	return SXRET_OK;` |
|    1089538 |  184 | `}` |
|     520327 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     520332 |  193 | `	pEntry = pHash->pList;` |
|     275825 |  194 | `	for(;;){` |
|     551446 |  195 | `		if( pHash->nEntry == 0 ){` |
|     520332 |  196 | `			break;` |
|          - |  197 | `		}` |
|      31119 |  198 | `		pNext = pEntry->pNext;` |
|      31119 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      31119 |  200 | `		pEntry = pNext;` |
|      31119 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     520332 |  203 | `	if( pHash->apBucket ){` |
|     520332 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     260268 |  205 | `	}` |
|     520332 |  206 | `	pHash->apBucket = 0;` |
|     520332 |  207 | `	pHash->nBucketSize = 0;` |
|     520332 |  208 | `	pHash->pAllocator = 0;` |
|     520332 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   79313116 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   79313121 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   79313121 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   71107892 |  218 | `	for(;;){` |
|  142386212 |  219 | `		if( pEntry == 0 ){` |
|   31520248 |  220 | `			break;` |
|          - |  221 | `		}` |
|  134761827 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   47796865 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   47792878 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   63073096 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   31520248 |  229 | `	return 0;` |
|   39662767 |  230 | `}` |
|   89201147 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   89201152 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    9888498 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   79312659 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   79312659 |  244 | `	if( pEntry == 0 ){` |
|   31520230 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   47792434 |  247 | `	return (SyHashEntry *)pEntry;` |
|   44606887 |  248 | `}` |
|     516676 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     516681 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     420696 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     210873 |  254 | `	}else{` |
|      95990 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     516681 |  257 | `	if( pEntry->pNextCollide ){` |
|       4395 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2200 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     516681 |  261 | `	if( pHash->pLast == pEntry ){` |
|     507125 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     254187 |  263 | `	}` |
|     516681 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     516681 |  265 | `	pHash->nEntry--;` |
|     516681 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     516681 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     516681 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        462 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        467 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        467 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        449 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        449 |  288 | `	return rc;` |
|        236 |  289 | `}` |
|     516232 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     516237 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     516237 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     516237 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3411490 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3411495 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3411495 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   26276680 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   26276685 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3411229 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3411229 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   22865461 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   22865461 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   22865461 |  328 | `	return (SyHashEntry *)pEntry;` |
|   13138345 |  329 | `}` |
|         14 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|          1 |  331 | `{` |
|          - |  332 | `	SyHashEntry_Pr *pEntry;` |
|          - |  333 | `	sxi32 rc;` |
|          - |  334 | `	sxu32 n;` |
|          - |  335 | `#if defined(UNTRUST)` |
|          - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|          - |  337 | `		return 0;` |
|          - |  338 | `	}` |
|          - |  339 | `#endif` |
|         15 |  340 | `	pEntry = pHash->pList;` |
|       4857 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4843 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4843 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4843 |  348 | `		pEntry = pEntry->pNext;` |
|       2422 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|     100196 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100201 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100201 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100201 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100201 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18540745 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18440549 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18440549 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18440549 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18440549 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8899322 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4450047 |  375 | `		}` |
|   18440549 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18440549 |  378 | `		pEntry = pEntry->pNext;` |
|    9220277 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100201 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100201 |  382 | `	pHash->apBucket = apNew;` |
|     100201 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100201 |  384 | `	return SXRET_OK;` |
|      50103 |  385 | `}` |
|   22490922 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   22490927 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   22490927 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   22490927 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   14118463 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    7059276 |  393 | `	}` |
|   22490927 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   22490927 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|     877905 |  399 | `		pHash->pLast->pNext = pEntry;` |
|     877905 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|     877905 |  401 | `		pHash->pLast = pEntry;` |
|     438955 |  402 | `	}else{` |
|   21613027 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   22490927 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1210408 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1210408 |  408 | `		pHash->pLast = pEntry;` |
|     605306 |  409 | `	}` |
|   22490927 |  410 | `	pHash->nEntry++;` |
|   22490927 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   22490922 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   22490927 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100201 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100201 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50098 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   22490927 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   22490927 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   22490927 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   22490927 |  435 | `	pEntry->pHash = pHash;` |
|   22490927 |  436 | `	pEntry->pKey = pKey;` |
|   22490927 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   22490927 |  438 | `	pEntry->pUserData = pUserData;` |
|   22490927 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   22490927 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   22490927 |  442 | `	return rc;` |
|   11246093 |  443 | `}` |
|   21352378 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   21352383 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|    1138544 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  455 | `{` |
|    1138549 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          5 |  457 | `}` |
|     556488 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     556493 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
