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
|  14291194 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  14291196 |   16 | `	pSet->nSize = 0 ;` |
|  14291196 |   17 | `	pSet->nUsed = 0;` |
|  14291196 |   18 | `	pSet->nCursor = 0;` |
|  14291196 |   19 | `	pSet->eSize = ElemSize;` |
|  14291196 |   20 | `	pSet->pAllocator = pAllocator;` |
|  14291196 |   21 | `	pSet->pBase =  0;` |
|  14291196 |   22 | `	pSet->pUserData = 0;` |
|  14291196 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  23643904 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  23643906 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3900796 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3900796 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3791950 |   34 | `			pSet->nSize = 4;` |
|   1895974 |   35 | `		}` |
|   3900796 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3900796 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3900796 |   40 | `		pSet->pBase = pNew;` |
|   3900796 |   41 | `		pSet->nSize <<= 1;` |
|   1950397 |   42 | `	}` |
|  23643906 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 175550038 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  23643906 |   45 | `	pSet->nUsed++;` |
|  23643906 |   46 | `	return SXRET_OK;` |
|  11821976 |   47 |  |
|    857766 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    857768 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    857768 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    857768 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    857768 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    857768 |   60 | `	pSet->nSize = nItem;` |
|    857768 |   61 | `	return SXRET_OK;` |
|    428885 |   62 |  |
|   1318080 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1318082 |   65 | `	pSet->nUsed   = 0;` |
|   1318082 |   66 | `	pSet->nCursor = 0;` |
|   1318082 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     45678 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     45680 |   71 | `	pSet->nCursor = 0;` |
|     45680 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     49760 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     49762 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     18734 |   79 | `		pSet->nCursor = 0;` |
|     18734 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     31030 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     31030 |   83 | `	if( ppEntry ){` |
|     31030 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     15514 |   85 | `	}` |
|     31030 |   86 | `	pSet->nCursor++;` |
|     31030 |   87 | `	return SXRET_OK;` |
|     24882 |   88 |  |
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
|    142540 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    142542 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|    142542 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8431670 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8431672 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8431672 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4302472 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2151235 |  112 | `	}` |
|   8431672 |  113 | `	pSet->pBase = 0;` |
|   8431672 |  114 | `	pSet->nUsed = 0;` |
|   8431672 |  115 | `	pSet->nCursor = 0;` |
|   8431672 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4578794 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4578796 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       108 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4578690 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4578690 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2289399 |  126 |  |
|   3289120 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3289122 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2143464 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1145660 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1145660 |  135 | `	pSet->nUsed--;` |
|   1145660 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1145660 |  137 | `	return pData;` |
|   1644562 |  138 |  |
|  10944477 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  10944479 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  10944479 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  10944479 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5472365 |  148 |  |
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
|    255060 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    255062 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    255062 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    255062 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    255062 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    255062 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    255062 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    255062 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    255062 |  180 | `	pHash->nEntry = 0;` |
|    255062 |  181 | `	pHash->apBucket = apNew;` |
|    255062 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    255062 |  183 | `	return SXRET_OK;` |
|    127532 |  184 |  |
|     76824 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     76826 |  193 | `	pEntry = pHash->pList;` |
|     40303 |  194 | `	for(;;){` |
|     80608 |  195 | `		if( pHash->nEntry == 0 ){` |
|     76826 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3784 |  198 | `		pNext = pEntry->pNext;` |
|      3784 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3784 |  200 | `		pEntry = pNext;` |
|      3784 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     76826 |  203 | `	if( pHash->apBucket ){` |
|     76826 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     38412 |  205 | `	}` |
|     76826 |  206 | `	pHash->apBucket = 0;` |
|     76826 |  207 | `	pHash->nBucketSize = 0;` |
|     76826 |  208 | `	pHash->pAllocator = 0;` |
|     76826 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  11872294 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  11872296 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  11872296 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  10799330 |  218 | `	for(;;){` |
|  21563174 |  219 | `		if( pEntry == 0 ){` |
|   6544974 |  220 | `			break;` |
|         - |  221 | `		}` |
|  17681733 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5327326 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5327324 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   9690880 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   6544974 |  229 | `	return 0;` |
|   5936413 |  230 |  |
|  12334254 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  12334256 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    461984 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  11872274 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  11872274 |  244 | `	if( pEntry == 0 ){` |
|   6544974 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5327302 |  247 | `	return (SyHashEntry *)pEntry;` |
|   6167393 |  248 |  |
|     88170 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     88172 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     66918 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     33460 |  254 | `	}else{` |
|     21256 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     88172 |  257 | `	if( pEntry->pNextCollide ){` |
|      4583 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2291 |  259 | `	}` |
|     88172 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     88172 |  261 | `	pHash->nEntry--;` |
|     88172 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     88172 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     88172 |  268 | `	return rc;` |
|         2 |  269 |  |
|        22 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|        24 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        24 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|        24 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        24 |  284 | `	return rc;` |
|        13 |  285 |  |
|     88148 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     88150 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     88150 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     88150 |  296 | `	return rc;` |
|         2 |  297 |  |
|    307542 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    307544 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    307544 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2412516 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2412518 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    307110 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    307110 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2105410 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2105410 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2105410 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1206260 |  325 |  |
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
|      1773 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1763 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1763 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1763 |  344 | `		pEntry = pEntry->pNext;` |
|       882 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     22322 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     22324 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     22324 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     22324 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     22324 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2835220 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2812898 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2812898 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2812898 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2812898 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1344319 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    672151 |  371 | `		}` |
|   2812898 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2812898 |  374 | `		pEntry = pEntry->pNext;` |
|   1406450 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     22324 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     22324 |  378 | `	pHash->apBucket = apNew;` |
|     22324 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     22324 |  380 | `	return SXRET_OK;` |
|     11163 |  381 |  |
|   2880632 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2880634 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2880634 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2880634 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1866377 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    933170 |  389 | `	}` |
|   2880634 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2880634 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2880634 |  393 | `	if( pHash->nEntry == 0 ){` |
|    128214 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     64106 |  395 | `	}` |
|   2880634 |  396 | `	pHash->nEntry++;` |
|   2880634 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2880632 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2880634 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     22324 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     22324 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     11161 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2880634 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2880634 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2880634 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2880634 |  421 | `	pEntry->pHash = pHash;` |
|   2880634 |  422 | `	pEntry->pKey = pKey;` |
|   2880634 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2880634 |  424 | `	pEntry->pUserData = pUserData;` |
|   2880634 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2880634 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2880634 |  428 | `	return rc;` |
|   1440318 |  429 |  |
|    113212 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    113214 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
