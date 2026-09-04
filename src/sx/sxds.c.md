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
|  144968394 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  144968399 |   16 | `	pSet->nSize = 0 ;` |
|  144968399 |   17 | `	pSet->nUsed = 0;` |
|  144968399 |   18 | `	pSet->nCursor = 0;` |
|  144968399 |   19 | `	pSet->eSize = ElemSize;` |
|  144968399 |   20 | `	pSet->pAllocator = pAllocator;` |
|  144968399 |   21 | `	pSet->pBase =  0;` |
|  144968399 |   22 | `	pSet->pUserData = 0;` |
|  144968399 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  324698475 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  324698480 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   18901573 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   18901573 |   33 | `		if( pSet->nSize <= 0 ){` |
|   16137067 |   34 | `			pSet->nSize = 4;` |
|    8068531 |   35 | `		}` |
|   18901573 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   18901573 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   18901573 |   40 | `		pSet->pBase = pNew;` |
|   18901573 |   41 | `		pSet->nSize <<= 1;` |
|    9450784 |   42 | `	}` |
|  324698480 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2407837608 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  324698480 |   45 | `	pSet->nUsed++;` |
|  324698480 |   46 | `	return SXRET_OK;` |
|  162349286 |   47 | `}` |
|   16019828 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   16019833 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   16019833 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   16019833 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   16019833 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   16019833 |   60 | `	pSet->nSize = nItem;` |
|   16019833 |   61 | `	return SXRET_OK;` |
|    8009919 |   62 | `}` |
|   22989363 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   22989368 |   65 | `	pSet->nUsed   = 0;` |
|   22989368 |   66 | `	pSet->nCursor = 0;` |
|   22989368 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69442 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69447 |   71 | `	pSet->nCursor = 0;` |
|      69447 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73654 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73659 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29999 |   79 | `		pSet->nCursor = 0;` |
|      29999 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43665 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43665 |   83 | `	if( ppEntry ){` |
|      43665 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21830 |   85 | `	}` |
|      43665 |   86 | `	pSet->nCursor++;` |
|      43665 |   87 | `	return SXRET_OK;` |
|      36832 |   88 | `}` |
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
|    2632752 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2632757 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1181 |  103 | `		pSet->nUsed = nNewSize;` |
|        588 |  104 | `	}` |
|    2632757 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   49734370 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   49734375 |  109 | `	sxi32 rc = SXRET_OK;` |
|   49734375 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   26574157 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   13287076 |  112 | `	}` |
|   49734375 |  113 | `	pSet->pBase = 0;` |
|   49734375 |  114 | `	pSet->nUsed = 0;` |
|   49734375 |  115 | `	pSet->nCursor = 0;` |
|   49734375 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   58640044 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   58640049 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15901 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   58624153 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   58624153 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   29320027 |  126 | `}` |
|    7971988 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    7971993 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2223729 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    5748269 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    5748269 |  135 | `	pSet->nUsed--;` |
|    5748269 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    5748269 |  137 | `	return pData;` |
|    3985999 |  138 | `}` |
|   28803615 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   28803620 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   28803598 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   28803598 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   14402041 |  148 | `}` |
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
|    1787326 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1787331 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1787331 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1787331 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1787331 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1787331 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1787331 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1787331 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1787331 |  180 | `	pHash->nEntry = 0;` |
|    1787331 |  181 | `	pHash->apBucket = apNew;` |
|    1787331 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1787331 |  183 | `	return SXRET_OK;` |
|     893668 |  184 | `}` |
|     385954 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     385959 |  193 | `	pEntry = pHash->pList;` |
|     205696 |  194 | `	for(;;){` |
|     411397 |  195 | `		if( pHash->nEntry == 0 ){` |
|     385959 |  196 | `			break;` |
|          - |  197 | `		}` |
|      25443 |  198 | `		pNext = pEntry->pNext;` |
|      25443 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      25443 |  200 | `		pEntry = pNext;` |
|      25443 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     385959 |  203 | `	if( pHash->apBucket ){` |
|     385959 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     192977 |  205 | `	}` |
|     385959 |  206 | `	pHash->apBucket = 0;` |
|     385959 |  207 | `	pHash->nBucketSize = 0;` |
|     385959 |  208 | `	pHash->pAllocator = 0;` |
|     385959 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   61190045 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   61190050 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   61190050 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   56905833 |  218 | `	for(;;){` |
|  113637063 |  219 | `		if( pEntry == 0 ){` |
|   22535822 |  220 | `			break;` |
|          - |  221 | `		}` |
|  110428151 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   38654332 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   38654233 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   52447018 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   22535822 |  229 | `	return 0;` |
|   30595551 |  230 | `}` |
|   67547921 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   67547926 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    6358233 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   61189698 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   61189698 |  244 | `	if( pEntry == 0 ){` |
|   22535804 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   38653899 |  247 | `	return (SyHashEntry *)pEntry;` |
|   33774489 |  248 | `}` |
|     234932 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     234937 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     189917 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      94961 |  254 | `	}else{` |
|      45025 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     234937 |  257 | `	if( pEntry->pNextCollide ){` |
|       4400 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2200 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     234937 |  261 | `	if( pHash->pLast == pEntry ){` |
|     227879 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     113937 |  263 | `	}` |
|     234937 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     234937 |  265 | `	pHash->nEntry--;` |
|     234937 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     234937 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     234937 |  272 | `	return rc;` |
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
|     234598 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     234603 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     234603 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     234603 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2928174 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2928179 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2928179 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   21764238 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   21764243 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2927913 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2927913 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18836335 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18836335 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18836335 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10882124 |  329 | `}` |
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
|       3829 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3819 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3819 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3819 |  348 | `		pEntry = pEntry->pNext;` |
|       1910 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      95178 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      95183 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      95183 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      95183 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      95183 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14992271 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14897093 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14897093 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14897093 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14897093 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    7127483 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3563983 |  375 | `		}` |
|   14897093 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14897093 |  378 | `		pEntry = pEntry->pNext;` |
|    7448549 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      95183 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      95183 |  382 | `	pHash->apBucket = apNew;` |
|      95183 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      95183 |  384 | `	return SXRET_OK;` |
|      47594 |  385 | `}` |
|   18006140 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   18006145 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   18006145 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   18006145 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11310165 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5655177 |  393 | `	}` |
|   18006145 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   18006145 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   18006093 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   18006145 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     969877 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     969877 |  408 | `		pHash->pLast = pEntry;` |
|     484936 |  409 | `	}` |
|   18006145 |  410 | `	pHash->nEntry++;` |
|   18006145 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   18006140 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   18006145 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      95183 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      95183 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      47589 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   18006145 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   18006145 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   18006145 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   18006145 |  435 | `	pEntry->pHash = pHash;` |
|   18006145 |  436 | `	pEntry->pKey = pKey;` |
|   18006145 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   18006145 |  438 | `	pEntry->pUserData = pUserData;` |
|   18006145 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   18006145 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   18006145 |  442 | `	return rc;` |
|    9003075 |  443 | `}` |
|   18006008 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   18006013 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     276422 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     276427 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
