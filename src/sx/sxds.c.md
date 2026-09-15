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
|  183844336 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  183844341 |   16 | `	pSet->nSize = 0 ;` |
|  183844341 |   17 | `	pSet->nUsed = 0;` |
|  183844341 |   18 | `	pSet->nCursor = 0;` |
|  183844341 |   19 | `	pSet->eSize = ElemSize;` |
|  183844341 |   20 | `	pSet->pAllocator = pAllocator;` |
|  183844341 |   21 | `	pSet->pBase =  0;` |
|  183844341 |   22 | `	pSet->pUserData = 0;` |
|  183844341 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  416146457 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  416146462 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   23725720 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   23725720 |   33 | `		if( pSet->nSize <= 0 ){` |
|   20169968 |   34 | `			pSet->nSize = 4;` |
|   10085422 |   35 | `		}` |
|   23725720 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   23725720 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   23725720 |   40 | `		pSet->pBase = pNew;` |
|   23725720 |   41 | `		pSet->nSize <<= 1;` |
|   11863298 |   42 | `	}` |
|  416146462 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3292745776 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  416146462 |   45 | `	pSet->nUsed++;` |
|  416146462 |   46 | `	return SXRET_OK;` |
|  208074878 |   47 | `}` |
|   20767186 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   20767191 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   20767191 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   20767191 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   20767191 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   20767191 |   60 | `	pSet->nSize = nItem;` |
|   20767191 |   61 | `	return SXRET_OK;` |
|   10383598 |   62 | `}` |
|   30741620 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   30741625 |   65 | `	pSet->nUsed   = 0;` |
|   30741625 |   66 | `	pSet->nCursor = 0;` |
|   30741625 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69012 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69017 |   71 | `	pSet->nCursor = 0;` |
|      69017 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      69256 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      69261 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29987 |   79 | `		pSet->nCursor = 0;` |
|      29987 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      39279 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      39279 |   83 | `	if( ppEntry ){` |
|      39279 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      19637 |   85 | `	}` |
|      39279 |   86 | `	pSet->nCursor++;` |
|      39279 |   87 | `	return SXRET_OK;` |
|      34633 |   88 | `}` |
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
|    3281448 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3281453 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1213 |  103 | `		pSet->nUsed = nNewSize;` |
|        604 |  104 | `	}` |
|    3281453 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   62547886 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   62547891 |  109 | `	sxi32 rc = SXRET_OK;` |
|   62547891 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   33741402 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   16871139 |  112 | `	}` |
|   62547891 |  113 | `	pSet->pBase = 0;` |
|   62547891 |  114 | `	pSet->nUsed = 0;` |
|   62547891 |  115 | `	pSet->nCursor = 0;` |
|   62547891 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   74258288 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   74258293 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       4005 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   74254293 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   74254293 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   37129149 |  126 | `}` |
|    9743795 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9743800 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2237291 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7506514 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7506514 |  135 | `	pSet->nUsed--;` |
|    7506514 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7506514 |  137 | `	return pData;` |
|    4872196 |  138 | `}` |
|   36168964 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   36168969 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   36168917 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   36168917 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   18087433 |  148 | `}` |
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
|    2140350 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    2140355 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2140355 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    2140355 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2140355 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    2140355 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    2140355 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    2140355 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    2140355 |  180 | `	pHash->nEntry = 0;` |
|    2140355 |  181 | `	pHash->apBucket = apNew;` |
|    2140355 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    2140355 |  183 | `	return SXRET_OK;` |
|    1070229 |  184 | `}` |
|     504192 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     504197 |  193 | `	pEntry = pHash->pList;` |
|     267316 |  194 | `	for(;;){` |
|     534539 |  195 | `		if( pHash->nEntry == 0 ){` |
|     504197 |  196 | `			break;` |
|          - |  197 | `		}` |
|      30347 |  198 | `		pNext = pEntry->pNext;` |
|      30347 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      30347 |  200 | `		pEntry = pNext;` |
|      30347 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     504197 |  203 | `	if( pHash->apBucket ){` |
|     504197 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     252145 |  205 | `	}` |
|     504197 |  206 | `	pHash->apBucket = 0;` |
|     504197 |  207 | `	pHash->nBucketSize = 0;` |
|     504197 |  208 | `	pHash->pAllocator = 0;` |
|     504197 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   74781535 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   74781540 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   74781540 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   67709736 |  218 | `	for(;;){` |
|  135398834 |  219 | `		if( pEntry == 0 ){` |
|   27469272 |  220 | `			break;` |
|          - |  221 | `		}` |
|  131585568 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   47316270 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   47312273 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   60617299 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   27469272 |  229 | `	return 0;` |
|   37395543 |  230 | `}` |
|   82883801 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   82883806 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    8102711 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   74781100 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   74781100 |  244 | `	if( pEntry == 0 ){` |
|   27469254 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   47311851 |  247 | `	return (SyHashEntry *)pEntry;` |
|   41446725 |  248 | `}` |
|     399072 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     399077 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     325547 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     163021 |  254 | `	}else{` |
|      73535 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     399077 |  257 | `	if( pEntry->pNextCollide ){` |
|       4321 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2160 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     399077 |  261 | `	if( pHash->pLast == pEntry ){` |
|     389491 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     195037 |  263 | `	}` |
|     399077 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     399077 |  265 | `	pHash->nEntry--;` |
|     399077 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     399077 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     399077 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        440 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        445 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        445 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        427 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        427 |  288 | `	return rc;` |
|        225 |  289 | `}` |
|     398650 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     398655 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     398655 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     398655 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3367740 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3367745 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3367745 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   26032560 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   26032565 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3367479 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3367479 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   22665091 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   22665091 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   22665091 |  328 | `	return (SyHashEntry *)pEntry;` |
|   13016285 |  329 | `}` |
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
|       4833 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4819 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4819 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4819 |  348 | `		pEntry = pEntry->pNext;` |
|       2410 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|     100608 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100613 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100613 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100613 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100613 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18628517 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18527909 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18527909 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18527909 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18527909 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8946034 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4473037 |  375 | `		}` |
|   18527909 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18527909 |  378 | `		pEntry = pEntry->pNext;` |
|    9263957 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100613 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100613 |  382 | `	pHash->apBucket = apNew;` |
|     100613 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100613 |  384 | `	return SXRET_OK;` |
|      50309 |  385 | `}` |
|   22326816 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   22326821 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   22326821 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   22326821 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   14135771 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    7067908 |  393 | `	}` |
|   22326821 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   22326821 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         63 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         63 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         63 |  401 | `		pHash->pLast = pEntry;` |
|         33 |  402 | `	}else{` |
|   22326761 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   22326821 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1176521 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1176521 |  408 | `		pHash->pLast = pEntry;` |
|     588307 |  409 | `	}` |
|   22326821 |  410 | `	pHash->nEntry++;` |
|   22326821 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   22326816 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   22326821 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100613 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100613 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50304 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   22326821 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   22326821 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   22326821 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   22326821 |  435 | `	pEntry->pHash = pHash;` |
|   22326821 |  436 | `	pEntry->pKey = pKey;` |
|   22326821 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   22326821 |  438 | `	pEntry->pUserData = pUserData;` |
|   22326821 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   22326821 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   22326821 |  442 | `	return rc;` |
|   11163707 |  443 | `}` |
|   22326670 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   22326675 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        146 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          3 |  455 | `{` |
|        149 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          3 |  457 | `}` |
|     438994 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     438999 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
