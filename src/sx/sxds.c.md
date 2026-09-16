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
|  184680862 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  184680867 |   16 | `	pSet->nSize = 0 ;` |
|  184680867 |   17 | `	pSet->nUsed = 0;` |
|  184680867 |   18 | `	pSet->nCursor = 0;` |
|  184680867 |   19 | `	pSet->eSize = ElemSize;` |
|  184680867 |   20 | `	pSet->pAllocator = pAllocator;` |
|  184680867 |   21 | `	pSet->pBase =  0;` |
|  184680867 |   22 | `	pSet->pUserData = 0;` |
|  184680867 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  418510250 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  418510255 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   24186086 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   24186086 |   33 | `		if( pSet->nSize <= 0 ){` |
|   20612970 |   34 | `			pSet->nSize = 4;` |
|   10307400 |   35 | `		}` |
|   24186086 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   24186086 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   24186086 |   40 | `		pSet->pBase = pNew;` |
|   24186086 |   41 | `		pSet->nSize <<= 1;` |
|   12093958 |   42 | `	}` |
|  418510255 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3304013019 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  418510255 |   45 | `	pSet->nUsed++;` |
|  418510255 |   46 | `	return SXRET_OK;` |
|  209258244 |   47 | `}` |
|   20799900 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   20799905 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   20799905 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   20799905 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   20799905 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   20799905 |   60 | `	pSet->nSize = nItem;` |
|   20799905 |   61 | `	return SXRET_OK;` |
|   10399955 |   62 | `}` |
|   30798631 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   30798636 |   65 | `	pSet->nUsed   = 0;` |
|   30798636 |   66 | `	pSet->nCursor = 0;` |
|   30798636 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69198 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69203 |   71 | `	pSet->nCursor = 0;` |
|      69203 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      69458 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      69463 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30119 |   79 | `		pSet->nCursor = 0;` |
|      30119 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      39349 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      39349 |   83 | `	if( ppEntry ){` |
|      39349 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      19672 |   85 | `	}` |
|      39349 |   86 | `	pSet->nCursor++;` |
|      39349 |   87 | `	return SXRET_OK;` |
|      34734 |   88 | `}` |
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
|    3286598 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3286603 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1253 |  103 | `		pSet->nUsed = nNewSize;` |
|        624 |  104 | `	}` |
|    3286603 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   63176760 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   63176765 |  109 | `	sxi32 rc = SXRET_OK;` |
|   63176765 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   34197242 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   17099536 |  112 | `	}` |
|   63176765 |  113 | `	pSet->pBase = 0;` |
|   63176765 |  114 | `	pSet->nUsed = 0;` |
|   63176765 |  115 | `	pSet->nCursor = 0;` |
|   63176765 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   74374300 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   74374305 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       4011 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   74370299 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   74370299 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   37187155 |  126 | `}` |
|    9866101 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9866106 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2237887 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7628224 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7628224 |  135 | `	pSet->nUsed--;` |
|    7628224 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7628224 |  137 | `	return pData;` |
|    4933667 |  138 | `}` |
|   37229520 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   37229525 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   37229473 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   37229473 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   18618845 |  148 | `}` |
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
|    2187084 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    2187089 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2187089 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    2187089 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2187089 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    2187089 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    2187089 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    2187089 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    2187089 |  180 | `	pHash->nEntry = 0;` |
|    2187089 |  181 | `	pHash->apBucket = apNew;` |
|    2187089 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    2187089 |  183 | `	return SXRET_OK;` |
|    1093649 |  184 | `}` |
|     521048 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     521053 |  193 | `	pEntry = pHash->pList;` |
|     276157 |  194 | `	for(;;){` |
|     552115 |  195 | `		if( pHash->nEntry == 0 ){` |
|     521053 |  196 | `			break;` |
|          - |  197 | `		}` |
|      31067 |  198 | `		pNext = pEntry->pNext;` |
|      31067 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      31067 |  200 | `		pEntry = pNext;` |
|      31067 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     521053 |  203 | `	if( pHash->apBucket ){` |
|     521053 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     260626 |  205 | `	}` |
|     521053 |  206 | `	pHash->apBucket = 0;` |
|     521053 |  207 | `	pHash->nBucketSize = 0;` |
|     521053 |  208 | `	pHash->pAllocator = 0;` |
|     521053 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   75277400 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   75277405 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   75277405 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   67930336 |  218 | `	for(;;){` |
|  136189123 |  219 | `		if( pEntry == 0 ){` |
|   27363898 |  220 | `			break;` |
|          - |  221 | `		}` |
|  132781476 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   47917517 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   47913512 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   60911723 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   27363898 |  229 | `	return 0;` |
|   37644760 |  230 | `}` |
|   84284268 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   84284273 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    9007331 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   75276947 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   75276947 |  244 | `	if( pEntry == 0 ){` |
|   27363880 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   47913072 |  247 | `	return (SyHashEntry *)pEntry;` |
|   42148296 |  248 | `}` |
|     508652 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     508657 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     413996 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     207509 |  254 | `	}else{` |
|      94666 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     508657 |  257 | `	if( pEntry->pNextCollide ){` |
|       4389 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2187 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     508657 |  261 | `	if( pHash->pLast == pEntry ){` |
|     499113 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     250166 |  263 | `	}` |
|     508657 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     508657 |  265 | `	pHash->nEntry--;` |
|     508657 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     508657 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     508657 |  272 | `	return rc;` |
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
|     508212 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     508217 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     508217 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     508217 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3430170 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3430175 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3430175 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   26431704 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   26431709 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3429909 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3429909 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   23001805 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   23001805 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   23001805 |  328 | `	return (SyHashEntry *)pEntry;` |
|   13215857 |  329 | `}` |
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
|       4853 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4839 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4839 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4839 |  348 | `		pEntry = pEntry->pNext;` |
|       2420 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|     100718 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100723 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100723 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100723 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100723 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18643411 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18542693 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18542693 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18542693 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18542693 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8946280 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4473103 |  375 | `		}` |
|   18542693 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18542693 |  378 | `		pEntry = pEntry->pNext;` |
|    9271349 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100723 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100723 |  382 | `	pHash->apBucket = apNew;` |
|     100723 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100723 |  384 | `	return SXRET_OK;` |
|      50364 |  385 | `}` |
|   22598346 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   22598351 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   22598351 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   22598351 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   14194628 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    7097231 |  393 | `	}` |
|   22598351 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   22598351 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|     881705 |  399 | `		pHash->pLast->pNext = pEntry;` |
|     881705 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|     881705 |  401 | `		pHash->pLast = pEntry;` |
|     440855 |  402 | `	}else{` |
|   21716651 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   22598351 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1214095 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1214095 |  408 | `		pHash->pLast = pEntry;` |
|     607147 |  409 | `	}` |
|   22598351 |  410 | `	pHash->nEntry++;` |
|   22598351 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   22598346 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   22598351 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100723 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100723 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50359 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   22598351 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   22598351 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   22598351 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   22598351 |  435 | `	pEntry->pHash = pHash;` |
|   22598351 |  436 | `	pEntry->pKey = pKey;` |
|   22598351 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   22598351 |  438 | `	pEntry->pUserData = pUserData;` |
|   22598351 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   22598351 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   22598351 |  442 | `	return rc;` |
|   11299790 |  443 | `}` |
|   21454888 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   21454893 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|    1143458 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  455 | `{` |
|    1143463 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          5 |  457 | `}` |
|     548658 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     548663 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
