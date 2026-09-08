# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 293/304 lines (96.38%)

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
|  156065318 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  156065323 |   16 | `	pSet->nSize = 0 ;` |
|  156065323 |   17 | `	pSet->nUsed = 0;` |
|  156065323 |   18 | `	pSet->nCursor = 0;` |
|  156065323 |   19 | `	pSet->eSize = ElemSize;` |
|  156065323 |   20 | `	pSet->pAllocator = pAllocator;` |
|  156065323 |   21 | `	pSet->pBase =  0;` |
|  156065323 |   22 | `	pSet->pUserData = 0;` |
|  156065323 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  355074335 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  355074340 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   20552209 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   20552209 |   33 | `		if( pSet->nSize <= 0 ){` |
|   17514485 |   34 | `			pSet->nSize = 4;` |
|    8757240 |   35 | `		}` |
|   20552209 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   20552209 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   20552209 |   40 | `		pSet->pBase = pNew;` |
|   20552209 |   41 | `		pSet->nSize <<= 1;` |
|   10276102 |   42 | `	}` |
|  355074340 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2800877002 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  355074340 |   45 | `	pSet->nUsed++;` |
|  355074340 |   46 | `	return SXRET_OK;` |
|  177537196 |   47 | `}` |
|   17456446 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   17456451 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   17456451 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   17456451 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   17456451 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   17456451 |   60 | `	pSet->nSize = nItem;` |
|   17456451 |   61 | `	return SXRET_OK;` |
|    8728228 |   62 | `}` |
|   25975953 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   25975958 |   65 | `	pSet->nUsed   = 0;` |
|   25975958 |   66 | `	pSet->nCursor = 0;` |
|   25975958 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69344 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69349 |   71 | `	pSet->nCursor = 0;` |
|      69349 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73484 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73489 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30013 |   79 | `		pSet->nCursor = 0;` |
|      30013 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43481 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43481 |   83 | `	if( ppEntry ){` |
|      43481 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21738 |   85 | `	}` |
|      43481 |   86 | `	pSet->nCursor++;` |
|      43481 |   87 | `	return SXRET_OK;` |
|      36747 |   88 | `}` |
|          - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|          8 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|          1 |   91 | `{` |
|          - |   92 | `	register unsigned char *zSrc;` |
|          9 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          3 |   94 | `		return 0;` |
|          - |   95 | `	}` |
|          7 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|          7 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|          5 |   98 | `}` |
|          - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    2693286 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2693291 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1181 |  103 | `		pSet->nUsed = nNewSize;` |
|        588 |  104 | `	}` |
|    2693291 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   53871218 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   53871223 |  109 | `	sxi32 rc = SXRET_OK;` |
|   53871223 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   29181001 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   14590498 |  112 | `	}` |
|   53871223 |  113 | `	pSet->pBase = 0;` |
|   53871223 |  114 | `	pSet->nUsed = 0;` |
|   53871223 |  115 | `	pSet->nCursor = 0;` |
|   53871223 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   63273418 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   63273423 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15349 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   63258079 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   63258079 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   31636714 |  126 | `}` |
|    8812734 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    8812739 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2213519 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    6599225 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    6599225 |  135 | `	pSet->nUsed--;` |
|    6599225 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    6599225 |  137 | `	return pData;` |
|    4406372 |  138 | `}` |
|   32007265 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   32007270 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   32007248 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   32007248 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   16003738 |  148 | `}` |
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
|    1761070 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1761075 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1761075 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1761075 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1761075 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1761075 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1761075 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1761075 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1761075 |  180 | `	pHash->nEntry = 0;` |
|    1761075 |  181 | `	pHash->apBucket = apNew;` |
|    1761075 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1761075 |  183 | `	return SXRET_OK;` |
|     880540 |  184 | `}` |
|     408070 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     408075 |  193 | `	pEntry = pHash->pList;` |
|     217209 |  194 | `	for(;;){` |
|     434423 |  195 | `		if( pHash->nEntry == 0 ){` |
|     408075 |  196 | `			break;` |
|          - |  197 | `		}` |
|      26353 |  198 | `		pNext = pEntry->pNext;` |
|      26353 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      26353 |  200 | `		pEntry = pNext;` |
|      26353 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     408075 |  203 | `	if( pHash->apBucket ){` |
|     408075 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     204035 |  205 | `	}` |
|     408075 |  206 | `	pHash->apBucket = 0;` |
|     408075 |  207 | `	pHash->nBucketSize = 0;` |
|     408075 |  208 | `	pHash->pAllocator = 0;` |
|     408075 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   65449483 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   65449488 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   65449488 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   58801845 |  218 | `	for(;;){` |
|  117951928 |  219 | `		if( pEntry == 0 ){` |
|   23767776 |  220 | `			break;` |
|          - |  221 | `		}` |
|  115024947 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   41681862 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   41681717 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   52502445 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   23767776 |  229 | `	return 0;` |
|   32725030 |  230 | `}` |
|   72255943 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   72255948 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    6806817 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   65449136 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   65449136 |  244 | `	if( pEntry == 0 ){` |
|   23767758 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   41681383 |  247 | `	return (SyHashEntry *)pEntry;` |
|   36128260 |  248 | `}` |
|     423324 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     423329 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     347855 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     173930 |  254 | `	}else{` |
|      75479 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     423329 |  257 | `	if( pEntry->pNextCollide ){` |
|       4406 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2202 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     423329 |  261 | `	if( pHash->pLast == pEntry ){` |
|     415613 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     207804 |  263 | `	}` |
|     423329 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     423329 |  265 | `	pHash->nEntry--;` |
|     423329 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     423329 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     423329 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        352 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        357 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        357 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        339 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        339 |  288 | `	return rc;` |
|        181 |  289 | `}` |
|     422990 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     422995 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     422995 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     422995 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2819602 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2819607 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2819607 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   20951288 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   20951293 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2819341 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2819341 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18131957 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18131957 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18131957 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10475649 |  329 | `}` |
|         10 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|          1 |  331 | `{` |
|          - |  332 | `	SyHashEntry_Pr *pEntry;` |
|          - |  333 | `	sxi32 rc;` |
|          - |  334 | `	sxu32 n;` |
|          - |  335 | `#if defined(UNTRUST)` |
|          - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|          - |  337 | `		return 0;` |
|          - |  338 | `	}` |
|          - |  339 | `#endif` |
|         11 |  340 | `	pEntry = pHash->pList;` |
|       3993 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3983 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3983 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3983 |  348 | `		pEntry = pEntry->pNext;` |
|       1992 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      91574 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      91579 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      91579 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      91579 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      91579 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14375227 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14283653 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14283653 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14283653 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14283653 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    6883549 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3442055 |  375 | `		}` |
|   14283653 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14283653 |  378 | `		pEntry = pEntry->pNext;` |
|    7141829 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      91579 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      91579 |  382 | `	pHash->apBucket = apNew;` |
|      91579 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      91579 |  384 | `	return SXRET_OK;` |
|      45792 |  385 | `}` |
|   18032960 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   18032965 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   18032965 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   18032965 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11333359 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5666557 |  393 | `	}` |
|   18032965 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   18032965 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   18032913 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   18032965 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     970479 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     970479 |  408 | `		pHash->pLast = pEntry;` |
|     485237 |  409 | `	}` |
|   18032965 |  410 | `	pHash->nEntry++;` |
|   18032965 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   18032960 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   18032965 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      91579 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      91579 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45787 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   18032965 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   18032965 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   18032965 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   18032965 |  435 | `	pEntry->pHash = pHash;` |
|   18032965 |  436 | `	pEntry->pKey = pKey;` |
|   18032965 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   18032965 |  438 | `	pEntry->pUserData = pUserData;` |
|   18032965 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   18032965 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   18032965 |  442 | `	return rc;` |
|    9016485 |  443 | `}` |
|   18032828 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   18032833 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        132 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          2 |  455 | `{` |
|        134 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          2 |  457 | `}` |
|     462558 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     462563 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
