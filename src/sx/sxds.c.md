# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 272/287 lines (94.77%)

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
|  11262806 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  11262808 |   16 | `	pSet->nSize = 0 ;` |
|  11262808 |   17 | `	pSet->nUsed = 0;` |
|  11262808 |   18 | `	pSet->nCursor = 0;` |
|  11262808 |   19 | `	pSet->eSize = ElemSize;` |
|  11262808 |   20 | `	pSet->pAllocator = pAllocator;` |
|  11262808 |   21 | `	pSet->pBase =  0;` |
|  11262808 |   22 | `	pSet->pUserData = 0;` |
|  11262808 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  18154436 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  18154438 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3493746 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3493746 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3414058 |   34 | `			pSet->nSize = 4;` |
|   1707028 |   35 | `		}` |
|   3493746 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3493746 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3493746 |   40 | `		pSet->pBase = pNew;` |
|   3493746 |   41 | `		pSet->nSize <<= 1;` |
|   1746872 |   42 | `	}` |
|  18154438 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 135286618 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  18154438 |   45 | `	pSet->nUsed++;` |
|  18154438 |   46 | `	return SXRET_OK;` |
|   9077242 |   47 |  |
|    542756 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    542758 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    542758 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    542758 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    542758 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    542758 |   60 | `	pSet->nSize = nItem;` |
|    542758 |   61 | `	return SXRET_OK;` |
|    271380 |   62 |  |
|   1012830 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1012832 |   65 | `	pSet->nUsed   = 0;` |
|   1012832 |   66 | `	pSet->nCursor = 0;` |
|   1012832 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     38826 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     38828 |   71 | `	pSet->nCursor = 0;` |
|     38828 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     42666 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     42668 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     15694 |   79 | `		pSet->nCursor = 0;` |
|     15694 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     26976 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     26976 |   83 | `	if( ppEntry ){` |
|     26976 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     13487 |   85 | `	}` |
|     26976 |   86 | `	pSet->nCursor++;` |
|     26976 |   87 | `	return SXRET_OK;` |
|     21335 |   88 |  |
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
|     65732 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     65734 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     65734 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   7278910 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   7278912 |  109 | `	sxi32 rc = SXRET_OK;` |
|   7278912 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3763262 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1881630 |  112 | `	}` |
|   7278912 |  113 | `	pSet->pBase = 0;` |
|   7278912 |  114 | `	pSet->nUsed = 0;` |
|   7278912 |  115 | `	pSet->nCursor = 0;` |
|   7278912 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3645746 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3645748 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3645658 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3645658 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1822875 |  126 |  |
|   3099644 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3099646 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2129752 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    969896 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    969896 |  135 | `	pSet->nUsed--;` |
|    969896 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    969896 |  137 | `	return pData;` |
|   1549824 |  138 |  |
|   9544398 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   9544400 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   9544400 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   9544400 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4772439 |  148 |  |
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
|     92930 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     92932 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     92932 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     92932 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     92932 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     92932 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     92932 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     92932 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     92932 |  180 | `	pHash->nEntry = 0;` |
|     92932 |  181 | `	pHash->apBucket = apNew;` |
|     92932 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     92932 |  183 | `	return SXRET_OK;` |
|     46467 |  184 |  |
|     11484 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     11486 |  193 | `	pEntry = pHash->pList;` |
|      7058 |  194 | `	for(;;){` |
|     14118 |  195 | `		if( pHash->nEntry == 0 ){` |
|     11486 |  196 | `			break;` |
|         - |  197 | `		}` |
|      2634 |  198 | `		pNext = pEntry->pNext;` |
|      2634 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      2634 |  200 | `		pEntry = pNext;` |
|      2634 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     11486 |  203 | `	if( pHash->apBucket ){` |
|     11486 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      5742 |  205 | `	}` |
|     11486 |  206 | `	pHash->apBucket = 0;` |
|     11486 |  207 | `	pHash->nBucketSize = 0;` |
|     11486 |  208 | `	pHash->pAllocator = 0;` |
|     11486 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   9449720 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   9449722 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   9449722 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   8081336 |  218 | `	for(;;){` |
|  16265858 |  219 | `		if( pEntry == 0 ){` |
|   5119730 |  220 | `			break;` |
|         - |  221 | `		}` |
|  13310996 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   4329996 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   4329994 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   6816138 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   5119730 |  229 | `	return 0;` |
|   4725126 |  230 |  |
|   9502052 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   9502054 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     52340 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   9449716 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   9449716 |  244 | `	if( pEntry == 0 ){` |
|   5119730 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   4329988 |  247 | `	return (SyHashEntry *)pEntry;` |
|   4751292 |  248 |  |
|     72308 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     72310 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     54336 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     27169 |  254 | `	}else{` |
|     17976 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     72310 |  257 | `	if( pEntry->pNextCollide ){` |
|      4107 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2053 |  259 | `	}` |
|     72310 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     72310 |  261 | `	pHash->nEntry--;` |
|     72310 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     72310 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     72310 |  268 | `	return rc;` |
|         2 |  269 |  |
|         6 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         1 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|         7 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|         7 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|         7 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|         7 |  284 | `	return rc;` |
|         4 |  285 |  |
|     72302 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     72304 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     72304 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     72304 |  296 | `	return rc;` |
|         2 |  297 |  |
|    132840 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    132842 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    132842 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    922698 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    922700 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    132408 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    132408 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    790294 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    790294 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    790294 |  324 | `	return (SyHashEntry *)pEntry;` |
|    461351 |  325 |  |
|        10 |  326 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         1 |  327 |  |
|         - |  328 | `	SyHashEntry_Pr *pEntry;` |
|         - |  329 | `	sxi32 rc;` |
|         - |  330 | `	sxu32 n;` |
|         - |  331 | `#if defined(UNTRUST)` |
|         - |  332 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  333 | `		return 0;` |
|         - |  334 | `	}` |
|         - |  335 | `#endif` |
|        11 |  336 | `	pEntry = pHash->pList;` |
|      1609 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1599 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1599 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1599 |  344 | `		pEntry = pEntry->pNext;` |
|       800 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     13688 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     13690 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     13690 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     13690 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     13690 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1881754 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1868066 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1868066 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1868066 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1868066 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    897013 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    448505 |  371 | `		}` |
|   1868066 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1868066 |  374 | `		pEntry = pEntry->pNext;` |
|    934034 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     13690 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     13690 |  378 | `	pHash->apBucket = apNew;` |
|     13690 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     13690 |  380 | `	return SXRET_OK;` |
|      6846 |  381 |  |
|   1692706 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1692708 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1692708 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1692708 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1143033 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    571498 |  389 | `	}` |
|   1692708 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1692708 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1692708 |  393 | `	if( pHash->nEntry == 0 ){` |
|     66810 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     33404 |  395 | `	}` |
|   1692708 |  396 | `	pHash->nEntry++;` |
|   1692708 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1692706 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1692708 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     13690 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     13690 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      6844 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1692708 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1692708 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1692708 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1692708 |  421 | `	pEntry->pHash = pHash;` |
|   1692708 |  422 | `	pEntry->pKey = pKey;` |
|   1692708 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1692708 |  424 | `	pEntry->pUserData = pUserData;` |
|   1692708 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1692708 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1692708 |  428 | `	return rc;` |
|    846355 |  429 |  |
|     89616 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     89618 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
