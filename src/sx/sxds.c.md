# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 289/304 lines (95.07%)

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
|  19103650 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 |  |
|  19103655 |   16 | `	pSet->nSize = 0 ;` |
|  19103655 |   17 | `	pSet->nUsed = 0;` |
|  19103655 |   18 | `	pSet->nCursor = 0;` |
|  19103655 |   19 | `	pSet->eSize = ElemSize;` |
|  19103655 |   20 | `	pSet->pAllocator = pAllocator;` |
|  19103655 |   21 | `	pSet->pBase =  0;` |
|  19103655 |   22 | `	pSet->pUserData = 0;` |
|  19103655 |   23 | `	return SXRET_OK;` |
|         5 |   24 |  |
|  31367865 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  31367870 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4572655 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4572655 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4414389 |   34 | `			pSet->nSize = 4;` |
|   2207192 |   35 | `		}` |
|   4572655 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4572655 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4572655 |   40 | `		pSet->pBase = pNew;` |
|   4572655 |   41 | `		pSet->nSize <<= 1;` |
|   2286325 |   42 | `	}` |
|  31367870 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 234430610 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  31367870 |   45 | `	pSet->nUsed++;` |
|  31367870 |   46 | `	return SXRET_OK;` |
|  15683980 |   47 |  |
|   1279838 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 |  |
|   1279843 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1279843 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1279843 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1279843 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1279843 |   60 | `	pSet->nSize = nItem;` |
|   1279843 |   61 | `	return SXRET_OK;` |
|    639924 |   62 |  |
|   1795705 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 |  |
|   1795710 |   65 | `	pSet->nUsed   = 0;` |
|   1795710 |   66 | `	pSet->nCursor = 0;` |
|   1795710 |   67 | `	return SXRET_OK;` |
|         5 |   68 |  |
|     57950 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 |  |
|     57955 |   71 | `	pSet->nCursor = 0;` |
|     57955 |   72 | `	return SXRET_OK;` |
|         5 |   73 |  |
|     62156 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     62161 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     23997 |   79 | `		pSet->nCursor = 0;` |
|     23997 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     38169 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     38169 |   83 | `	if( ppEntry ){` |
|     38169 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     19082 |   85 | `	}` |
|     38169 |   86 | `	pSet->nCursor++;` |
|     38169 |   87 | `	return SXRET_OK;` |
|     31083 |   88 |  |
|         - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         8 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|         1 |   91 |  |
|         - |   92 | `	register unsigned char *zSrc;` |
|         9 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         3 |   94 | `		return 0;` |
|         - |   95 | `	}` |
|         7 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|         7 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|         5 |   98 |  |
|         - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    207682 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 |  |
|    207687 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       141 |  103 | `		pSet->nUsed = nNewSize;` |
|        68 |  104 | `	}` |
|    207687 |  105 | `	return SXRET_OK;` |
|         5 |  106 |  |
|   9959890 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 |  |
|   9959895 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9959895 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5000981 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2500488 |  112 | `	}` |
|   9959895 |  113 | `	pSet->pBase = 0;` |
|   9959895 |  114 | `	pSet->nUsed = 0;` |
|   9959895 |  115 | `	pSet->nCursor = 0;` |
|   9959895 |  116 | `	return rc;` |
|         5 |  117 |  |
|   5715588 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5715593 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       131 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5715467 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5715467 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2857799 |  126 |  |
|   3596520 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3596525 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2182171 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1414359 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1414359 |  135 | `	pSet->nUsed--;` |
|   1414359 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1414359 |  137 | `	return pData;` |
|   1798265 |  138 |  |
|  13363222 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  13363227 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13363227 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13363227 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6681953 |  148 |  |
|         - |  149 | `/* Private hash entry */` |
|         - |  150 | `struct SyHashEntry_Pr` |
|         - |  151 |  |
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
|    579746 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    579751 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    579751 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    579751 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    579751 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    579751 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    579751 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    579751 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    579751 |  180 | `	pHash->nEntry = 0;` |
|    579751 |  181 | `	pHash->apBucket = apNew;` |
|    579751 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    579751 |  183 | `	return SXRET_OK;` |
|    289878 |  184 |  |
|    103634 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    103639 |  193 | `	pEntry = pHash->pList;` |
|     55530 |  194 | `	for(;;){` |
|    111065 |  195 | `		if( pHash->nEntry == 0 ){` |
|    103639 |  196 | `			break;` |
|         - |  197 | `		}` |
|      7431 |  198 | `		pNext = pEntry->pNext;` |
|      7431 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      7431 |  200 | `		pEntry = pNext;` |
|      7431 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    103639 |  203 | `	if( pHash->apBucket ){` |
|    103639 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     51817 |  205 | `	}` |
|    103639 |  206 | `	pHash->apBucket = 0;` |
|    103639 |  207 | `	pHash->nBucketSize = 0;` |
|    103639 |  208 | `	pHash->pAllocator = 0;` |
|    103639 |  209 | `	return SXRET_OK;` |
|         5 |  210 |  |
|  17434892 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  17434897 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  17434897 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  15615829 |  218 | `	for(;;){` |
|  31126009 |  219 | `		if( pEntry == 0 ){` |
|   9273285 |  220 | `			break;` |
|         - |  221 | `		}` |
|  25933282 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   8161616 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   8161617 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  13691117 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   9273285 |  229 | `	return 0;` |
|   8717961 |  230 |  |
|  18291556 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  18291561 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    856877 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  17434689 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  17434689 |  244 | `	if( pEntry == 0 ){` |
|   9273285 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   8161409 |  247 | `	return (SyHashEntry *)pEntry;` |
|   9146293 |  248 |  |
|    123876 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    123881 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     95447 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     47726 |  254 | `	}else{` |
|     28439 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    123881 |  257 | `	if( pEntry->pNextCollide ){` |
|      5245 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2622 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    123881 |  261 | `	if( pHash->pLast == pEntry ){` |
|    117591 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     58793 |  263 | `	}` |
|    123881 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    123881 |  265 | `	pHash->nEntry--;` |
|    123881 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    123881 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    123881 |  272 | `	return rc;` |
|         5 |  273 |  |
|       208 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 |  |
|         - |  276 | `	SyHashEntry_Pr *pEntry;` |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( INVALID_HASH(pHash) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|       213 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       213 |  284 | `	if( pEntry == 0 ){` |
|       ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|         - |  286 | `	}` |
|       213 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       213 |  288 | `	return rc;` |
|       109 |  289 |  |
|    123668 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 |  |
|    123673 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    123673 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    123673 |  300 | `	return rc;` |
|         5 |  301 |  |
|   1162592 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 |  |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1162597 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1162597 |  310 | `	return SXRET_OK;` |
|         5 |  311 |  |
|   7324678 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 |  |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   7324683 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1162335 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1162335 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   6162353 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   6162353 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   6162353 |  328 | `	return (SyHashEntry *)pEntry;` |
|   3662344 |  329 |  |
|        10 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         1 |  331 |  |
|         - |  332 | `	SyHashEntry_Pr *pEntry;` |
|         - |  333 | `	sxi32 rc;` |
|         - |  334 | `	sxu32 n;` |
|         - |  335 | `#if defined(UNTRUST)` |
|         - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  337 | `		return 0;` |
|         - |  338 | `	}` |
|         - |  339 | `#endif` |
|        11 |  340 | `	pEntry = pHash->pList;` |
|      1987 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|      1977 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1977 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|      1977 |  348 | `		pEntry = pEntry->pNext;` |
|       989 |  349 | `	}` |
|        11 |  350 | `	return SXRET_OK;` |
|         6 |  351 |  |
|     29670 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  353 |  |
|     29675 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  355 | `	SyHashEntry_Pr *pEntry;` |
|         - |  356 | `	SyHashEntry_Pr **apNew;` |
|         - |  357 | `	sxu32 n,iBucket;` |
|         - |  358 |  |
|         - |  359 | `	/* Allocate a new larger table */` |
|     29675 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     29675 |  361 | `	if( apNew == 0 ){` |
|         - |  362 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  363 | `		return SXRET_OK;` |
|         - |  364 | `	}` |
|         - |  365 | `	/* Zero the new table */` |
|     29675 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  367 | `	/* Rehash all entries */` |
|   3771179 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3741509 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  370 | `		/* Install in the new bucket */` |
|   3741509 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3741509 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3741509 |  373 | `		if( apNew[iBucket] != 0 ){` |
|   1794752 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    897443 |  375 | `		}` |
|   3741509 |  376 | `		apNew[iBucket] = pEntry;` |
|         - |  377 | `		/* Point to the next entry */` |
|   3741509 |  378 | `		pEntry = pEntry->pNext;` |
|   1870757 |  379 | `	}` |
|         - |  380 | `	/* Release the old table and reflect the change */` |
|     29675 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     29675 |  382 | `	pHash->apBucket = apNew;` |
|     29675 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     29675 |  384 | `	return SXRET_OK;` |
|     14840 |  385 |  |
|   4945634 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 |  |
|   4945639 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   4945639 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4945639 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2796889 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1398450 |  393 | `	}` |
|   4945639 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   4945639 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        33 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        33 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        33 |  401 | `		pHash->pLast = pEntry;` |
|        17 |  402 | `	}else{` |
|   4945607 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   4945639 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    311713 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    311713 |  408 | `		pHash->pLast = pEntry;` |
|    155854 |  409 | `	}` |
|   4945639 |  410 | `	pHash->nEntry++;` |
|   4945639 |  411 | `	return SXRET_OK;` |
|         5 |  412 |  |
|   4945634 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 |  |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   4945639 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     29675 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     29675 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     14835 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   4945639 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4945639 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   4945639 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4945639 |  435 | `	pEntry->pHash = pHash;` |
|   4945639 |  436 | `	pEntry->pKey = pKey;` |
|   4945639 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   4945639 |  438 | `	pEntry->pUserData = pUserData;` |
|   4945639 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4945639 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   4945639 |  442 | `	return rc;` |
|   2472822 |  443 |  |
|   4945564 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 |  |
|   4945569 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|         5 |  447 |  |
|         - |  448 | `/*` |
|         - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|         - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|         - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|         - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|         - |  453 | ` */` |
|        70 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  455 |  |
|        72 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|         2 |  457 |  |
|    160422 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 |  |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    160427 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 |  |
|         - |  468 |  |
