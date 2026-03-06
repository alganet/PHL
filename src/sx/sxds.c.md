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
|  10056572 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  10056574 |   16 | `	pSet->nSize = 0 ;` |
|  10056574 |   17 | `	pSet->nUsed = 0;` |
|  10056574 |   18 | `	pSet->nCursor = 0;` |
|  10056574 |   19 | `	pSet->eSize = ElemSize;` |
|  10056574 |   20 | `	pSet->pAllocator = pAllocator;` |
|  10056574 |   21 | `	pSet->pBase =  0;` |
|  10056574 |   22 | `	pSet->pUserData = 0;` |
|  10056574 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  15873732 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  15873734 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3264488 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3264488 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3203200 |   34 | `			pSet->nSize = 4;` |
|   1601599 |   35 | `		}` |
|   3264488 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3264488 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3264488 |   40 | `		pSet->pBase = pNew;` |
|   3264488 |   41 | `		pSet->nSize <<= 1;` |
|   1632243 |   42 | `	}` |
|  15873734 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 119753730 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  15873734 |   45 | `	pSet->nUsed++;` |
|  15873734 |   46 | `	return SXRET_OK;` |
|   7936890 |   47 |  |
|    438684 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    438686 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    438686 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    438686 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    438686 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    438686 |   60 | `	pSet->nSize = nItem;` |
|    438686 |   61 | `	return SXRET_OK;` |
|    219344 |   62 |  |
|    862588 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|    862590 |   65 | `	pSet->nUsed   = 0;` |
|    862590 |   66 | `	pSet->nCursor = 0;` |
|    862590 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     34970 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     34972 |   71 | `	pSet->nCursor = 0;` |
|     34972 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     38320 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     38322 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     14030 |   79 | `		pSet->nCursor = 0;` |
|     14030 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     24294 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     24294 |   83 | `	if( ppEntry ){` |
|     24294 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     12146 |   85 | `	}` |
|     24294 |   86 | `	pSet->nCursor++;` |
|     24294 |   87 | `	return SXRET_OK;` |
|     19162 |   88 |  |
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
|     55620 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     55622 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     55622 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   6771614 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   6771616 |  109 | `	sxi32 rc = SXRET_OK;` |
|   6771616 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3477776 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1738887 |  112 | `	}` |
|   6771616 |  113 | `	pSet->pBase = 0;` |
|   6771616 |  114 | `	pSet->nUsed = 0;` |
|   6771616 |  115 | `	pSet->nCursor = 0;` |
|   6771616 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3306900 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3306902 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3306812 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3306812 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1653452 |  126 |  |
|   2947934 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   2947936 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2122500 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    825438 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    825438 |  135 | `	pSet->nUsed--;` |
|    825438 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    825438 |  137 | `	return pData;` |
|   1473969 |  138 |  |
|   8532009 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   8532011 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   8532011 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   8532011 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4266225 |  148 |  |
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
|     78986 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     78988 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     78988 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     78988 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     78988 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     78988 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     78988 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     78988 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     78988 |  180 | `	pHash->nEntry = 0;` |
|     78988 |  181 | `	pHash->apBucket = apNew;` |
|     78988 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     78988 |  183 | `	return SXRET_OK;` |
|     39495 |  184 |  |
|      9988 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|      9990 |  193 | `	pEntry = pHash->pList;` |
|      5846 |  194 | `	for(;;){` |
|     11694 |  195 | `		if( pHash->nEntry == 0 ){` |
|      9990 |  196 | `			break;` |
|         - |  197 | `		}` |
|      1706 |  198 | `		pNext = pEntry->pNext;` |
|      1706 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      1706 |  200 | `		pEntry = pNext;` |
|      1706 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|      9990 |  203 | `	if( pHash->apBucket ){` |
|      9990 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      4994 |  205 | `	}` |
|      9990 |  206 | `	pHash->apBucket = 0;` |
|      9990 |  207 | `	pHash->nBucketSize = 0;` |
|      9990 |  208 | `	pHash->pAllocator = 0;` |
|      9990 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   7988130 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   7988132 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   7988132 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   6953313 |  218 | `	for(;;){` |
|  13820946 |  219 | `		if( pEntry == 0 ){` |
|   4328454 |  220 | `			break;` |
|         - |  221 | `		}` |
|  11322203 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   3659682 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   3659680 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   5832816 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   4328454 |  229 | `	return 0;` |
|   3994331 |  230 |  |
|   8032730 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   8032732 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     44608 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   7988126 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   7988126 |  244 | `	if( pEntry == 0 ){` |
|   4328454 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   3659674 |  247 | `	return (SyHashEntry *)pEntry;` |
|   4016631 |  248 |  |
|     63938 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     63940 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     47886 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     23944 |  254 | `	}else{` |
|     16056 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     63940 |  257 | `	if( pEntry->pNextCollide ){` |
|      3763 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      1881 |  259 | `	}` |
|     63940 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     63940 |  261 | `	pHash->nEntry--;` |
|     63940 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     63940 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     63940 |  268 | `	return rc;` |
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
|     63932 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     63934 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     63934 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     63934 |  296 | `	return rc;` |
|         2 |  297 |  |
|    115144 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    115146 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    115146 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    802308 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    802310 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    114712 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    114712 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    687600 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    687600 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    687600 |  324 | `	return (SyHashEntry *)pEntry;` |
|    401156 |  325 |  |
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
|      1579 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1569 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1569 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1569 |  344 | `		pEntry = pEntry->pNext;` |
|       785 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     11180 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     11182 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     11182 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     11182 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     11182 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1531342 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1520162 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1520162 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1520162 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1520162 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    729996 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    364993 |  371 | `		}` |
|   1520162 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1520162 |  374 | `		pEntry = pEntry->pNext;` |
|    760082 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     11182 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     11182 |  378 | `	pHash->apBucket = apNew;` |
|     11182 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     11182 |  380 | `	return SXRET_OK;` |
|      5592 |  381 |  |
|   1379702 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1379704 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1379704 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1379704 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    919901 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    459937 |  389 | `	}` |
|   1379704 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1379704 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1379704 |  393 | `	if( pHash->nEntry == 0 ){` |
|     56628 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     28313 |  395 | `	}` |
|   1379704 |  396 | `	pHash->nEntry++;` |
|   1379704 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1379702 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1379704 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     11182 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     11182 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      5590 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1379704 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1379704 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1379704 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1379704 |  421 | `	pEntry->pHash = pHash;` |
|   1379704 |  422 | `	pEntry->pKey = pKey;` |
|   1379704 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1379704 |  424 | `	pEntry->pUserData = pUserData;` |
|   1379704 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1379704 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1379704 |  428 | `	return rc;` |
|    689853 |  429 |  |
|     78026 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     78028 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
