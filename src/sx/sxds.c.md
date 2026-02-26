# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 272/287 lines (94.77%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "sxtypes.h"` |
|        - |    7 | `#include "sxmacros.h"` |
|        - |    8 | `#include "sxset.h"` |
|        - |    9 | `#include "sxmem.h"` |
|        - |   10 | `#include "sxhashtable.h"` |
|        - |   11 | `#include "sxhash.h"` |
|        - |   12 | `#include "sxstr.h"` |
|        - |   13 |  |
|  5076476 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|        2 |   15 |  |
|  5076478 |   16 | `	pSet->nSize = 0 ;` |
|  5076478 |   17 | `	pSet->nUsed = 0;` |
|  5076478 |   18 | `	pSet->nCursor = 0;` |
|  5076478 |   19 | `	pSet->eSize = ElemSize;` |
|  5076478 |   20 | `	pSet->pAllocator = pAllocator;` |
|  5076478 |   21 | `	pSet->pBase =  0;` |
|  5076478 |   22 | `	pSet->pUserData = 0;` |
|  5076478 |   23 | `	return SXRET_OK;` |
|        2 |   24 |  |
|  8359500 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|        2 |   26 |  |
|        - |   27 | `	unsigned char *zbase;` |
|  8359502 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|        - |   29 | `		void *pNew;` |
|  1015966 |   30 | `		if( pSet->pAllocator == 0 ){` |
|      ! 0 |   31 | `			return  SXERR_LOCKED;` |
|        - |   32 | `		}` |
|  1015966 |   33 | `		if( pSet->nSize <= 0 ){` |
|   963394 |   34 | `			pSet->nSize = 4;` |
|   481696 |   35 | `		}` |
|  1015966 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|  1015966 |   37 | `		if( pNew == 0 ){` |
|      ! 0 |   38 | `			return SXERR_MEM;` |
|        - |   39 | `		}` |
|  1015966 |   40 | `		pSet->pBase = pNew;` |
|  1015966 |   41 | `		pSet->nSize <<= 1;` |
|   507982 |   42 | `	}` |
|  8359502 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 55578786 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  8359502 |   45 | `	pSet->nUsed++;` |
|  8359502 |   46 | `	return SXRET_OK;` |
|  4179774 |   47 |  |
|   377948 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|        2 |   49 |  |
|   377950 |   50 | `	if( pSet->nSize > 0 ){` |
|      ! 0 |   51 | `		return SXERR_LOCKED;` |
|        - |   52 | `	}` |
|   377950 |   53 | `	if( nItem < 8 ){` |
|      ! 0 |   54 | `		nItem = 8;` |
|      ! 0 |   55 | `	}` |
|   377950 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   377950 |   57 | `	if( pSet->pBase == 0 ){` |
|      ! 0 |   58 | `		return SXERR_MEM;` |
|        - |   59 | `	}` |
|   377950 |   60 | `	pSet->nSize = nItem;` |
|   377950 |   61 | `	return SXRET_OK;` |
|   188976 |   62 |  |
|   763716 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|        2 |   64 |  |
|   763718 |   65 | `	pSet->nUsed   = 0;` |
|   763718 |   66 | `	pSet->nCursor = 0;` |
|   763718 |   67 | `	return SXRET_OK;` |
|        2 |   68 |  |
|    31924 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|        2 |   70 |  |
|    31926 |   71 | `	pSet->nCursor = 0;` |
|    31926 |   72 | `	return SXRET_OK;` |
|        2 |   73 |  |
|    34902 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|        2 |   75 |  |
|        - |   76 | `	register unsigned char *zSrc;` |
|    34904 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|        - |   78 | `		/* Reset cursor */` |
|    12702 |   79 | `		pSet->nCursor = 0;` |
|    12702 |   80 | `		return SXERR_EOF;` |
|        - |   81 | `	}` |
|    22204 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|    22204 |   83 | `	if( ppEntry ){` |
|    22204 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|    11101 |   85 | `	}` |
|    22204 |   86 | `	pSet->nCursor++;` |
|    22204 |   87 | `	return SXRET_OK;` |
|    17453 |   88 |  |
|        - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        8 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|        1 |   91 |  |
|        - |   92 | `	register unsigned char *zSrc;` |
|        9 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|        3 |   94 | `		return 0;` |
|        - |   95 | `	}` |
|        7 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|        7 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|        5 |   98 |  |
|        - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    47382 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|        2 |  101 |  |
|    47384 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       20 |  103 | `		pSet->nUsed = nNewSize;` |
|        9 |  104 | `	}` |
|    47384 |  105 | `	return SXRET_OK;` |
|        2 |  106 |  |
|  2238750 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|        2 |  108 |  |
|  2238752 |  109 | `	sxi32 rc = SXRET_OK;` |
|  2238752 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|  1202320 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   601159 |  112 | `	}` |
|  2238752 |  113 | `	pSet->pBase = 0;` |
|  2238752 |  114 | `	pSet->nUsed = 0;` |
|  2238752 |  115 | `	pSet->nCursor = 0;` |
|  2238752 |  116 | `	return rc;` |
|        2 |  117 |  |
|  1097958 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|        2 |  119 |  |
|        - |  120 | `	const char *zBase;` |
|  1097960 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       92 |  122 | `		return 0;` |
|        - |  123 | `	}` |
|  1097870 |  124 | `	zBase = (const char *)pSet->pBase;` |
|  1097870 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   548981 |  126 |  |
|   743514 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|        2 |  128 |  |
|        - |  129 | `	const char *zBase;` |
|        - |  130 | `	void *pData;` |
|   743516 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    79690 |  132 | `		return 0;` |
|        - |  133 | `	}` |
|   663828 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   663828 |  135 | `	pSet->nUsed--;` |
|   663828 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   663828 |  137 | `	return pData;` |
|   371759 |  138 |  |
|  5560870 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|        2 |  140 |  |
|        - |  141 | `	const char *zBase;` |
|  5560872 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|        - |  143 | `		/* Out of range */` |
|      ! 0 |  144 | `		return 0;` |
|        - |  145 | `	}` |
|  5560872 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  5560872 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|  2780626 |  148 |  |
|        - |  149 | `/* Private hash entry */` |
|        - |  150 | `struct SyHashEntry_Pr` |
|        - |  151 |  |
|        - |  152 | `	const void *pKey; /* Hash key */` |
|        - |  153 | `	sxu32 nKeyLen;    /* Key length */` |
|        - |  154 | `	void *pUserData;  /* User private data */` |
|        - |  155 | `	/* Private fields */` |
|        - |  156 | `	sxu32 nHash;` |
|        - |  157 | `	SyHash *pHash;` |
|        - |  158 | `	SyHashEntry_Pr *pNext,*pPrev; /* Next and previous entry in the list */` |
|        - |  159 | `	SyHashEntry_Pr *pNextCollide,*pPrevCollide; /* Collision list */` |
|        - |  160 | `};` |
|        - |  161 | `#define INVALID_HASH(H) ((H)->apBucket == 0)` |
|    67604 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|        2 |  163 |  |
|        - |  164 | `	SyHashEntry_Pr **apNew;` |
|        - |  165 | `#if defined(UNTRUST)` |
|        - |  166 | `	if( pHash == 0 ){` |
|        - |  167 | `		return SXERR_EMPTY;` |
|        - |  168 | `	}` |
|        - |  169 | `#endif` |
|        - |  170 | `	/* Allocate a new table */` |
|    67606 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    67606 |  172 | `	if( apNew == 0 ){` |
|      ! 0 |  173 | `		return SXERR_MEM;` |
|        - |  174 | `	}` |
|    67606 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    67606 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    67606 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    67606 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    67606 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    67606 |  180 | `	pHash->nEntry = 0;` |
|    67606 |  181 | `	pHash->apBucket = apNew;` |
|    67606 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    67606 |  183 | `	return SXRET_OK;` |
|    33804 |  184 |  |
|     8770 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|        2 |  186 |  |
|        - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|        - |  188 | `#if defined(UNTRUST)` |
|        - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|        - |  190 | `		return SXERR_EMPTY;` |
|        - |  191 | `	}` |
|        - |  192 | `#endif` |
|     8772 |  193 | `	pEntry = pHash->pList;` |
|     4872 |  194 | `	for(;;){` |
|     9746 |  195 | `		if( pHash->nEntry == 0 ){` |
|     8772 |  196 | `			break;` |
|        - |  197 | `		}` |
|      976 |  198 | `		pNext = pEntry->pNext;` |
|      976 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      976 |  200 | `		pEntry = pNext;` |
|      976 |  201 | `		pHash->nEntry--;` |
|        2 |  202 | `	}` |
|     8772 |  203 | `	if( pHash->apBucket ){` |
|     8772 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     4385 |  205 | `	}` |
|     8772 |  206 | `	pHash->apBucket = 0;` |
|     8772 |  207 | `	pHash->nBucketSize = 0;` |
|     8772 |  208 | `	pHash->pAllocator = 0;` |
|     8772 |  209 | `	return SXRET_OK;` |
|        2 |  210 |  |
|  6844716 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|        2 |  212 |  |
|        - |  213 | `	SyHashEntry_Pr *pEntry;` |
|        - |  214 | `	sxu32 nHash;` |
|        - |  215 |  |
|  6844718 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  6844718 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  6029165 |  218 | `	for(;;){` |
| 12122732 |  219 | `		if( pEntry == 0 ){` |
|  3696920 |  220 | `			break;` |
|        - |  221 | `		}` |
|  9999583 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|  3147802 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|  3147800 |  224 | `				return pEntry;` |
|        - |  225 | `		}` |
|  5278016 |  226 | `		pEntry = pEntry->pNextCollide;` |
|        2 |  227 | `	}` |
|        - |  228 | `	/* Entry not found */` |
|  3696920 |  229 | `	return 0;` |
|  3422624 |  230 |  |
|  6883114 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|        2 |  232 |  |
|        - |  233 | `	SyHashEntry_Pr *pEntry;` |
|        - |  234 | `#if defined(UNTRUST)` |
|        - |  235 | `	if( INVALID_HASH(pHash) ){` |
|        - |  236 | `		return 0;` |
|        - |  237 | `	}` |
|        - |  238 | `#endif` |
|  6883116 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|        - |  240 | `		/* Don't bother hashing,return immediately */` |
|    38406 |  241 | `		return 0;` |
|        - |  242 | `	}` |
|  6844712 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  6844712 |  244 | `	if( pEntry == 0 ){` |
|  3696920 |  245 | `		return 0;` |
|        - |  246 | `	}` |
|  3147794 |  247 | `	return (SyHashEntry *)pEntry;` |
|  3441823 |  248 |  |
|    57170 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|        2 |  250 |  |
|        - |  251 | `	sxi32 rc;` |
|    57172 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    42648 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|    21325 |  254 | `	}else{` |
|    14526 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|        - |  256 | `	}` |
|    57172 |  257 | `	if( pEntry->pNextCollide ){` |
|     3473 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     1736 |  259 | `	}` |
|    57172 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    57172 |  261 | `	pHash->nEntry--;` |
|    57172 |  262 | `	if( ppUserData ){` |
|        - |  263 | `		/* Write a pointer to the user data */` |
|      ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|      ! 0 |  265 | `	}` |
|        - |  266 | `	/* Release the entry */` |
|    57172 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    57172 |  268 | `	return rc;` |
|        2 |  269 |  |
|        6 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|        1 |  271 |  |
|        - |  272 | `	SyHashEntry_Pr *pEntry;` |
|        - |  273 | `	sxi32 rc;` |
|        - |  274 | `#if defined(UNTRUST)` |
|        - |  275 | `	if( INVALID_HASH(pHash) ){` |
|        - |  276 | `		return SXERR_CORRUPT;` |
|        - |  277 | `	}` |
|        - |  278 | `#endif` |
|        7 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        7 |  280 | `	if( pEntry == 0 ){` |
|      ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|        - |  282 | `	}` |
|        7 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        7 |  284 | `	return rc;` |
|        4 |  285 |  |
|    57164 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|        2 |  287 |  |
|    57166 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|        - |  289 | `	sxi32 rc;` |
|        - |  290 | `#if defined(UNTRUST)` |
|        - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|        - |  292 | `		return SXERR_CORRUPT;` |
|        - |  293 | `	}` |
|        - |  294 | `#endif` |
|    57166 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    57166 |  296 | `	return rc;` |
|        2 |  297 |  |
|   101204 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|        2 |  299 |  |
|        - |  300 | `#if defined(UNTRUST)` |
|        - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|        - |  302 | `		return SXERR_CORRUPT;` |
|        - |  303 | `	}` |
|        - |  304 | `#endif` |
|   101206 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   101206 |  306 | `	return SXRET_OK;` |
|        2 |  307 |  |
|   707334 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|        2 |  309 |  |
|        - |  310 | `	SyHashEntry_Pr *pEntry;` |
|        - |  311 | `#if defined(UNTRUST)` |
|        - |  312 | `	if( INVALID_HASH(pHash) ){` |
|        - |  313 | `		return 0;` |
|        - |  314 | `	}` |
|        - |  315 | `#endif` |
|   707336 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   100772 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   100772 |  318 | `		return 0;` |
|        - |  319 | `	}` |
|   606566 |  320 | `	pEntry = pHash->pCurrent;` |
|        - |  321 | `	/* Advance the cursor */` |
|   606566 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|        - |  323 | `	/* Return the current entry */` |
|   606566 |  324 | `	return (SyHashEntry *)pEntry;` |
|   353669 |  325 |  |
|       10 |  326 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|        1 |  327 |  |
|        - |  328 | `	SyHashEntry_Pr *pEntry;` |
|        - |  329 | `	sxi32 rc;` |
|        - |  330 | `	sxu32 n;` |
|        - |  331 | `#if defined(UNTRUST)` |
|        - |  332 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|        - |  333 | `		return 0;` |
|        - |  334 | `	}` |
|        - |  335 | `#endif` |
|       11 |  336 | `	pEntry = pHash->pList;` |
|     1573 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|        - |  338 | `		/* Invoke the callback */` |
|     1563 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|     1563 |  340 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  341 | `			return rc;` |
|        - |  342 | `		}` |
|        - |  343 | `		/* Point to the next entry */` |
|     1563 |  344 | `		pEntry = pEntry->pNext;` |
|      782 |  345 | `	}` |
|       11 |  346 | `	return SXRET_OK;` |
|        6 |  347 |  |
|     9244 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|        2 |  349 |  |
|     9246 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|        - |  351 | `	SyHashEntry_Pr *pEntry;` |
|        - |  352 | `	SyHashEntry_Pr **apNew;` |
|        - |  353 | `	sxu32 n,iBucket;` |
|        - |  354 |  |
|        - |  355 | `	/* Allocate a new larger table */` |
|     9246 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     9246 |  357 | `	if( apNew == 0 ){` |
|        - |  358 | `		/* Not so fatal,simply a performance hit */` |
|      ! 0 |  359 | `		return SXRET_OK;` |
|        - |  360 | `	}` |
|        - |  361 | `	/* Zero the new table */` |
|     9246 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|        - |  363 | `	/* Rehash all entries */` |
|  1262238 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|  1252994 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - |  366 | `		/* Install in the new bucket */` |
|  1252994 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|  1252994 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|  1252994 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   601742 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|   300867 |  371 | `		}` |
|  1252994 |  372 | `		apNew[iBucket] = pEntry;` |
|        - |  373 | `		/* Point to the next entry */` |
|  1252994 |  374 | `		pEntry = pEntry->pNext;` |
|   626498 |  375 | `	}` |
|        - |  376 | `	/* Release the old table and reflect the change */` |
|     9246 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     9246 |  378 | `	pHash->apBucket = apNew;` |
|     9246 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     9246 |  380 | `	return SXRET_OK;` |
|     4624 |  381 |  |
|  1151734 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|        2 |  383 |  |
|  1151736 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|        - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|  1151736 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|  1151736 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   763439 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   381700 |  389 | `	}` |
|  1151736 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|        - |  391 | `	/* Link to the entry list */` |
|  1151736 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|  1151736 |  393 | `	if( pHash->nEntry == 0 ){` |
|    48384 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    24191 |  395 | `	}` |
|  1151736 |  396 | `	pHash->nEntry++;` |
|  1151736 |  397 | `	return SXRET_OK;` |
|        2 |  398 |  |
|  1151734 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|        2 |  400 |  |
|        - |  401 | `	SyHashEntry_Pr *pEntry;` |
|        - |  402 | `	sxi32 rc;` |
|        - |  403 | `#if defined(UNTRUST)` |
|        - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|        - |  405 | `		return SXERR_CORRUPT;` |
|        - |  406 | `	}` |
|        - |  407 | `#endif` |
|  1151736 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     9246 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     9246 |  410 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  411 | `			return rc;` |
|        - |  412 | `		}` |
|     4622 |  413 | `	}` |
|        - |  414 | `	/* Allocate a new hash entry */` |
|  1151736 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|  1151736 |  416 | `	if( pEntry == 0 ){` |
|      ! 0 |  417 | `		return SXERR_MEM;` |
|        - |  418 | `	}` |
|        - |  419 | `	/* Zero the entry */` |
|  1151736 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|  1151736 |  421 | `	pEntry->pHash = pHash;` |
|  1151736 |  422 | `	pEntry->pKey = pKey;` |
|  1151736 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|  1151736 |  424 | `	pEntry->pUserData = pUserData;` |
|  1151736 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|        - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|  1151736 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|  1151736 |  428 | `	return rc;` |
|   575869 |  429 |  |
|    68826 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|        2 |  431 |  |
|        - |  432 | `#if defined(UNTRUST)` |
|        - |  433 | `	if( INVALID_HASH(pHash) ){` |
|        - |  434 | `		return 0;` |
|        - |  435 | `	}` |
|        - |  436 | `#endif` |
|        - |  437 | `	/* Last inserted entry */` |
|    68828 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|        2 |  439 |  |
|        - |  440 |  |
