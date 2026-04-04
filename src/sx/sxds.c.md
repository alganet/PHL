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
|  14189982 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  14189984 |   16 | `	pSet->nSize = 0 ;` |
|  14189984 |   17 | `	pSet->nUsed = 0;` |
|  14189984 |   18 | `	pSet->nCursor = 0;` |
|  14189984 |   19 | `	pSet->eSize = ElemSize;` |
|  14189984 |   20 | `	pSet->pAllocator = pAllocator;` |
|  14189984 |   21 | `	pSet->pBase =  0;` |
|  14189984 |   22 | `	pSet->pUserData = 0;` |
|  14189984 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  23702464 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  23702466 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3847266 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3847266 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3736788 |   34 | `			pSet->nSize = 4;` |
|   1868393 |   35 | `		}` |
|   3847266 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3847266 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3847266 |   40 | `		pSet->pBase = pNew;` |
|   3847266 |   41 | `		pSet->nSize <<= 1;` |
|   1923632 |   42 | `	}` |
|  23702466 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 176665638 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  23702466 |   45 | `	pSet->nUsed++;` |
|  23702466 |   46 | `	return SXRET_OK;` |
|  11851256 |   47 |  |
|    872950 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    872952 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    872952 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    872952 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    872952 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    872952 |   60 | `	pSet->nSize = nItem;` |
|    872952 |   61 | `	return SXRET_OK;` |
|    436477 |   62 |  |
|   1297704 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1297706 |   65 | `	pSet->nUsed   = 0;` |
|   1297706 |   66 | `	pSet->nCursor = 0;` |
|   1297706 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     42716 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     42718 |   71 | `	pSet->nCursor = 0;` |
|     42718 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     46644 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     46646 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     17574 |   79 | `		pSet->nCursor = 0;` |
|     17574 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     29074 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     29074 |   83 | `	if( ppEntry ){` |
|     29074 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     14536 |   85 | `	}` |
|     29074 |   86 | `	pSet->nCursor++;` |
|     29074 |   87 | `	return SXRET_OK;` |
|     23324 |   88 |  |
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
|    145942 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    145944 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|    145944 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8331356 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8331358 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8331358 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4254110 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2127054 |  112 | `	}` |
|   8331358 |  113 | `	pSet->pBase = 0;` |
|   8331358 |  114 | `	pSet->nUsed = 0;` |
|   8331358 |  115 | `	pSet->nCursor = 0;` |
|   8331358 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4626042 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4626044 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4625954 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4625954 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2313023 |  126 |  |
|   3226806 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3226808 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2144404 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1082406 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1082406 |  135 | `	pSet->nUsed--;` |
|   1082406 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1082406 |  137 | `	return pData;` |
|   1613405 |  138 |  |
|  10187848 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  10187850 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  10187850 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  10187850 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5094141 |  148 |  |
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
|    184812 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    184814 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    184814 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    184814 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    184814 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    184814 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    184814 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    184814 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    184814 |  180 | `	pHash->nEntry = 0;` |
|    184814 |  181 | `	pHash->apBucket = apNew;` |
|    184814 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    184814 |  183 | `	return SXRET_OK;` |
|     92408 |  184 |  |
|     29774 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     29776 |  193 | `	pEntry = pHash->pList;` |
|     16608 |  194 | `	for(;;){` |
|     33218 |  195 | `		if( pHash->nEntry == 0 ){` |
|     29776 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3444 |  198 | `		pNext = pEntry->pNext;` |
|      3444 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3444 |  200 | `		pEntry = pNext;` |
|      3444 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     29776 |  203 | `	if( pHash->apBucket ){` |
|     29776 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     14887 |  205 | `	}` |
|     29776 |  206 | `	pHash->apBucket = 0;` |
|     29776 |  207 | `	pHash->nBucketSize = 0;` |
|     29776 |  208 | `	pHash->pAllocator = 0;` |
|     29776 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  11403322 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  11403324 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  11403324 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  10328925 |  218 | `	for(;;){` |
|  20674842 |  219 | `		if( pEntry == 0 ){` |
|   6309100 |  220 | `			break;` |
|         - |  221 | `		}` |
|  16912726 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5094228 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5094226 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   9271520 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   6309100 |  229 | `	return 0;` |
|   5701927 |  230 |  |
|  11514744 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  11514746 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    111434 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  11403314 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  11403314 |  244 | `	if( pEntry == 0 ){` |
|   6309100 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5094216 |  247 | `	return (SyHashEntry *)pEntry;` |
|   5757638 |  248 |  |
|     84492 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     84494 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     64492 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     32247 |  254 | `	}else{` |
|     20004 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     84494 |  257 | `	if( pEntry->pNextCollide ){` |
|      4307 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2153 |  259 | `	}` |
|     84494 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     84494 |  261 | `	pHash->nEntry--;` |
|     84494 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     84494 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     84494 |  268 | `	return rc;` |
|         2 |  269 |  |
|        10 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|        12 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        12 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|        12 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        12 |  284 | `	return rc;` |
|         7 |  285 |  |
|     84482 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     84484 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     84484 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     84484 |  296 | `	return rc;` |
|         2 |  297 |  |
|    269218 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    269220 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    269220 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2028126 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2028128 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    268786 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    268786 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   1759344 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   1759344 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   1759344 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1014065 |  325 |  |
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
|      1709 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1699 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1699 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1699 |  344 | `		pEntry = pEntry->pNext;` |
|       850 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     22986 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     22988 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     22988 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     22988 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     22988 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2918828 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2895842 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2895842 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2895842 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2895842 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1383885 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    691952 |  371 | `		}` |
|   2895842 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2895842 |  374 | `		pEntry = pEntry->pNext;` |
|   1447922 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     22988 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     22988 |  378 | `	pHash->apBucket = apNew;` |
|     22988 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     22988 |  380 | `	return SXRET_OK;` |
|     11495 |  381 |  |
|   2792578 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2792580 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2792580 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2792580 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1843433 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    921740 |  389 | `	}` |
|   2792580 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2792580 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2792580 |  393 | `	if( pHash->nEntry == 0 ){` |
|    116550 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     58274 |  395 | `	}` |
|   2792580 |  396 | `	pHash->nEntry++;` |
|   2792580 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2792578 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2792580 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     22988 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     22988 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     11493 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2792580 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2792580 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2792580 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2792580 |  421 | `	pEntry->pHash = pHash;` |
|   2792580 |  422 | `	pEntry->pKey = pKey;` |
|   2792580 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2792580 |  424 | `	pEntry->pUserData = pUserData;` |
|   2792580 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2792580 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2792580 |  428 | `	return rc;` |
|   1396291 |  429 |  |
|    110408 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    110410 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
