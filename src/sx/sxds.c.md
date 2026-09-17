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
|  183543181 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  183543186 |   16 | `	pSet->nSize = 0 ;` |
|  183543186 |   17 | `	pSet->nUsed = 0;` |
|  183543186 |   18 | `	pSet->nCursor = 0;` |
|  183543186 |   19 | `	pSet->eSize = ElemSize;` |
|  183543186 |   20 | `	pSet->pAllocator = pAllocator;` |
|  183543186 |   21 | `	pSet->pBase =  0;` |
|  183543186 |   22 | `	pSet->pUserData = 0;` |
|  183543186 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  415852047 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  415852052 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   24068051 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   24068051 |   33 | `		if( pSet->nSize <= 0 ){` |
|   20518625 |   34 | `			pSet->nSize = 4;` |
|   10260250 |   35 | `		}` |
|   24068051 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   24068051 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   24068051 |   40 | `		pSet->pBase = pNew;` |
|   24068051 |   41 | `		pSet->nSize <<= 1;` |
|   12034963 |   42 | `	}` |
|  415852052 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3282742300 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  415852052 |   45 | `	pSet->nUsed++;` |
|  415852052 |   46 | `	return SXRET_OK;` |
|  207929219 |   47 | `}` |
|   20661398 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   20661403 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   20661403 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   20661403 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   20661403 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   20661403 |   60 | `	pSet->nSize = nItem;` |
|   20661403 |   61 | `	return SXRET_OK;` |
|   10330704 |   62 | `}` |
|   30602591 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   30602596 |   65 | `	pSet->nUsed   = 0;` |
|   30602596 |   66 | `	pSet->nCursor = 0;` |
|   30602596 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69140 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69145 |   71 | `	pSet->nCursor = 0;` |
|      69145 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      69394 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      69399 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30139 |   79 | `		pSet->nCursor = 0;` |
|      30139 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      39265 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      39265 |   83 | `	if( ppEntry ){` |
|      39265 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      19630 |   85 | `	}` |
|      39265 |   86 | `	pSet->nCursor++;` |
|      39265 |   87 | `	return SXRET_OK;` |
|      34702 |   88 | `}` |
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
|    3264676 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3264681 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1253 |  103 | `		pSet->nUsed = nNewSize;` |
|        624 |  104 | `	}` |
|    3264681 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   62836195 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   62836200 |  109 | `	sxi32 rc = SXRET_OK;` |
|   62836200 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   34012427 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   17007151 |  112 | `	}` |
|   62836200 |  113 | `	pSet->pBase = 0;` |
|   62836200 |  114 | `	pSet->nUsed = 0;` |
|   62836200 |  115 | `	pSet->nCursor = 0;` |
|   62836200 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   73894326 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   73894331 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       3989 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   73890347 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   73890347 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   36947168 |  126 | `}` |
|    9837903 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9837908 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2237085 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7600828 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7600828 |  135 | `	pSet->nUsed--;` |
|    7600828 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7600828 |  137 | `	return pData;` |
|    4919583 |  138 | `}` |
|   37201221 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   37201226 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   37201174 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   37201174 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   18604776 |  148 | `}` |
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
|    2174423 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    2174428 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2174428 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    2174428 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2174428 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    2174428 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    2174428 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    2174428 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    2174428 |  180 | `	pHash->nEntry = 0;` |
|    2174428 |  181 | `	pHash->apBucket = apNew;` |
|    2174428 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    2174428 |  183 | `	return SXRET_OK;` |
|    1087321 |  184 | `}` |
|     519419 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     519424 |  193 | `	pEntry = pHash->pList;` |
|     275359 |  194 | `	for(;;){` |
|     550514 |  195 | `		if( pHash->nEntry == 0 ){` |
|     519424 |  196 | `			break;` |
|          - |  197 | `		}` |
|      31095 |  198 | `		pNext = pEntry->pNext;` |
|      31095 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      31095 |  200 | `		pEntry = pNext;` |
|      31095 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     519424 |  203 | `	if( pHash->apBucket ){` |
|     519424 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     259814 |  205 | `	}` |
|     519424 |  206 | `	pHash->apBucket = 0;` |
|     519424 |  207 | `	pHash->nBucketSize = 0;` |
|     519424 |  208 | `	pHash->pAllocator = 0;` |
|     519424 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   79153388 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   79153393 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   79153393 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   70935015 |  218 | `	for(;;){` |
|  141769049 |  219 | `		if( pEntry == 0 ){` |
|   31459334 |  220 | `			break;` |
|          - |  221 | `		}` |
|  134156167 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   47698043 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   47694064 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   62615661 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   31459334 |  229 | `	return 0;` |
|   39582903 |  230 | `}` |
|   89020617 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   89020622 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    9867692 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   79152935 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   79152935 |  244 | `	if( pEntry == 0 ){` |
|   31459316 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   47693624 |  247 | `	return (SyHashEntry *)pEntry;` |
|   44516622 |  248 | `}` |
|     516056 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     516061 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     420212 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     210632 |  254 | `	}else{` |
|      95854 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     516061 |  257 | `	if( pEntry->pNextCollide ){` |
|       4394 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2195 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     516061 |  261 | `	if( pHash->pLast == pEntry ){` |
|     506497 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     253873 |  263 | `	}` |
|     516061 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     516061 |  265 | `	pHash->nEntry--;` |
|     516061 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     516061 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     516061 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        458 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        463 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        463 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        445 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        445 |  288 | `	return rc;` |
|        234 |  289 | `}` |
|     515616 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     515621 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     515621 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     515621 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3404628 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3404633 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3404633 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   26226462 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   26226467 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3404367 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3404367 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   22822105 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   22822105 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   22822105 |  328 | `	return (SyHashEntry *)pEntry;` |
|   13113236 |  329 | `}` |
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
|       4859 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4845 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4845 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4845 |  348 | `		pEntry = pEntry->pNext;` |
|       2423 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|      99980 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      99985 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      99985 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      99985 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      99985 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18499825 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18399845 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18399845 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18399845 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18399845 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8878450 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4438642 |  375 | `		}` |
|   18399845 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18399845 |  378 | `		pEntry = pEntry->pNext;` |
|    9199925 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      99985 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      99985 |  382 | `	pHash->apBucket = apNew;` |
|      99985 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      99985 |  384 | `	return SXRET_OK;` |
|      49995 |  385 | `}` |
|   22446450 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   22446455 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   22446455 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   22446455 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   14090401 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    7045553 |  393 | `	}` |
|   22446455 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   22446455 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|     876073 |  399 | `		pHash->pLast->pNext = pEntry;` |
|     876073 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|     876073 |  401 | `		pHash->pLast = pEntry;` |
|     438039 |  402 | `	}else{` |
|   21570387 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   22446455 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1207980 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1207980 |  408 | `		pHash->pLast = pEntry;` |
|     604092 |  409 | `	}` |
|   22446455 |  410 | `	pHash->nEntry++;` |
|   22446455 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   22446450 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   22446455 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      99985 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      99985 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      49990 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   22446455 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   22446455 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   22446455 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   22446455 |  435 | `	pEntry->pHash = pHash;` |
|   22446455 |  436 | `	pEntry->pKey = pKey;` |
|   22446455 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   22446455 |  438 | `	pEntry->pUserData = pUserData;` |
|   22446455 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   22446455 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   22446455 |  442 | `	return rc;` |
|   11223857 |  443 | `}` |
|   21310292 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   21310297 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|    1136158 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  455 | `{` |
|    1136163 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          5 |  457 | `}` |
|     555748 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     555753 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
