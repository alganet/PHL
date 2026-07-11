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
|  20728332 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 | `{` |
|  20728337 |   16 | `	pSet->nSize = 0 ;` |
|  20728337 |   17 | `	pSet->nUsed = 0;` |
|  20728337 |   18 | `	pSet->nCursor = 0;` |
|  20728337 |   19 | `	pSet->eSize = ElemSize;` |
|  20728337 |   20 | `	pSet->pAllocator = pAllocator;` |
|  20728337 |   21 | `	pSet->pBase =  0;` |
|  20728337 |   22 | `	pSet->pUserData = 0;` |
|  20728337 |   23 | `	return SXRET_OK;` |
|         5 |   24 | `}` |
|  34353755 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 | `{` |
|         - |   27 | `	unsigned char *zbase;` |
|  34353760 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4852723 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4852723 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4682003 |   34 | `			pSet->nSize = 4;` |
|   2340999 |   35 | `		}` |
|   4852723 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4852723 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4852723 |   40 | `		pSet->pBase = pNew;` |
|   4852723 |   41 | `		pSet->nSize <<= 1;` |
|   2426359 |   42 | `	}` |
|  34353760 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 256824374 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  34353760 |   45 | `	pSet->nUsed++;` |
|  34353760 |   46 | `	return SXRET_OK;` |
|  17176926 |   47 | `}` |
|   1430564 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 | `{` |
|   1430569 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1430569 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1430569 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1430569 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1430569 |   60 | `	pSet->nSize = nItem;` |
|   1430569 |   61 | `	return SXRET_OK;` |
|    715287 |   62 | `}` |
|   2307727 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 | `{` |
|   2307732 |   65 | `	pSet->nUsed   = 0;` |
|   2307732 |   66 | `	pSet->nCursor = 0;` |
|   2307732 |   67 | `	return SXRET_OK;` |
|         5 |   68 | `}` |
|     66758 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 | `{` |
|     66763 |   71 | `	pSet->nCursor = 0;` |
|     66763 |   72 | `	return SXRET_OK;` |
|         5 |   73 | `}` |
|     70952 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 | `{` |
|         - |   76 | `	register unsigned char *zSrc;` |
|     70957 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     28503 |   79 | `		pSet->nCursor = 0;` |
|     28503 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     42459 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     42459 |   83 | `	if( ppEntry ){` |
|     42459 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     21227 |   85 | `	}` |
|     42459 |   86 | `	pSet->nCursor++;` |
|     42459 |   87 | `	return SXRET_OK;` |
|     35481 |   88 | `}` |
|         - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         8 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|         1 |   91 | `{` |
|         - |   92 | `	register unsigned char *zSrc;` |
|         9 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         3 |   94 | `		return 0;` |
|         - |   95 | `	}` |
|         7 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|         7 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|         5 |   98 | `}` |
|         - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    240672 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 | `{` |
|    240677 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       669 |  103 | `		pSet->nUsed = nNewSize;` |
|       332 |  104 | `	}` |
|    240677 |  105 | `	return SXRET_OK;` |
|         5 |  106 | `}` |
|  10579348 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 | `{` |
|  10579353 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10579353 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5314789 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2657392 |  112 | `	}` |
|  10579353 |  113 | `	pSet->pBase = 0;` |
|  10579353 |  114 | `	pSet->nUsed = 0;` |
|  10579353 |  115 | `	pSet->nCursor = 0;` |
|  10579353 |  116 | `	return rc;` |
|         5 |  117 | `}` |
|   6147704 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 | `{` |
|         - |  120 | `	const char *zBase;` |
|   6147709 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       133 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   6147581 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   6147581 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   3073857 |  126 | `}` |
|   3720422 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 | `{` |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3720427 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2192885 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1527547 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1527547 |  135 | `	pSet->nUsed--;` |
|   1527547 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1527547 |  137 | `	return pData;` |
|   1860216 |  138 | `}` |
|  14137872 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	const char *zBase;` |
|  14137877 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  14137877 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  14137877 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   7069309 |  148 | `}` |
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
|    681476 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 | `{` |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    681481 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    681481 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    681481 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    681481 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    681481 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    681481 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    681481 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    681481 |  180 | `	pHash->nEntry = 0;` |
|    681481 |  181 | `	pHash->apBucket = apNew;` |
|    681481 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    681481 |  183 | `	return SXRET_OK;` |
|    340743 |  184 | `}` |
|    154670 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 | `{` |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    154675 |  193 | `	pEntry = pHash->pList;` |
|     82003 |  194 | `	for(;;){` |
|    164011 |  195 | `		if( pHash->nEntry == 0 ){` |
|    154675 |  196 | `			break;` |
|         - |  197 | `		}` |
|      9341 |  198 | `		pNext = pEntry->pNext;` |
|      9341 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      9341 |  200 | `		pEntry = pNext;` |
|      9341 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    154675 |  203 | `	if( pHash->apBucket ){` |
|    154675 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     77335 |  205 | `	}` |
|    154675 |  206 | `	pHash->apBucket = 0;` |
|    154675 |  207 | `	pHash->nBucketSize = 0;` |
|    154675 |  208 | `	pHash->pAllocator = 0;` |
|    154675 |  209 | `	return SXRET_OK;` |
|         5 |  210 | `}` |
|  19170432 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 | `{` |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  19170437 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  19170437 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  17308278 |  218 | `	for(;;){` |
|  34525784 |  219 | `		if( pEntry == 0 ){` |
|  10168703 |  220 | `			break;` |
|         - |  221 | `		}` |
|  28857697 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   9001744 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   9001739 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  15355352 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|  10168703 |  229 | `	return 0;` |
|   9585743 |  230 | `}` |
|  20139506 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 | `{` |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  20139511 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    969303 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  19170213 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  19170213 |  244 | `	if( pEntry == 0 ){` |
|  10168703 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   9001515 |  247 | `	return (SyHashEntry *)pEntry;` |
|  10070280 |  248 | `}` |
|    176194 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 | `{` |
|         - |  251 | `	sxi32 rc;` |
|    176199 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    137687 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     68846 |  254 | `	}else{` |
|     38517 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    176199 |  257 | `	if( pEntry->pNextCollide ){` |
|      5200 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2599 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    176199 |  261 | `	if( pHash->pLast == pEntry ){` |
|    169713 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     84854 |  263 | `	}` |
|    176199 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    176199 |  265 | `	pHash->nEntry--;` |
|    176199 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    176199 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    176199 |  272 | `	return rc;` |
|         5 |  273 | `}` |
|       224 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 | `{` |
|         - |  276 | `	SyHashEntry_Pr *pEntry;` |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( INVALID_HASH(pHash) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|       229 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       229 |  284 | `	if( pEntry == 0 ){` |
|       ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|         - |  286 | `	}` |
|       229 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       229 |  288 | `	return rc;` |
|       117 |  289 | `}` |
|    175970 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 | `{` |
|    175975 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    175975 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    175975 |  300 | `	return rc;` |
|         5 |  301 | `}` |
|   1335056 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 | `{` |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1335061 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1335061 |  310 | `	return SXRET_OK;` |
|         5 |  311 | `}` |
|   8380594 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 | `{` |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   8380599 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1334799 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1334799 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   7045805 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   7045805 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   7045805 |  328 | `	return (SyHashEntry *)pEntry;` |
|   4190302 |  329 | `}` |
|        10 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         1 |  331 | `{` |
|         - |  332 | `	SyHashEntry_Pr *pEntry;` |
|         - |  333 | `	sxi32 rc;` |
|         - |  334 | `	sxu32 n;` |
|         - |  335 | `#if defined(UNTRUST)` |
|         - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  337 | `		return 0;` |
|         - |  338 | `	}` |
|         - |  339 | `#endif` |
|        11 |  340 | `	pEntry = pHash->pList;` |
|      2077 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|      2067 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      2067 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|      2067 |  348 | `		pEntry = pEntry->pNext;` |
|      1034 |  349 | `	}` |
|        11 |  350 | `	return SXRET_OK;` |
|         6 |  351 | `}` |
|     33032 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  353 | `{` |
|     33037 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  355 | `	SyHashEntry_Pr *pEntry;` |
|         - |  356 | `	SyHashEntry_Pr **apNew;` |
|         - |  357 | `	sxu32 n,iBucket;` |
|         - |  358 |  |
|         - |  359 | `	/* Allocate a new larger table */` |
|     33037 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     33037 |  361 | `	if( apNew == 0 ){` |
|         - |  362 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  363 | `		return SXRET_OK;` |
|         - |  364 | `	}` |
|         - |  365 | `	/* Zero the new table */` |
|     33037 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  367 | `	/* Rehash all entries */` |
|   4164109 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   4131077 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  370 | `		/* Install in the new bucket */` |
|   4131077 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   4131077 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   4131077 |  373 | `		if( apNew[iBucket] != 0 ){` |
|   1982754 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    991372 |  375 | `		}` |
|   4131077 |  376 | `		apNew[iBucket] = pEntry;` |
|         - |  377 | `		/* Point to the next entry */` |
|   4131077 |  378 | `		pEntry = pEntry->pNext;` |
|   2065541 |  379 | `	}` |
|         - |  380 | `	/* Release the old table and reflect the change */` |
|     33037 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     33037 |  382 | `	pHash->apBucket = apNew;` |
|     33037 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     33037 |  384 | `	return SXRET_OK;` |
|     16521 |  385 | `}` |
|   5579946 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 | `{` |
|   5579951 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   5579951 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   5579951 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   3121443 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1560821 |  393 | `	}` |
|   5579951 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   5579951 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        51 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        51 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        51 |  401 | `		pHash->pLast = pEntry;` |
|        26 |  402 | `	}else{` |
|   5579901 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   5579951 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    359985 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    359985 |  408 | `		pHash->pLast = pEntry;` |
|    179990 |  409 | `	}` |
|   5579951 |  410 | `	pHash->nEntry++;` |
|   5579951 |  411 | `	return SXRET_OK;` |
|         5 |  412 | `}` |
|   5579946 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 | `{` |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   5579951 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     33037 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     33037 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     16516 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   5579951 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   5579951 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   5579951 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   5579951 |  435 | `	pEntry->pHash = pHash;` |
|   5579951 |  436 | `	pEntry->pKey = pKey;` |
|   5579951 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   5579951 |  438 | `	pEntry->pUserData = pUserData;` |
|   5579951 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   5579951 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   5579951 |  442 | `	return rc;` |
|   2789978 |  443 | `}` |
|   5579830 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 | `{` |
|   5579835 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|         5 |  447 | `}` |
|         - |  448 | `/*` |
|         - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|         - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|         - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|         - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|         - |  453 | ` */` |
|       116 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  455 | `{` |
|       118 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|         2 |  457 | `}` |
|    215982 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 | `{` |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    215987 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 | `}` |
|         - |  468 |  |
