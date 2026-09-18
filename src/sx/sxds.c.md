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
|  184000691 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  184000696 |   16 | `	pSet->nSize = 0 ;` |
|  184000696 |   17 | `	pSet->nUsed = 0;` |
|  184000696 |   18 | `	pSet->nCursor = 0;` |
|  184000696 |   19 | `	pSet->eSize = ElemSize;` |
|  184000696 |   20 | `	pSet->pAllocator = pAllocator;` |
|  184000696 |   21 | `	pSet->pBase =  0;` |
|  184000696 |   22 | `	pSet->pUserData = 0;` |
|  184000696 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  416910633 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  416910638 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   24121181 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   24121181 |   33 | `		if( pSet->nSize <= 0 ){` |
|   20562555 |   34 | `			pSet->nSize = 4;` |
|   10282233 |   35 | `		}` |
|   24121181 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   24121181 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   24121181 |   40 | `		pSet->pBase = pNew;` |
|   24121181 |   41 | `		pSet->nSize <<= 1;` |
|   12061546 |   42 | `	}` |
|  416910638 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3291108730 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  416910638 |   45 | `	pSet->nUsed++;` |
|  416910638 |   46 | `	return SXRET_OK;` |
|  208458572 |   47 | `}` |
|   20715168 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   20715173 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   20715173 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   20715173 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   20715173 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   20715173 |   60 | `	pSet->nSize = nItem;` |
|   20715173 |   61 | `	return SXRET_OK;` |
|   10357589 |   62 | `}` |
|   30682423 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   30682428 |   65 | `	pSet->nUsed   = 0;` |
|   30682428 |   66 | `	pSet->nCursor = 0;` |
|   30682428 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69270 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69275 |   71 | `	pSet->nCursor = 0;` |
|      69275 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      69524 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      69529 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30205 |   79 | `		pSet->nCursor = 0;` |
|      30205 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      39329 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      39329 |   83 | `	if( ppEntry ){` |
|      39329 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      19662 |   85 | `	}` |
|      39329 |   86 | `	pSet->nCursor++;` |
|      39329 |   87 | `	return SXRET_OK;` |
|      34767 |   88 | `}` |
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
|    3273140 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3273145 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1253 |  103 | `		pSet->nUsed = nNewSize;` |
|        624 |  104 | `	}` |
|    3273145 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   62980635 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   62980640 |  109 | `	sxi32 rc = SXRET_OK;` |
|   62980640 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   34091349 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   17046630 |  112 | `	}` |
|   62980640 |  113 | `	pSet->pBase = 0;` |
|   62980640 |  114 | `	pSet->nUsed = 0;` |
|   62980640 |  115 | `	pSet->nCursor = 0;` |
|   62980640 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   74081728 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   74081733 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       3999 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   74077739 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   74077739 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   37040869 |  126 | `}` |
|    9854231 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9854236 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2237625 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7616616 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7616616 |  135 | `	pSet->nUsed--;` |
|    7616616 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7616616 |  137 | `	return pData;` |
|    4927759 |  138 | `}` |
|   37280592 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   37280597 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   37280545 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   37280545 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   18644530 |  148 | `}` |
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
|    2179957 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    2179962 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2179962 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    2179962 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2179962 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    2179962 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    2179962 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    2179962 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    2179962 |  180 | `	pHash->nEntry = 0;` |
|    2179962 |  181 | `	pHash->apBucket = apNew;` |
|    2179962 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    2179962 |  183 | `	return SXRET_OK;` |
|    1090090 |  184 | `}` |
|     520555 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     520560 |  193 | `	pEntry = pHash->pList;` |
|     275944 |  194 | `	for(;;){` |
|     551680 |  195 | `		if( pHash->nEntry == 0 ){` |
|     520560 |  196 | `			break;` |
|          - |  197 | `		}` |
|      31125 |  198 | `		pNext = pEntry->pNext;` |
|      31125 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      31125 |  200 | `		pEntry = pNext;` |
|      31125 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     520560 |  203 | `	if( pHash->apBucket ){` |
|     520560 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     260384 |  205 | `	}` |
|     520560 |  206 | `	pHash->apBucket = 0;` |
|     520560 |  207 | `	pHash->nBucketSize = 0;` |
|     520560 |  208 | `	pHash->pAllocator = 0;` |
|     520560 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   79351926 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   79351931 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   79351931 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   71059291 |  218 | `	for(;;){` |
|  142202909 |  219 | `		if( pEntry == 0 ){` |
|   31535350 |  220 | `			break;` |
|          - |  221 | `		}` |
|  134575231 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   47820575 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   47816586 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   62850983 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   31535350 |  229 | `	return 0;` |
|   39682284 |  230 | `}` |
|   89245049 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   89245054 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    9893590 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   79351469 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   79351469 |  244 | `	if( pEntry == 0 ){` |
|   31535332 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   47816142 |  247 | `	return (SyHashEntry *)pEntry;` |
|   44628952 |  248 | `}` |
|     516758 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     516763 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     420762 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     210916 |  254 | `	}else{` |
|      96006 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     516763 |  257 | `	if( pEntry->pNextCollide ){` |
|       4391 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2198 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     516763 |  261 | `	if( pHash->pLast == pEntry ){` |
|     507207 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     254240 |  263 | `	}` |
|     516763 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     516763 |  265 | `	pHash->nEntry--;` |
|     516763 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     516763 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     516763 |  272 | `	return rc;` |
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
|     516314 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     516319 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     516319 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     516319 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3413198 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3413203 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3413203 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   26289456 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   26289461 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3412937 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3412937 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   22876529 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   22876529 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   22876529 |  328 | `	return (SyHashEntry *)pEntry;` |
|   13144733 |  329 | `}` |
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
|     100250 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100255 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100255 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100255 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100255 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18550975 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18450725 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18450725 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18450725 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18450725 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8905515 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4453045 |  375 | `		}` |
|   18450725 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18450725 |  378 | `		pEntry = pEntry->pNext;` |
|    9225365 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100255 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100255 |  382 | `	pHash->apBucket = apNew;` |
|     100255 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100255 |  384 | `	return SXRET_OK;` |
|      50130 |  385 | `}` |
|   22502894 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   22502899 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   22502899 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   22502899 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   14125691 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    7062875 |  393 | `	}` |
|   22502899 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   22502899 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|     878387 |  399 | `		pHash->pLast->pNext = pEntry;` |
|     878387 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|     878387 |  401 | `		pHash->pLast = pEntry;` |
|     439196 |  402 | `	}else{` |
|   21624517 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   22502899 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1211020 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1211020 |  408 | `		pHash->pLast = pEntry;` |
|     605614 |  409 | `	}` |
|   22502899 |  410 | `	pHash->nEntry++;` |
|   22502899 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   22502894 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   22502899 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100255 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100255 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50125 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   22502899 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   22502899 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   22502899 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   22502899 |  435 | `	pEntry->pHash = pHash;` |
|   22502899 |  436 | `	pEntry->pKey = pKey;` |
|   22502899 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   22502899 |  438 | `	pEntry->pUserData = pUserData;` |
|   22502899 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   22502899 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   22502899 |  442 | `	return rc;` |
|   11252091 |  443 | `}` |
|   21363716 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   21363721 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|    1139178 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  455 | `{` |
|    1139183 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          5 |  457 | `}` |
|     556614 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     556619 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
