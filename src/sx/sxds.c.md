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
|  160391010 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  160391015 |   16 | `	pSet->nSize = 0 ;` |
|  160391015 |   17 | `	pSet->nUsed = 0;` |
|  160391015 |   18 | `	pSet->nCursor = 0;` |
|  160391015 |   19 | `	pSet->eSize = ElemSize;` |
|  160391015 |   20 | `	pSet->pAllocator = pAllocator;` |
|  160391015 |   21 | `	pSet->pBase =  0;` |
|  160391015 |   22 | `	pSet->pUserData = 0;` |
|  160391015 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  365902353 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  365902358 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   21054099 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   21054099 |   33 | `		if( pSet->nSize <= 0 ){` |
|   17917573 |   34 | `			pSet->nSize = 4;` |
|    8958784 |   35 | `		}` |
|   21054099 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   21054099 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   21054099 |   40 | `		pSet->pBase = pNew;` |
|   21054099 |   41 | `		pSet->nSize <<= 1;` |
|   10527047 |   42 | `	}` |
|  365902358 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2889019922 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  365902358 |   45 | `	pSet->nUsed++;` |
|  365902358 |   46 | `	return SXRET_OK;` |
|  182951205 |   47 | `}` |
|   18172294 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   18172299 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   18172299 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   18172299 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   18172299 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   18172299 |   60 | `	pSet->nSize = nItem;` |
|   18172299 |   61 | `	return SXRET_OK;` |
|    9086152 |   62 | `}` |
|   27000487 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   27000492 |   65 | `	pSet->nUsed   = 0;` |
|   27000492 |   66 | `	pSet->nCursor = 0;` |
|   27000492 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69848 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69853 |   71 | `	pSet->nCursor = 0;` |
|      69853 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      74016 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      74021 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30221 |   79 | `		pSet->nCursor = 0;` |
|      30221 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43805 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43805 |   83 | `	if( ppEntry ){` |
|      43805 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21900 |   85 | `	}` |
|      43805 |   86 | `	pSet->nCursor++;` |
|      43805 |   87 | `	return SXRET_OK;` |
|      37013 |   88 | `}` |
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
|    2740970 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2740975 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1183 |  103 | `		pSet->nUsed = nNewSize;` |
|        589 |  104 | `	}` |
|    2740975 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   55265168 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   55265173 |  109 | `	sxi32 rc = SXRET_OK;` |
|   55265173 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   30104859 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   15052427 |  112 | `	}` |
|   55265173 |  113 | `	pSet->pBase = 0;` |
|   55265173 |  114 | `	pSet->nUsed = 0;` |
|   55265173 |  115 | `	pSet->nCursor = 0;` |
|   55265173 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   65124144 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   65124149 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19197 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   65104957 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   65104957 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   32562077 |  126 | `}` |
|    9007362 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9007367 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2231839 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    6775533 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    6775533 |  135 | `	pSet->nUsed--;` |
|    6775533 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    6775533 |  137 | `	return pData;` |
|    4503686 |  138 | `}` |
|   33523822 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   33523827 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   33523805 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   33523805 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   16762064 |  148 | `}` |
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
|    1766450 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1766455 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1766455 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1766455 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1766455 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1766455 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1766455 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1766455 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1766455 |  180 | `	pHash->nEntry = 0;` |
|    1766455 |  181 | `	pHash->apBucket = apNew;` |
|    1766455 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1766455 |  183 | `	return SXRET_OK;` |
|     883230 |  184 | `}` |
|     410536 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     410541 |  193 | `	pEntry = pHash->pList;` |
|     219135 |  194 | `	for(;;){` |
|     438275 |  195 | `		if( pHash->nEntry == 0 ){` |
|     410541 |  196 | `			break;` |
|          - |  197 | `		}` |
|      27739 |  198 | `		pNext = pEntry->pNext;` |
|      27739 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      27739 |  200 | `		pEntry = pNext;` |
|      27739 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     410541 |  203 | `	if( pHash->apBucket ){` |
|     410541 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     205268 |  205 | `	}` |
|     410541 |  206 | `	pHash->apBucket = 0;` |
|     410541 |  207 | `	pHash->nBucketSize = 0;` |
|     410541 |  208 | `	pHash->pAllocator = 0;` |
|     410541 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   66903795 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   66903800 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   66903800 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   60701759 |  218 | `	for(;;){` |
|  121160443 |  219 | `		if( pEntry == 0 ){` |
|   24232174 |  220 | `			break;` |
|          - |  221 | `		}` |
|  118265932 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   42675598 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   42671631 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   54256648 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   24232174 |  229 | `	return 0;` |
|   33452186 |  230 | `}` |
|   73821723 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   73821728 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    6918377 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   66903356 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   66903356 |  244 | `	if( pEntry == 0 ){` |
|   24232156 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   42671205 |  247 | `	return (SyHashEntry *)pEntry;` |
|   36911150 |  248 | `}` |
|     430448 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     430453 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     354015 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     177010 |  254 | `	}else{` |
|      76443 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     430453 |  257 | `	if( pEntry->pNextCollide ){` |
|       4238 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2118 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     430453 |  261 | `	if( pHash->pLast == pEntry ){` |
|     422725 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     211360 |  263 | `	}` |
|     430453 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     430453 |  265 | `	pHash->nEntry--;` |
|     430453 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     430453 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     430453 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        444 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        449 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        449 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        431 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        431 |  288 | `	return rc;` |
|        227 |  289 | `}` |
|     430022 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     430027 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     430027 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     430027 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2829198 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2829203 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2829203 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   21119444 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   21119449 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2828937 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2828937 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18290517 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18290517 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18290517 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10559727 |  329 | `}` |
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
|       4061 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4051 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4051 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4051 |  348 | `		pEntry = pEntry->pNext;` |
|       2026 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      91776 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      91781 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      91781 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      91781 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      91781 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14407685 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14315909 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14315909 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14315909 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14315909 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    6878409 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3438728 |  375 | `		}` |
|   14315909 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14315909 |  378 | `		pEntry = pEntry->pNext;` |
|    7157957 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      91781 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      91781 |  382 | `	pHash->apBucket = apNew;` |
|      91781 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      91781 |  384 | `	return SXRET_OK;` |
|      45893 |  385 | `}` |
|   18410908 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   18410913 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   18410913 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   18410913 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11654591 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5827471 |  393 | `	}` |
|   18410913 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   18410913 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   18410861 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   18410913 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     973843 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     973843 |  408 | `		pHash->pLast = pEntry;` |
|     486919 |  409 | `	}` |
|   18410913 |  410 | `	pHash->nEntry++;` |
|   18410913 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   18410908 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   18410913 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      91781 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      91781 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45888 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   18410913 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   18410913 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   18410913 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   18410913 |  435 | `	pEntry->pHash = pHash;` |
|   18410913 |  436 | `	pEntry->pKey = pKey;` |
|   18410913 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   18410913 |  438 | `	pEntry->pUserData = pUserData;` |
|   18410913 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   18410913 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   18410913 |  442 | `	return rc;` |
|    9205459 |  443 | `}` |
|   18410774 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   18410779 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     469690 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     469695 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
