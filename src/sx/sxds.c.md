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
|  157498082 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  157498087 |   16 | `	pSet->nSize = 0 ;` |
|  157498087 |   17 | `	pSet->nUsed = 0;` |
|  157498087 |   18 | `	pSet->nCursor = 0;` |
|  157498087 |   19 | `	pSet->eSize = ElemSize;` |
|  157498087 |   20 | `	pSet->pAllocator = pAllocator;` |
|  157498087 |   21 | `	pSet->pBase =  0;` |
|  157498087 |   22 | `	pSet->pUserData = 0;` |
|  157498087 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  358290631 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  358290636 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   20711689 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   20711689 |   33 | `		if( pSet->nSize <= 0 ){` |
|   17652145 |   34 | `			pSet->nSize = 4;` |
|    8826070 |   35 | `		}` |
|   20711689 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   20711689 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   20711689 |   40 | `		pSet->pBase = pNew;` |
|   20711689 |   41 | `		pSet->nSize <<= 1;` |
|   10355842 |   42 | `	}` |
|  358290636 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2826969072 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  358290636 |   45 | `	pSet->nUsed++;` |
|  358290636 |   46 | `	return SXRET_OK;` |
|  179145344 |   47 | `}` |
|   17612468 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   17612473 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   17612473 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   17612473 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   17612473 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   17612473 |   60 | `	pSet->nSize = nItem;` |
|   17612473 |   61 | `	return SXRET_OK;` |
|    8806239 |   62 | `}` |
|   26181069 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   26181074 |   65 | `	pSet->nUsed   = 0;` |
|   26181074 |   66 | `	pSet->nCursor = 0;` |
|   26181074 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69728 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69733 |   71 | `	pSet->nCursor = 0;` |
|      69733 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73888 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73893 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30173 |   79 | `		pSet->nCursor = 0;` |
|      30173 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43725 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43725 |   83 | `	if( ppEntry ){` |
|      43725 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21860 |   85 | `	}` |
|      43725 |   86 | `	pSet->nCursor++;` |
|      43725 |   87 | `	return SXRET_OK;` |
|      36949 |   88 | `}` |
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
|    2721866 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2721871 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1181 |  103 | `		pSet->nUsed = nNewSize;` |
|        588 |  104 | `	}` |
|    2721871 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   54289276 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   54289281 |  109 | `	sxi32 rc = SXRET_OK;` |
|   54289281 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   29405111 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   14702553 |  112 | `	}` |
|   54289281 |  113 | `	pSet->pBase = 0;` |
|   54289281 |  114 | `	pSet->nUsed = 0;` |
|   54289281 |  115 | `	pSet->nCursor = 0;` |
|   54289281 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   63856324 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   63856329 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15381 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   63840953 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   63840953 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   31928167 |  126 | `}` |
|    8881114 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    8881119 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2213785 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    6667339 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    6667339 |  135 | `	pSet->nUsed--;` |
|    6667339 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    6667339 |  137 | `	return pData;` |
|    4440562 |  138 | `}` |
|   32330805 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   32330810 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   32330788 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   32330788 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   16165502 |  148 | `}` |
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
|    1765692 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1765697 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1765697 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1765697 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1765697 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1765697 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1765697 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1765697 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1765697 |  180 | `	pHash->nEntry = 0;` |
|    1765697 |  181 | `	pHash->apBucket = apNew;` |
|    1765697 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1765697 |  183 | `	return SXRET_OK;` |
|     882851 |  184 | `}` |
|     409830 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     409835 |  193 | `	pEntry = pHash->pList;` |
|     218750 |  194 | `	for(;;){` |
|     437505 |  195 | `		if( pHash->nEntry == 0 ){` |
|     409835 |  196 | `			break;` |
|          - |  197 | `		}` |
|      27675 |  198 | `		pNext = pEntry->pNext;` |
|      27675 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      27675 |  200 | `		pEntry = pNext;` |
|      27675 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     409835 |  203 | `	if( pHash->apBucket ){` |
|     409835 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     204915 |  205 | `	}` |
|     409835 |  206 | `	pHash->apBucket = 0;` |
|     409835 |  207 | `	pHash->nBucketSize = 0;` |
|     409835 |  208 | `	pHash->pAllocator = 0;` |
|     409835 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   65938883 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   65938888 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   65938888 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   59205429 |  218 | `	for(;;){` |
|  118290887 |  219 | `		if( pEntry == 0 ){` |
|   23929868 |  220 | `			break;` |
|          - |  221 | `		}` |
|  115365470 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   42009174 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   42009025 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   52352004 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   23929868 |  229 | `	return 0;` |
|   32969730 |  230 | `}` |
|   72779865 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   72779870 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    6841339 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   65938536 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   65938536 |  244 | `	if( pEntry == 0 ){` |
|   23929850 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   42008691 |  247 | `	return (SyHashEntry *)pEntry;` |
|   36390221 |  248 | `}` |
|     426754 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     426759 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     350987 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     175496 |  254 | `	}else{` |
|      75777 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     426759 |  257 | `	if( pEntry->pNextCollide ){` |
|       4248 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2123 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     426759 |  261 | `	if( pHash->pLast == pEntry ){` |
|     419043 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     209519 |  263 | `	}` |
|     426759 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     426759 |  265 | `	pHash->nEntry--;` |
|     426759 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     426759 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     426759 |  272 | `	return rc;` |
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
|     426420 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     426425 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     426425 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     426425 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2826862 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2826867 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2826867 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   21074642 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   21074647 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2826601 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2826601 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18248051 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18248051 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18248051 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10537326 |  329 | `}` |
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
|       4025 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4015 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4015 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4015 |  348 | `		pEntry = pEntry->pNext;` |
|       2008 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      91774 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      91779 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      91779 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      91779 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      91779 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14406915 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14315141 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14315141 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14315141 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14315141 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    6895598 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3447681 |  375 | `		}` |
|   14315141 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14315141 |  378 | `		pEntry = pEntry->pNext;` |
|    7157573 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      91779 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      91779 |  382 | `	pHash->apBucket = apNew;` |
|      91779 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      91779 |  384 | `	return SXRET_OK;` |
|      45892 |  385 | `}` |
|   18167466 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   18167471 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   18167471 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   18167471 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11439284 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5719751 |  393 | `	}` |
|   18167471 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   18167471 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   18167419 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   18167471 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     973213 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     973213 |  408 | `		pHash->pLast = pEntry;` |
|     486604 |  409 | `	}` |
|   18167471 |  410 | `	pHash->nEntry++;` |
|   18167471 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   18167466 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   18167471 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      91779 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      91779 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45887 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   18167471 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   18167471 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   18167471 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   18167471 |  435 | `	pEntry->pHash = pHash;` |
|   18167471 |  436 | `	pEntry->pKey = pKey;` |
|   18167471 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   18167471 |  438 | `	pEntry->pUserData = pUserData;` |
|   18167471 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   18167471 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   18167471 |  442 | `	return rc;` |
|    9083738 |  443 | `}` |
|   18167332 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   18167337 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        134 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          2 |  455 | `{` |
|        136 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          2 |  457 | `}` |
|     466080 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     466085 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
