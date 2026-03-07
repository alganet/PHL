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
|  10115094 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  10115096 |   16 | `	pSet->nSize = 0 ;` |
|  10115096 |   17 | `	pSet->nUsed = 0;` |
|  10115096 |   18 | `	pSet->nCursor = 0;` |
|  10115096 |   19 | `	pSet->eSize = ElemSize;` |
|  10115096 |   20 | `	pSet->pAllocator = pAllocator;` |
|  10115096 |   21 | `	pSet->pBase =  0;` |
|  10115096 |   22 | `	pSet->pUserData = 0;` |
|  10115096 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  15959682 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  15959684 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3279702 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3279702 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3217932 |   34 | `			pSet->nSize = 4;` |
|   1608965 |   35 | `		}` |
|   3279702 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3279702 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3279702 |   40 | `		pSet->pBase = pNew;` |
|   3279702 |   41 | `		pSet->nSize <<= 1;` |
|   1639850 |   42 | `	}` |
|  15959684 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 120287416 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  15959684 |   45 | `	pSet->nUsed++;` |
|  15959684 |   46 | `	return SXRET_OK;` |
|   7979865 |   47 |  |
|    442272 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    442274 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    442274 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    442274 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    442274 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    442274 |   60 | `	pSet->nSize = nItem;` |
|    442274 |   61 | `	return SXRET_OK;` |
|    221138 |   62 |  |
|    869818 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|    869820 |   65 | `	pSet->nUsed   = 0;` |
|    869820 |   66 | `	pSet->nCursor = 0;` |
|    869820 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     35298 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     35300 |   71 | `	pSet->nCursor = 0;` |
|     35300 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     38704 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     38706 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     14166 |   79 | `		pSet->nCursor = 0;` |
|     14166 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     24542 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     24542 |   83 | `	if( ppEntry ){` |
|     24542 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     12270 |   85 | `	}` |
|     24542 |   86 | `	pSet->nCursor++;` |
|     24542 |   87 | `	return SXRET_OK;` |
|     19354 |   88 |  |
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
|     56028 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     56030 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     56030 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   6803772 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   6803774 |  109 | `	sxi32 rc = SXRET_OK;` |
|   6803774 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3494816 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1747407 |  112 | `	}` |
|   6803774 |  113 | `	pSet->pBase = 0;` |
|   6803774 |  114 | `	pSet->nUsed = 0;` |
|   6803774 |  115 | `	pSet->nCursor = 0;` |
|   6803774 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3316246 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3316248 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3316158 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3316158 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1658125 |  126 |  |
|   2960730 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   2960732 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2122802 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    837932 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    837932 |  135 | `	pSet->nUsed--;` |
|    837932 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    837932 |  137 | `	return pData;` |
|   1480367 |  138 |  |
|   8602222 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   8602224 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   8602224 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   8602224 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4301351 |  148 |  |
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
|     79568 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     79570 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     79570 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     79570 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     79570 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     79570 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     79570 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     79570 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     79570 |  180 | `	pHash->nEntry = 0;` |
|     79570 |  181 | `	pHash->apBucket = apNew;` |
|     79570 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     79570 |  183 | `	return SXRET_OK;` |
|     39786 |  184 |  |
|     10066 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     10068 |  193 | `	pEntry = pHash->pList;` |
|      5903 |  194 | `	for(;;){` |
|     11808 |  195 | `		if( pHash->nEntry == 0 ){` |
|     10068 |  196 | `			break;` |
|         - |  197 | `		}` |
|      1742 |  198 | `		pNext = pEntry->pNext;` |
|      1742 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      1742 |  200 | `		pEntry = pNext;` |
|      1742 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     10068 |  203 | `	if( pHash->apBucket ){` |
|     10068 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      5033 |  205 | `	}` |
|     10068 |  206 | `	pHash->apBucket = 0;` |
|     10068 |  207 | `	pHash->nBucketSize = 0;` |
|     10068 |  208 | `	pHash->pAllocator = 0;` |
|     10068 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   8055268 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   8055270 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   8055270 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   6933625 |  218 | `	for(;;){` |
|  14031738 |  219 | `		if( pEntry == 0 ){` |
|   4365004 |  220 | `			break;` |
|         - |  221 | `		}` |
|  11511739 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   3690270 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   3690268 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   5976470 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   4365004 |  229 | `	return 0;` |
|   4027900 |  230 |  |
|   8100192 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   8100194 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     44932 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   8055264 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   8055264 |  244 | `	if( pEntry == 0 ){` |
|   4365004 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   3690262 |  247 | `	return (SyHashEntry *)pEntry;` |
|   4050362 |  248 |  |
|     64510 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     64512 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     48310 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     24156 |  254 | `	}else{` |
|     16204 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     64512 |  257 | `	if( pEntry->pNextCollide ){` |
|      3845 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      1922 |  259 | `	}` |
|     64512 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     64512 |  261 | `	pHash->nEntry--;` |
|     64512 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     64512 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     64512 |  268 | `	return rc;` |
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
|     64504 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     64506 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     64506 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     64506 |  296 | `	return rc;` |
|         2 |  297 |  |
|    115980 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    115982 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    115982 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    807604 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    807606 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    115548 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    115548 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    692060 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    692060 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    692060 |  324 | `	return (SyHashEntry *)pEntry;` |
|    403804 |  325 |  |
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
|     11276 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     11278 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     11278 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     11278 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     11278 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1544686 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1533410 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1533410 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1533410 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1533410 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    736360 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    368180 |  371 | `		}` |
|   1533410 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1533410 |  374 | `		pEntry = pEntry->pNext;` |
|    766706 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     11278 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     11278 |  378 | `	pHash->apBucket = apNew;` |
|     11278 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     11278 |  380 | `	return SXRET_OK;` |
|      5640 |  381 |  |
|   1391242 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1391244 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1391244 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1391244 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    927758 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    463862 |  389 | `	}` |
|   1391244 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1391244 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1391244 |  393 | `	if( pHash->nEntry == 0 ){` |
|     57054 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     28526 |  395 | `	}` |
|   1391244 |  396 | `	pHash->nEntry++;` |
|   1391244 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1391242 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1391244 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     11278 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     11278 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      5638 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1391244 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1391244 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1391244 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1391244 |  421 | `	pEntry->pHash = pHash;` |
|   1391244 |  422 | `	pEntry->pKey = pKey;` |
|   1391244 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1391244 |  424 | `	pEntry->pUserData = pUserData;` |
|   1391244 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1391244 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1391244 |  428 | `	return rc;` |
|    695623 |  429 |  |
|     78718 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     78720 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
