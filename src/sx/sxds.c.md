# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 290/304 lines (95.39%)

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
|   84821570 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|   84821575 |   16 | `	pSet->nSize = 0 ;` |
|   84821575 |   17 | `	pSet->nUsed = 0;` |
|   84821575 |   18 | `	pSet->nCursor = 0;` |
|   84821575 |   19 | `	pSet->eSize = ElemSize;` |
|   84821575 |   20 | `	pSet->pAllocator = pAllocator;` |
|   84821575 |   21 | `	pSet->pBase =  0;` |
|   84821575 |   22 | `	pSet->pUserData = 0;` |
|   84821575 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  183317315 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  183317320 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   12307263 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   12307263 |   33 | `		if( pSet->nSize <= 0 ){` |
|   10825411 |   34 | `			pSet->nSize = 4;` |
|    5412703 |   35 | `		}` |
|   12307263 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   12307263 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   12307263 |   40 | `		pSet->pBase = pNew;` |
|   12307263 |   41 | `		pSet->nSize <<= 1;` |
|    6153629 |   42 | `	}` |
|  183317320 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 1343543612 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  183317320 |   45 | `	pSet->nUsed++;` |
|  183317320 |   46 | `	return SXRET_OK;` |
|   91658705 |   47 | `}` |
|    8938622 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|    8938627 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|    8938627 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|    8938627 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    8938627 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|    8938627 |   60 | `	pSet->nSize = nItem;` |
|    8938627 |   61 | `	return SXRET_OK;` |
|    4469316 |   62 | `}` |
|   13907877 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   13907882 |   65 | `	pSet->nUsed   = 0;` |
|   13907882 |   66 | `	pSet->nCursor = 0;` |
|   13907882 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69008 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69013 |   71 | `	pSet->nCursor = 0;` |
|      69013 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73218 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73223 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29697 |   79 | `		pSet->nCursor = 0;` |
|      29697 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43531 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43531 |   83 | `	if( ppEntry ){` |
|      43531 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21763 |   85 | `	}` |
|      43531 |   86 | `	pSet->nCursor++;` |
|      43531 |   87 | `	return SXRET_OK;` |
|      36614 |   88 | `}` |
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
|    1418404 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    1418409 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1179 |  103 | `		pSet->nUsed = nNewSize;` |
|        587 |  104 | `	}` |
|    1418409 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   31439156 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   31439161 |  109 | `	sxi32 rc = SXRET_OK;` |
|   31439161 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   16880377 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|    8440186 |  112 | `	}` |
|   31439161 |  113 | `	pSet->pBase = 0;` |
|   31439161 |  114 | `	pSet->nUsed = 0;` |
|   31439161 |  115 | `	pSet->nCursor = 0;` |
|   31439161 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   31946176 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   31946181 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        133 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   31946053 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   31946053 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   15973093 |  126 | `}` |
|    6254892 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    6254897 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2195445 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    4059457 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    4059457 |  135 | `	pSet->nUsed--;` |
|    4059457 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    4059457 |  137 | `	return pData;` |
|    3127451 |  138 | `}` |
|   21578607 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   21578612 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   21578590 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   21578590 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   10789647 |  148 | `}` |
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
|    1174142 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1174147 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1174147 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1174147 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1174147 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1174147 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1174147 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1174147 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1174147 |  180 | `	pHash->nEntry = 0;` |
|    1174147 |  181 | `	pHash->apBucket = apNew;` |
|    1174147 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1174147 |  183 | `	return SXRET_OK;` |
|     587076 |  184 | `}` |
|     311390 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     311395 |  193 | `	pEntry = pHash->pList;` |
|     164724 |  194 | `	for(;;){` |
|     329453 |  195 | `		if( pHash->nEntry == 0 ){` |
|     311395 |  196 | `			break;` |
|          - |  197 | `		}` |
|      18063 |  198 | `		pNext = pEntry->pNext;` |
|      18063 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      18063 |  200 | `		pEntry = pNext;` |
|      18063 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     311395 |  203 | `	if( pHash->apBucket ){` |
|     311395 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     155695 |  205 | `	}` |
|     311395 |  206 | `	pHash->apBucket = 0;` |
|     311395 |  207 | `	pHash->nBucketSize = 0;` |
|     311395 |  208 | `	pHash->pAllocator = 0;` |
|     311395 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   41067045 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   41067050 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   41067050 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   38387823 |  218 | `	for(;;){` |
|   76769994 |  219 | `		if( pEntry == 0 ){` |
|   16271030 |  220 | `			break;` |
|          - |  221 | `		}` |
|   72896745 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   24796062 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   24796025 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   35702949 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   16271030 |  229 | `	return 0;` |
|   20534039 |  230 | `}` |
|   44836683 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   44836688 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    3769965 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   41066728 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   41066728 |  244 | `	if( pEntry == 0 ){` |
|   16271030 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   24795703 |  247 | `	return (SyHashEntry *)pEntry;` |
|   22418858 |  248 | `}` |
|     212098 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     212103 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     169649 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      84827 |  254 | `	}else{` |
|      42459 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     212103 |  257 | `	if( pEntry->pNextCollide ){` |
|       4162 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2079 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     212103 |  261 | `	if( pHash->pLast == pEntry ){` |
|     205341 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     102668 |  263 | `	}` |
|     212103 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     212103 |  265 | `	pHash->nEntry--;` |
|     212103 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|        ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        ! 0 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     212103 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     212103 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        322 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        327 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        327 |  284 | `	if( pEntry == 0 ){` |
|        ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        327 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        327 |  288 | `	return rc;` |
|        166 |  289 | `}` |
|     211776 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     211781 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     211781 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     211781 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    1797516 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    1797521 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    1797521 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   13496436 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   13496441 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    1797255 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    1797255 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   11699191 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   11699191 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   11699191 |  328 | `	return (SyHashEntry *)pEntry;` |
|    6748223 |  329 | `}` |
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
|       3127 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3117 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3117 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3117 |  348 | `		pEntry = pEntry->pNext;` |
|       1559 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      77794 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      77799 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      77799 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      77799 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      77799 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|    9262407 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|    9184613 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|    9184613 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|    9184613 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|    9184613 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    4415017 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    2207481 |  375 | `		}` |
|    9184613 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|    9184613 |  378 | `		pEntry = pEntry->pNext;` |
|    4592309 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      77799 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      77799 |  382 | `	pHash->apBucket = apNew;` |
|      77799 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      77799 |  384 | `	return SXRET_OK;` |
|      38902 |  385 | `}` |
|   11573428 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   11573433 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   11573433 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   11573433 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    7292936 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    3646519 |  393 | `	}` |
|   11573433 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   11573433 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   11573381 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   11573433 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     613225 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     613225 |  408 | `		pHash->pLast = pEntry;` |
|     306610 |  409 | `	}` |
|   11573433 |  410 | `	pHash->nEntry++;` |
|   11573433 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   11573428 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   11573433 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      77799 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      77799 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      38897 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   11573433 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   11573433 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   11573433 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   11573433 |  435 | `	pEntry->pHash = pHash;` |
|   11573433 |  436 | `	pEntry->pKey = pKey;` |
|   11573433 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   11573433 |  438 | `	pEntry->pUserData = pUserData;` |
|   11573433 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   11573433 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   11573433 |  442 | `	return rc;` |
|    5786719 |  443 | `}` |
|   11573300 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   11573305 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        128 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          2 |  455 | `{` |
|        130 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          2 |  457 | `}` |
|     252920 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     252925 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
