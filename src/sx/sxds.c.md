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
|  184949502 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  184949507 |   16 | `	pSet->nSize = 0 ;` |
|  184949507 |   17 | `	pSet->nUsed = 0;` |
|  184949507 |   18 | `	pSet->nCursor = 0;` |
|  184949507 |   19 | `	pSet->eSize = ElemSize;` |
|  184949507 |   20 | `	pSet->pAllocator = pAllocator;` |
|  184949507 |   21 | `	pSet->pBase =  0;` |
|  184949507 |   22 | `	pSet->pUserData = 0;` |
|  184949507 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  419136758 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  419136763 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   24216912 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   24216912 |   33 | `		if( pSet->nSize <= 0 ){` |
|   20638310 |   34 | `			pSet->nSize = 4;` |
|   10320079 |   35 | `		}` |
|   24216912 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   24216912 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   24216912 |   40 | `		pSet->pBase = pNew;` |
|   24216912 |   41 | `		pSet->nSize <<= 1;` |
|   12109380 |   42 | `	}` |
|  419136763 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3308982029 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  419136763 |   45 | `	pSet->nUsed++;` |
|  419136763 |   46 | `	return SXRET_OK;` |
|  209571529 |   47 | `}` |
|   20831904 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   20831909 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   20831909 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   20831909 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   20831909 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   20831909 |   60 | `	pSet->nSize = nItem;` |
|   20831909 |   61 | `	return SXRET_OK;` |
|   10415957 |   62 | `}` |
|   30844779 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   30844784 |   65 | `	pSet->nUsed   = 0;` |
|   30844784 |   66 | `	pSet->nCursor = 0;` |
|   30844784 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69218 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69223 |   71 | `	pSet->nCursor = 0;` |
|      69223 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      69478 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      69483 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30133 |   79 | `		pSet->nCursor = 0;` |
|      30133 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      39355 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      39355 |   83 | `	if( ppEntry ){` |
|      39355 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      19675 |   85 | `	}` |
|      39355 |   86 | `	pSet->nCursor++;` |
|      39355 |   87 | `	return SXRET_OK;` |
|      34744 |   88 | `}` |
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
|    3291666 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3291671 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1253 |  103 | `		pSet->nUsed = nNewSize;` |
|        624 |  104 | `	}` |
|    3291671 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   63261622 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   63261627 |  109 | `	sxi32 rc = SXRET_OK;` |
|   63261627 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   34243518 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   17122683 |  112 | `	}` |
|   63261627 |  113 | `	pSet->pBase = 0;` |
|   63261627 |  114 | `	pSet->nUsed = 0;` |
|   63261627 |  115 | `	pSet->nCursor = 0;` |
|   63261627 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   74485606 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   74485611 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       4017 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   74481599 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   74481599 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   37242808 |  126 | `}` |
|    9875161 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9875166 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2238057 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7637114 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7637114 |  135 | `	pSet->nUsed--;` |
|    7637114 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7637114 |  137 | `	return pData;` |
|    4938203 |  138 | `}` |
|   37266449 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   37266454 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   37266402 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   37266402 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   18637355 |  148 | `}` |
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
|    2190248 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    2190253 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2190253 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    2190253 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2190253 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    2190253 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    2190253 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    2190253 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    2190253 |  180 | `	pHash->nEntry = 0;` |
|    2190253 |  181 | `	pHash->apBucket = apNew;` |
|    2190253 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    2190253 |  183 | `	return SXRET_OK;` |
|    1095232 |  184 | `}` |
|     521638 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     521643 |  193 | `	pEntry = pHash->pList;` |
|     276453 |  194 | `	for(;;){` |
|     552705 |  195 | `		if( pHash->nEntry == 0 ){` |
|     521643 |  196 | `			break;` |
|          - |  197 | `		}` |
|      31067 |  198 | `		pNext = pEntry->pNext;` |
|      31067 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      31067 |  200 | `		pEntry = pNext;` |
|      31067 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     521643 |  203 | `	if( pHash->apBucket ){` |
|     521643 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     260922 |  205 | `	}` |
|     521643 |  206 | `	pHash->apBucket = 0;` |
|     521643 |  207 | `	pHash->nBucketSize = 0;` |
|     521643 |  208 | `	pHash->pAllocator = 0;` |
|     521643 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   75375146 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   75375151 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   75375151 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   68113841 |  218 | `	for(;;){` |
|  136486891 |  219 | `		if( pEntry == 0 ){` |
|   27396650 |  220 | `			break;` |
|          - |  221 | `		}` |
|  133078964 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   47982517 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   47978506 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   61111745 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   27396650 |  229 | `	return 0;` |
|   37693699 |  230 | `}` |
|   84395760 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   84395765 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    9021077 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   75374693 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   75374693 |  244 | `	if( pEntry == 0 ){` |
|   27396632 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   47978066 |  247 | `	return (SyHashEntry *)pEntry;` |
|   42204109 |  248 | `}` |
|     509036 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     509041 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     414307 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     207670 |  254 | `	}else{` |
|      94739 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     509041 |  257 | `	if( pEntry->pNextCollide ){` |
|       4390 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2188 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     509041 |  261 | `	if( pHash->pLast == pEntry ){` |
|     499497 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     250364 |  263 | `	}` |
|     509041 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     509041 |  265 | `	pHash->nEntry--;` |
|     509041 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     509041 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     509041 |  272 | `	return rc;` |
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
|     508596 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     508601 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     508601 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     508601 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3434502 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3434507 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3434507 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   26461978 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   26461983 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3434241 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3434241 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   23027747 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   23027747 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   23027747 |  328 | `	return (SyHashEntry *)pEntry;` |
|   13230994 |  329 | `}` |
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
|     100862 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100867 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100867 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100867 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100867 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18668803 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18567941 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18567941 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18567941 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18567941 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8957689 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4478295 |  375 | `		}` |
|   18567941 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18567941 |  378 | `		pEntry = pEntry->pNext;` |
|    9283973 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100867 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100867 |  382 | `	pHash->apBucket = apNew;` |
|     100867 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100867 |  384 | `	return SXRET_OK;` |
|      50436 |  385 | `}` |
|   22630668 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   22630673 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   22630673 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   22630673 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   14214979 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    7107493 |  393 | `	}` |
|   22630673 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   22630673 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|     883039 |  399 | `		pHash->pLast->pNext = pEntry;` |
|     883039 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|     883039 |  401 | `		pHash->pLast = pEntry;` |
|     441522 |  402 | `	}else{` |
|   21747639 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   22630673 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1215855 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1215855 |  408 | `		pHash->pLast = pEntry;` |
|     608028 |  409 | `	}` |
|   22630673 |  410 | `	pHash->nEntry++;` |
|   22630673 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   22630668 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   22630673 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100867 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100867 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50431 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   22630673 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   22630673 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   22630673 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   22630673 |  435 | `	pEntry->pHash = pHash;` |
|   22630673 |  436 | `	pEntry->pKey = pKey;` |
|   22630673 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   22630673 |  438 | `	pEntry->pUserData = pUserData;` |
|   22630673 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   22630673 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   22630673 |  442 | `	return rc;` |
|   11315957 |  443 | `}` |
|   21485476 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   21485481 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|    1145192 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  455 | `{` |
|    1145197 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          5 |  457 | `}` |
|     549088 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     549093 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
