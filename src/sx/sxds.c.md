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
|  184782627 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  184782632 |   16 | `	pSet->nSize = 0 ;` |
|  184782632 |   17 | `	pSet->nUsed = 0;` |
|  184782632 |   18 | `	pSet->nCursor = 0;` |
|  184782632 |   19 | `	pSet->eSize = ElemSize;` |
|  184782632 |   20 | `	pSet->pAllocator = pAllocator;` |
|  184782632 |   21 | `	pSet->pBase =  0;` |
|  184782632 |   22 | `	pSet->pUserData = 0;` |
|  184782632 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  418747560 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  418747565 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   24203855 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   24203855 |   33 | `		if( pSet->nSize <= 0 ){` |
|   20628869 |   34 | `			pSet->nSize = 4;` |
|   10315363 |   35 | `		}` |
|   24203855 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   24203855 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   24203855 |   40 | `		pSet->pBase = pNew;` |
|   24203855 |   41 | `		pSet->nSize <<= 1;` |
|   12102856 |   42 | `	}` |
|  418747565 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3305800741 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  418747565 |   45 | `	pSet->nUsed++;` |
|  418747565 |   46 | `	return SXRET_OK;` |
|  209376944 |   47 | `}` |
|   20810772 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   20810777 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   20810777 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   20810777 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   20810777 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   20810777 |   60 | `	pSet->nSize = nItem;` |
|   20810777 |   61 | `	return SXRET_OK;` |
|   10405391 |   62 | `}` |
|   30816504 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   30816509 |   65 | `	pSet->nUsed   = 0;` |
|   30816509 |   66 | `	pSet->nCursor = 0;` |
|   30816509 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69228 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69233 |   71 | `	pSet->nCursor = 0;` |
|      69233 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      69486 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      69491 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30147 |   79 | `		pSet->nCursor = 0;` |
|      30147 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      39349 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      39349 |   83 | `	if( ppEntry ){` |
|      39349 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      19672 |   85 | `	}` |
|      39349 |   86 | `	pSet->nCursor++;` |
|      39349 |   87 | `	return SXRET_OK;` |
|      34748 |   88 | `}` |
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
|    3288308 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3288313 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1253 |  103 | `		pSet->nUsed = nNewSize;` |
|        624 |  104 | `	}` |
|    3288313 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   63218551 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   63218556 |  109 | `	sxi32 rc = SXRET_OK;` |
|   63218556 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   34220495 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   17111176 |  112 | `	}` |
|   63218556 |  113 | `	pSet->pBase = 0;` |
|   63218556 |  114 | `	pSet->nUsed = 0;` |
|   63218556 |  115 | `	pSet->nCursor = 0;` |
|   63218556 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   74412278 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   74412283 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       4013 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   74408275 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   74408275 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   37206144 |  126 | `}` |
|    9874547 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9874552 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2237765 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7636792 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7636792 |  135 | `	pSet->nUsed--;` |
|    7636792 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7636792 |  137 | `	return pData;` |
|    4937899 |  138 | `}` |
|   37320085 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   37320090 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   37320038 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   37320038 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   18664179 |  148 | `}` |
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
|    2188715 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    2188720 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2188720 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    2188720 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2188720 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    2188720 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    2188720 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    2188720 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    2188720 |  180 | `	pHash->nEntry = 0;` |
|    2188720 |  181 | `	pHash->apBucket = apNew;` |
|    2188720 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    2188720 |  183 | `	return SXRET_OK;` |
|    1094466 |  184 | `}` |
|     521785 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     521790 |  193 | `	pEntry = pHash->pList;` |
|     276529 |  194 | `	for(;;){` |
|     552856 |  195 | `		if( pHash->nEntry == 0 ){` |
|     521790 |  196 | `			break;` |
|          - |  197 | `		}` |
|      31071 |  198 | `		pNext = pEntry->pNext;` |
|      31071 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      31071 |  200 | `		pEntry = pNext;` |
|      31071 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     521790 |  203 | `	if( pHash->apBucket ){` |
|     521790 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     260996 |  205 | `	}` |
|     521790 |  206 | `	pHash->apBucket = 0;` |
|     521790 |  207 | `	pHash->nBucketSize = 0;` |
|     521790 |  208 | `	pHash->pAllocator = 0;` |
|     521790 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   79584238 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   79584243 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   79584243 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   71204474 |  218 | `	for(;;){` |
|  142655022 |  219 | `		if( pEntry == 0 ){` |
|   31610519 |  220 | `			break;` |
|          - |  221 | `		}` |
|  135030829 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   47977736 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   47973729 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   63070784 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   31610519 |  229 | `	return 0;` |
|   39798263 |  230 | `}` |
|   89521339 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   89521344 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    9937564 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   79583785 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   79583785 |  244 | `	if( pEntry == 0 ){` |
|   31610501 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   47973289 |  247 | `	return (SyHashEntry *)pEntry;` |
|   44766917 |  248 | `}` |
|     512840 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     512845 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     417483 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     209262 |  254 | `	}else{` |
|      95367 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     512845 |  257 | `	if( pEntry->pNextCollide ){` |
|       4434 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2212 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     512845 |  261 | `	if( pHash->pLast == pEntry ){` |
|     503277 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     252257 |  263 | `	}` |
|     512845 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     512845 |  265 | `	pHash->nEntry--;` |
|     512845 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     512845 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     512845 |  272 | `	return rc;` |
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
|     512400 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     512405 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     512405 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     512405 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3428812 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3428817 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3428817 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   26410740 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   26410745 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3428551 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3428551 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   22982199 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   22982199 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   22982199 |  328 | `	return (SyHashEntry *)pEntry;` |
|   13205375 |  329 | `}` |
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
|     100700 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100705 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100705 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100705 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100705 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18632449 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18531749 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18531749 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18531749 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18531749 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8940718 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4469923 |  375 | `		}` |
|   18531749 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18531749 |  378 | `		pEntry = pEntry->pNext;` |
|    9265877 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100705 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100705 |  382 | `	pHash->apBucket = apNew;` |
|     100705 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100705 |  384 | `	return SXRET_OK;` |
|      50355 |  385 | `}` |
|   22600900 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   22600905 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   22600905 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   22600905 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   14191034 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    7095776 |  393 | `	}` |
|   22600905 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   22600905 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|     882175 |  399 | `		pHash->pLast->pNext = pEntry;` |
|     882175 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|     882175 |  401 | `		pHash->pLast = pEntry;` |
|     441090 |  402 | `	}else{` |
|   21718735 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   22600905 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1215336 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1215336 |  408 | `		pHash->pLast = pEntry;` |
|     607769 |  409 | `	}` |
|   22600905 |  410 | `	pHash->nEntry++;` |
|   22600905 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   22600900 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   22600905 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100705 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100705 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50350 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   22600905 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   22600905 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   22600905 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   22600905 |  435 | `	pEntry->pHash = pHash;` |
|   22600905 |  436 | `	pEntry->pKey = pKey;` |
|   22600905 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   22600905 |  438 | `	pEntry->pUserData = pUserData;` |
|   22600905 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   22600905 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   22600905 |  442 | `	return rc;` |
|   11301076 |  443 | `}` |
|   21456826 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   21456831 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|    1144074 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  455 | `{` |
|    1144079 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          5 |  457 | `}` |
|     552784 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     552789 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
