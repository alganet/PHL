# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 296/315 lines (93.97%)

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
| 165014524 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 | `{` |
| 165014529 |   16 | `	pSet->nSize = 0 ;` |
| 165014529 |   17 | `	pSet->nUsed = 0;` |
| 165014529 |   18 | `	pSet->nCursor = 0;` |
| 165014529 |   19 | `	pSet->eSize = ElemSize;` |
| 165014529 |   20 | `	pSet->pAllocator = pAllocator;` |
| 165014529 |   21 | `	pSet->pBase =  0;` |
| 165014529 |   22 | `	pSet->pUserData = 0;` |
| 165014529 |   23 | `	return SXRET_OK;` |
|         5 |   24 | `}` |
| 108959554 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 | `{` |
|         - |   27 | `	unsigned char *zbase;` |
| 108959559 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|  21640137 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|  21640137 |   33 | `		if( pSet->nSize <= 0 ){` |
|  21175917 |   34 | `			pSet->nSize = 4;` |
|  10591036 |   35 | `		}` |
|  21640137 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|  21640137 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|  21640137 |   40 | `		pSet->pBase = pNew;` |
|  21640137 |   41 | `		pSet->nSize <<= 1;` |
|  10823283 |   42 | `	}` |
| 108959559 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 693908313 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
| 108959559 |   45 | `	pSet->nUsed++;` |
| 108959559 |   46 | `	return SXRET_OK;` |
|  54502573 |   47 | `}` |
|   9350506 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 | `{` |
|   9350511 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   9350511 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   9350511 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   9350511 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   9350511 |   60 | `	pSet->nSize = nItem;` |
|   9350511 |   61 | `	return SXRET_OK;` |
|   4677026 |   62 | `}` |
|  10081557 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 | `{` |
|  10081562 |   65 | `	pSet->nUsed   = 0;` |
|  10081562 |   66 | `	pSet->nCursor = 0;` |
|  10081562 |   67 | `	return SXRET_OK;` |
|         5 |   68 | `}` |
|    137812 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 | `{` |
|    137817 |   71 | `	pSet->nCursor = 0;` |
|    137817 |   72 | `	return SXRET_OK;` |
|         5 |   73 | `}` |
|    143410 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 | `{` |
|         - |   76 | `	register unsigned char *zSrc;` |
|    143415 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     63619 |   79 | `		pSet->nCursor = 0;` |
|     63619 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     79801 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     79801 |   83 | `	if( ppEntry ){` |
|     79801 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     39898 |   85 | `	}` |
|     79801 |   86 | `	pSet->nCursor++;` |
|     79801 |   87 | `	return SXRET_OK;` |
|     71710 |   88 | `}` |
|         - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|       ! 0 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|       ! 0 |   91 | `{` |
|         - |   92 | `	register unsigned char *zSrc;` |
|       ! 0 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|       ! 0 |   94 | `		return 0;` |
|         - |   95 | `	}` |
|       ! 0 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|       ! 0 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|       ! 0 |   98 | `}` |
|         - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    170094 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 | `{` |
|    170099 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|      3093 |  103 | `		pSet->nUsed = nNewSize;` |
|      1544 |  104 | `	}` |
|    170099 |  105 | `	return SXRET_OK;` |
|         5 |  106 | `}` |
|  68193506 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 | `{` |
|  68193511 |  109 | `	sxi32 rc = SXRET_OK;` |
|  68193511 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|  22511038 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|  11258957 |  112 | `	}` |
|  68193511 |  113 | `	pSet->pBase = 0;` |
|  68193511 |  114 | `	pSet->nUsed = 0;` |
|  68193511 |  115 | `	pSet->nCursor = 0;` |
|  68193511 |  116 | `	return rc;` |
|         5 |  117 | `}` |
|  18533946 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 | `{` |
|         - |  120 | `	const char *zBase;` |
|  18533951 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       133 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|  18533823 |  124 | `	zBase = (const char *)pSet->pBase;` |
|  18533823 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   9270003 |  126 | `}` |
|  25675116 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 | `{` |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|  25675121 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   3033722 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|  22641404 |  134 | `	zBase = (const char *)pSet->pBase;` |
|  22641404 |  135 | `	pSet->nUsed--;` |
|  22641404 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|  22641404 |  137 | `	return pData;` |
|  12839442 |  138 | `}` |
|  90885103 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	const char *zBase;` |
|  90885108 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       139 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  90884972 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  90884972 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|  45463182 |  148 | `}` |
|         - |  149 | `/* Private hash entry */` |
|         - |  150 | `struct SyHashEntry_Pr` |
|         - |  151 | `{` |
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
|  10157917 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 | `{` |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|  10157922 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|  10157922 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|  10157922 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|  10157922 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|  10157922 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|  10157922 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|  10157922 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|  10157922 |  180 | `	pHash->nEntry = 0;` |
|  10157922 |  181 | `	pHash->apBucket = apNew;` |
|  10157922 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|  10157922 |  183 | `	return SXRET_OK;` |
|   5080035 |  184 | `}` |
|   5384697 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 | `{` |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|   5384702 |  193 | `	pEntry = pHash->pList;` |
|   8199385 |  194 | `	for(;;){` |
|  16395158 |  195 | `		if( pHash->nEntry == 0 ){` |
|   5384702 |  196 | `			break;` |
|         - |  197 | `		}` |
|  11010461 |  198 | `		pNext = pEntry->pNext;` |
|  11010461 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|  11010461 |  200 | `		pEntry = pNext;` |
|  11010461 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|   5384702 |  203 | `	if( pHash->apBucket ){` |
|   5384702 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|   2692613 |  205 | `	}` |
|   5384702 |  206 | `	pHash->apBucket = 0;` |
|   5384702 |  207 | `	pHash->nBucketSize = 0;` |
|   5384702 |  208 | `	pHash->pAllocator = 0;` |
|   5384702 |  209 | `	return SXRET_OK;` |
|         5 |  210 | `}` |
| 156605891 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 | `{` |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
| 156605896 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
| 156605896 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
| 140933717 |  218 | `	for(;;){` |
| 282410271 |  219 | `		if( pEntry == 0 ){` |
|  67366832 |  220 | `			break;` |
|         - |  221 | `		}` |
| 259898181 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|  89747359 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|  89239069 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
| 125804380 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|  67366832 |  229 | `	return 0;` |
|  78349250 |  230 | `}` |
| 156477176 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 | `{` |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
| 156477181 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|   6734195 |  241 | `		return 0;` |
|         - |  242 | `	}` |
| 149742991 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
| 149742991 |  244 | `	if( pEntry == 0 ){` |
|  67357410 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|  82385586 |  247 | `	return (SyHashEntry *)pEntry;` |
|  78286308 |  248 | `}` |
|   6860401 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 | `{` |
|         - |  251 | `	sxi32 rc;` |
|   6860406 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|   6849384 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|   3424705 |  254 | `	}else{` |
|     11027 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|   6860406 |  257 | `	if( pEntry->pNextCollide ){` |
|   6480996 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|   3242861 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|   6860406 |  261 | `	if( pHash->pLast == pEntry ){` |
|     46639 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     23319 |  263 | `	}` |
|   6860406 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|   6860406 |  265 | `	pHash->nEntry--;` |
|   6860406 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       106 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        52 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|   6860406 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|   6860406 |  272 | `	return rc;` |
|         5 |  273 | `}` |
|   6862905 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 | `{` |
|         - |  276 | `	SyHashEntry_Pr *pEntry;` |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( INVALID_HASH(pHash) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|   6862910 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   6862910 |  284 | `	if( pEntry == 0 ){` |
|      9424 |  285 | `		return SXERR_NOTFOUND;` |
|         - |  286 | `	}` |
|   6853488 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|   6853488 |  288 | `	return rc;` |
|   3431459 |  289 | `}` |
|      6918 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 | `{` |
|      6923 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|      6923 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|      6923 |  300 | `	return rc;` |
|         5 |  301 | `}` |
|  13492512 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 | `{` |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|  13492517 |  309 | `	pHash->pCurrent = pHash->pList;` |
|  13492517 |  310 | `	return SXRET_OK;` |
|         5 |  311 | `}` |
| 102220942 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 | `{` |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
| 102220947 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|  13492203 |  321 | `		pHash->pCurrent = pHash->pList;` |
|  13492203 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|  88728749 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|  88728749 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|  88728749 |  328 | `	return (SyHashEntry *)pEntry;` |
|  51122776 |  329 | `}` |
|        74 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         4 |  331 | `{` |
|         - |  332 | `	SyHashEntry_Pr *pEntry;` |
|         - |  333 | `	sxi32 rc;` |
|         - |  334 | `	sxu32 n;` |
|         - |  335 | `#if defined(UNTRUST)` |
|         - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  337 | `		return 0;` |
|         - |  338 | `	}` |
|         - |  339 | `#endif` |
|        78 |  340 | `	pEntry = pHash->pList;` |
|     83422 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|     83348 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|     83348 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|     83348 |  348 | `		pEntry = pEntry->pNext;` |
|     41676 |  349 | `	}` |
|        78 |  350 | `	return SXRET_OK;` |
|        41 |  351 | `}` |
|         - |  352 | `/*` |
|         - |  353 | ` * Like SyHashForEach but walks the entries from the tail (pLast) back to the` |
|         - |  354 | ` * head via pPrev. The frame's local-variable table is built with SyHashInsert` |
|         - |  355 | ` * (head-push), so its forward pList order is reverse-insertion (LIFO); walking` |
|         - |  356 | ` * it backward yields DECLARATION order, which is what php's get_defined_vars()` |
|         - |  357 | ` * reports. Kept as its own primitive so the shared head-push insert path — and` |
|         - |  358 | ` * the SyHashLastEntry()==pList head contract every RefObj install relies on —` |
|         - |  359 | ` * stays untouched.` |
|         - |  360 | ` */` |
|       144 |  361 | `PH7_PRIVATE sxi32 SyHashForEachReverse(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         5 |  362 | `{` |
|         - |  363 | `	SyHashEntry_Pr *pEntry;` |
|         - |  364 | `	sxi32 rc;` |
|         - |  365 | `	sxu32 n;` |
|         - |  366 | `#if defined(UNTRUST)` |
|         - |  367 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  368 | `		return 0;` |
|         - |  369 | `	}` |
|         - |  370 | `#endif` |
|       149 |  371 | `	pEntry = pHash->pLast;` |
|    170105 |  372 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  373 | `		/* Invoke the callback */` |
|    169961 |  374 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|    169961 |  375 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  376 | `			return rc;` |
|         - |  377 | `		}` |
|         - |  378 | `		/* Point to the previous entry */` |
|    169961 |  379 | `		pEntry = pEntry->pPrev;` |
|     84983 |  380 | `	}` |
|       149 |  381 | `	return SXRET_OK;` |
|        77 |  382 | `}` |
|    171954 |  383 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  384 | `{` |
|    171959 |  385 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  386 | `	SyHashEntry_Pr *pEntry;` |
|         - |  387 | `	SyHashEntry_Pr **apNew;` |
|         - |  388 | `	sxu32 n,iBucket;` |
|         - |  389 |  |
|         - |  390 | `	/* Allocate a new larger table */` |
|    171959 |  391 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|    171959 |  392 | `	if( apNew == 0 ){` |
|         - |  393 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  394 | `		return SXRET_OK;` |
|         - |  395 | `	}` |
|         - |  396 | `	/* Zero the new table */` |
|    171959 |  397 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  398 | `	/* Rehash all entries */` |
|  30292727 |  399 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|  30120773 |  400 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  401 | `		/* Install in the new bucket */` |
|  30120773 |  402 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|  30120773 |  403 | `		pEntry->pNextCollide = apNew[iBucket];` |
|  30120773 |  404 | `		if( apNew[iBucket] != 0 ){` |
|  14517629 |  405 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|   7261827 |  406 | `		}` |
|  30120773 |  407 | `		apNew[iBucket] = pEntry;` |
|         - |  408 | `		/* Point to the next entry */` |
|  30120773 |  409 | `		pEntry = pEntry->pNext;` |
|  15065957 |  410 | `	}` |
|         - |  411 | `	/* Release the old table and reflect the change */` |
|    171959 |  412 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|    171959 |  413 | `	pHash->apBucket = apNew;` |
|    171959 |  414 | `	pHash->nBucketSize = nNewSize;` |
|    171959 |  415 | `	return SXRET_OK;` |
|     86014 |  416 | `}` |
|  61910787 |  417 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  418 | `{` |
|  61910792 |  419 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  420 | `	/* Insert the entry in its corresponding bucket */` |
|  61910792 |  421 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|  61910792 |  422 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|  32872825 |  423 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|  16443509 |  424 | `	}` |
|  61910792 |  425 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  426 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  427 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  428 | `	 * callers that need a FIFO traversal. */` |
|  61910792 |  429 | `	if( bTail && pHash->pLast != 0 ){` |
|  12304019 |  430 | `		pHash->pLast->pNext = pEntry;` |
|  12304019 |  431 | `		pEntry->pPrev = pHash->pLast;` |
|  12304019 |  432 | `		pHash->pLast = pEntry;` |
|   6152603 |  433 | `	}else{` |
|  49606778 |  434 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  435 | `	}` |
|  61910792 |  436 | `	if( pHash->nEntry == 0 ){` |
|         - |  437 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|   4663568 |  438 | `		pHash->pCurrent = pHash->pList;` |
|   4663568 |  439 | `		pHash->pLast = pEntry;` |
|   2332482 |  440 | `	}` |
|  61910792 |  441 | `	pHash->nEntry++;` |
|  61910792 |  442 | `	return SXRET_OK;` |
|         5 |  443 | `}` |
|  61910787 |  444 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  445 | `{` |
|         - |  446 | `	SyHashEntry_Pr *pEntry;` |
|         - |  447 | `	sxi32 rc;` |
|         - |  448 | `#if defined(UNTRUST)` |
|         - |  449 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  450 | `		return SXERR_CORRUPT;` |
|         - |  451 | `	}` |
|         - |  452 | `#endif` |
|  61910792 |  453 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|    171959 |  454 | `		rc = HashGrowTable(&(*pHash));` |
|    171959 |  455 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  456 | `			return rc;` |
|         - |  457 | `		}` |
|     86009 |  458 | `	}` |
|         - |  459 | `	/* Allocate a new hash entry */` |
|  61910792 |  460 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|  61910792 |  461 | `	if( pEntry == 0 ){` |
|       ! 0 |  462 | `		return SXERR_MEM;` |
|         - |  463 | `	}` |
|         - |  464 | `	/* Zero the entry */` |
|  61910792 |  465 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|  61910792 |  466 | `	pEntry->pHash = pHash;` |
|  61910792 |  467 | `	pEntry->pKey = pKey;` |
|  61910792 |  468 | `	pEntry->nKeyLen = nKeyLen;` |
|  61910792 |  469 | `	pEntry->pUserData = pUserData;` |
|  61910792 |  470 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  471 | `	/* Finally insert the entry in its corresponding bucket */` |
|  61910792 |  472 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|  61910792 |  473 | `	return rc;` |
|  30964747 |  474 | `}` |
|  47291026 |  475 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  476 | `{` |
|  47291031 |  477 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|         5 |  478 | `}` |
|         - |  479 | `/*` |
|         - |  480 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|         - |  481 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|         - |  482 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|         - |  483 | ` * object properties, where PHP preserves property-creation order.` |
|         - |  484 | ` */` |
|  14619761 |  485 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  486 | `{` |
|  14619766 |  487 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|         5 |  488 | `}` |
|   1296211 |  489 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  490 | `{` |
|         - |  491 | `#if defined(UNTRUST)` |
|         - |  492 | `	if( INVALID_HASH(pHash) ){` |
|         - |  493 | `		return 0;` |
|         - |  494 | `	}` |
|         - |  495 | `#endif` |
|         - |  496 | `	/* Last inserted entry */` |
|   1296216 |  497 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  498 | `}` |
|         - |  499 |  |
