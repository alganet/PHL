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
|  183816246 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  183816251 |   16 | `	pSet->nSize = 0 ;` |
|  183816251 |   17 | `	pSet->nUsed = 0;` |
|  183816251 |   18 | `	pSet->nCursor = 0;` |
|  183816251 |   19 | `	pSet->eSize = ElemSize;` |
|  183816251 |   20 | `	pSet->pAllocator = pAllocator;` |
|  183816251 |   21 | `	pSet->pBase =  0;` |
|  183816251 |   22 | `	pSet->pUserData = 0;` |
|  183816251 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  416488159 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  416488164 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   24099524 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   24099524 |   33 | `		if( pSet->nSize <= 0 ){` |
|   20544574 |   34 | `			pSet->nSize = 4;` |
|   10273229 |   35 | `		}` |
|   24099524 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   24099524 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   24099524 |   40 | `		pSet->pBase = pNew;` |
|   24099524 |   41 | `		pSet->nSize <<= 1;` |
|   12050704 |   42 | `	}` |
|  416488164 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3287765536 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  416488164 |   45 | `	pSet->nUsed++;` |
|  416488164 |   46 | `	return SXRET_OK;` |
|  208247291 |   47 | `}` |
|   20693654 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   20693659 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   20693659 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   20693659 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   20693659 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   20693659 |   60 | `	pSet->nSize = nItem;` |
|   20693659 |   61 | `	return SXRET_OK;` |
|   10346832 |   62 | `}` |
|   30650830 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   30650835 |   65 | `	pSet->nUsed   = 0;` |
|   30650835 |   66 | `	pSet->nCursor = 0;` |
|   30650835 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69230 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69235 |   71 | `	pSet->nCursor = 0;` |
|      69235 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      69484 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      69489 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30185 |   79 | `		pSet->nCursor = 0;` |
|      30185 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      39309 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      39309 |   83 | `	if( ppEntry ){` |
|      39309 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      19652 |   85 | `	}` |
|      39309 |   86 | `	pSet->nCursor++;` |
|      39309 |   87 | `	return SXRET_OK;` |
|      34747 |   88 | `}` |
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
|    3269744 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3269749 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1253 |  103 | `		pSet->nUsed = nNewSize;` |
|        624 |  104 | `	}` |
|    3269749 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   62922046 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   62922051 |  109 | `	sxi32 rc = SXRET_OK;` |
|   62922051 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   34059472 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   17030678 |  112 | `	}` |
|   62922051 |  113 | `	pSet->pBase = 0;` |
|   62922051 |  114 | `	pSet->nUsed = 0;` |
|   62922051 |  115 | `	pSet->nCursor = 0;` |
|   62922051 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   74007064 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   74007069 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       3995 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   74003079 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   74003079 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   37003537 |  126 | `}` |
|    9847109 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9847114 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2237303 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7609816 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7609816 |  135 | `	pSet->nUsed--;` |
|    7609816 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7609816 |  137 | `	return pData;` |
|    4924189 |  138 | `}` |
|   37249826 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   37249831 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   37249779 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   37249779 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   18629083 |  148 | `}` |
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
|    2177768 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    2177773 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2177773 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    2177773 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2177773 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    2177773 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    2177773 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    2177773 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    2177773 |  180 | `	pHash->nEntry = 0;` |
|    2177773 |  181 | `	pHash->apBucket = apNew;` |
|    2177773 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    2177773 |  183 | `	return SXRET_OK;` |
|    1088994 |  184 | `}` |
|     520116 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     520121 |  193 | `	pEntry = pHash->pList;` |
|     275720 |  194 | `	for(;;){` |
|     551235 |  195 | `		if( pHash->nEntry == 0 ){` |
|     520121 |  196 | `			break;` |
|          - |  197 | `		}` |
|      31119 |  198 | `		pNext = pEntry->pNext;` |
|      31119 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      31119 |  200 | `		pEntry = pNext;` |
|      31119 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     520121 |  203 | `	if( pHash->apBucket ){` |
|     520121 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     260163 |  205 | `	}` |
|     520121 |  206 | `	pHash->apBucket = 0;` |
|     520121 |  207 | `	pHash->nBucketSize = 0;` |
|     520121 |  208 | `	pHash->pAllocator = 0;` |
|     520121 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   79274168 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   79274173 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   79274173 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   71230697 |  218 | `	for(;;){` |
|  142064252 |  219 | `		if( pEntry == 0 ){` |
|   31505037 |  220 | `			break;` |
|          - |  221 | `		}` |
|  134443192 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   47773126 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   47769141 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   62790084 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   31505037 |  229 | `	return 0;` |
|   39643331 |  230 | `}` |
|   89157132 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   89157137 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    9883431 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   79273711 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   79273711 |  244 | `	if( pEntry == 0 ){` |
|   31505019 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   47768697 |  247 | `	return (SyHashEntry *)pEntry;` |
|   44584918 |  248 | `}` |
|     516584 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     516589 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     420621 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     210838 |  254 | `	}else{` |
|      95973 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     516589 |  257 | `	if( pEntry->pNextCollide ){` |
|       4395 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2200 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     516589 |  261 | `	if( pHash->pLast == pEntry ){` |
|     507033 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     254144 |  263 | `	}` |
|     516589 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     516589 |  265 | `	pHash->nEntry--;` |
|     516589 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     516589 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     516589 |  272 | `	return rc;` |
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
|     516140 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     516145 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     516145 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     516145 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3409780 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3409785 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3409785 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   26263910 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   26263915 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3409519 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3409519 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   22854401 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   22854401 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   22854401 |  328 | `	return (SyHashEntry *)pEntry;` |
|   13131960 |  329 | `}` |
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
|     100142 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100147 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100147 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100147 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100147 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18530515 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18430373 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18430373 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18430373 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18430373 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8895321 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4447295 |  375 | `		}` |
|   18430373 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18430373 |  378 | `		pEntry = pEntry->pNext;` |
|    9215189 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100147 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100147 |  382 | `	pHash->apBucket = apNew;` |
|     100147 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100147 |  384 | `	return SXRET_OK;` |
|      50076 |  385 | `}` |
|   22478960 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   22478965 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   22478965 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   22478965 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   14110827 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    7055612 |  393 | `	}` |
|   22478965 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   22478965 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|     877431 |  399 | `		pHash->pLast->pNext = pEntry;` |
|     877431 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|     877431 |  401 | `		pHash->pLast = pEntry;` |
|     438718 |  402 | `	}else{` |
|   21601539 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   22478965 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1209797 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1209797 |  408 | `		pHash->pLast = pEntry;` |
|     605001 |  409 | `	}` |
|   22478965 |  410 | `	pHash->nEntry++;` |
|   22478965 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   22478960 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   22478965 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100147 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100147 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50071 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   22478965 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   22478965 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   22478965 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   22478965 |  435 | `	pEntry->pHash = pHash;` |
|   22478965 |  436 | `	pEntry->pKey = pKey;` |
|   22478965 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   22478965 |  438 | `	pEntry->pUserData = pUserData;` |
|   22478965 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   22478965 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   22478965 |  442 | `	return rc;` |
|   11240115 |  443 | `}` |
|   21341042 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   21341047 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|    1137918 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  455 | `{` |
|    1137923 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          5 |  457 | `}` |
|     556348 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     556353 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
