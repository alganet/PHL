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
|  145302616 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  145302621 |   16 | `	pSet->nSize = 0 ;` |
|  145302621 |   17 | `	pSet->nUsed = 0;` |
|  145302621 |   18 | `	pSet->nCursor = 0;` |
|  145302621 |   19 | `	pSet->eSize = ElemSize;` |
|  145302621 |   20 | `	pSet->pAllocator = pAllocator;` |
|  145302621 |   21 | `	pSet->pBase =  0;` |
|  145302621 |   22 | `	pSet->pUserData = 0;` |
|  145302621 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  325398284 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  325398289 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   18949271 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   18949271 |   33 | `		if( pSet->nSize <= 0 ){` |
|   16179211 |   34 | `			pSet->nSize = 4;` |
|    8089603 |   35 | `		}` |
|   18949271 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   18949271 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   18949271 |   40 | `		pSet->pBase = pNew;` |
|   18949271 |   41 | `		pSet->nSize <<= 1;` |
|    9474633 |   42 | `	}` |
|  325398289 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2412824355 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  325398289 |   45 | `	pSet->nUsed++;` |
|  325398289 |   46 | `	return SXRET_OK;` |
|  162699188 |   47 | `}` |
|   16052122 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   16052127 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   16052127 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   16052127 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   16052127 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   16052127 |   60 | `	pSet->nSize = nItem;` |
|   16052127 |   61 | `	return SXRET_OK;` |
|    8026066 |   62 | `}` |
|   23048875 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   23048880 |   65 | `	pSet->nUsed   = 0;` |
|   23048880 |   66 | `	pSet->nCursor = 0;` |
|   23048880 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69442 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69447 |   71 | `	pSet->nCursor = 0;` |
|      69447 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73644 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73649 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30005 |   79 | `		pSet->nCursor = 0;` |
|      30005 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43649 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43649 |   83 | `	if( ppEntry ){` |
|      43649 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21822 |   85 | `	}` |
|      43649 |   86 | `	pSet->nCursor++;` |
|      43649 |   87 | `	return SXRET_OK;` |
|      36827 |   88 | `}` |
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
|    2638080 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2638085 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1181 |  103 | `		pSet->nUsed = nNewSize;` |
|        588 |  104 | `	}` |
|    2638085 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   49848700 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   49848705 |  109 | `	sxi32 rc = SXRET_OK;` |
|   49848705 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   26637307 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   13318651 |  112 | `	}` |
|   49848705 |  113 | `	pSet->pBase = 0;` |
|   49848705 |  114 | `	pSet->nUsed = 0;` |
|   49848705 |  115 | `	pSet->nCursor = 0;` |
|   49848705 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   58754566 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   58754571 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15933 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   58738643 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   58738643 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   29377288 |  126 | `}` |
|    7992262 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    7992267 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2223961 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    5768311 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    5768311 |  135 | `	pSet->nUsed--;` |
|    5768311 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    5768311 |  137 | `	return pData;` |
|    3996136 |  138 | `}` |
|   29321435 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   29321440 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   29321418 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   29321418 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   14660923 |  148 | `}` |
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
|    1792562 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1792567 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1792567 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1792567 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1792567 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1792567 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1792567 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1792567 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1792567 |  180 | `	pHash->nEntry = 0;` |
|    1792567 |  181 | `	pHash->apBucket = apNew;` |
|    1792567 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1792567 |  183 | `	return SXRET_OK;` |
|     896286 |  184 | `}` |
|     388366 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     388371 |  193 | `	pEntry = pHash->pList;` |
|     206902 |  194 | `	for(;;){` |
|     413809 |  195 | `		if( pHash->nEntry == 0 ){` |
|     388371 |  196 | `			break;` |
|          - |  197 | `		}` |
|      25443 |  198 | `		pNext = pEntry->pNext;` |
|      25443 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      25443 |  200 | `		pEntry = pNext;` |
|      25443 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     388371 |  203 | `	if( pHash->apBucket ){` |
|     388371 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     194183 |  205 | `	}` |
|     388371 |  206 | `	pHash->apBucket = 0;` |
|     388371 |  207 | `	pHash->nBucketSize = 0;` |
|     388371 |  208 | `	pHash->pAllocator = 0;` |
|     388371 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   62151517 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   62151522 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   62151522 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   57441411 |  218 | `	for(;;){` |
|  114773330 |  219 | `		if( pEntry == 0 ){` |
|   23014939 |  220 | `			break;` |
|          - |  221 | `		}` |
|  111326487 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   39136687 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   39136588 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   52621813 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   23014939 |  229 | `	return 0;` |
|   31076270 |  230 | `}` |
|   68523991 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   68523996 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    6372831 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   62151170 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   62151170 |  244 | `	if( pEntry == 0 ){` |
|   23014921 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   39136254 |  247 | `	return (SyHashEntry *)pEntry;` |
|   34262507 |  248 | `}` |
|     246026 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     246031 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     199283 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      99644 |  254 | `	}else{` |
|      46753 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     246031 |  257 | `	if( pEntry->pNextCollide ){` |
|       4398 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2199 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     246031 |  261 | `	if( pHash->pLast == pEntry ){` |
|     238975 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     119485 |  263 | `	}` |
|     246031 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     246031 |  265 | `	pHash->nEntry--;` |
|     246031 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     246031 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     246031 |  272 | `	return rc;` |
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
|     245692 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     245697 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     245697 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     245697 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2933454 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2933459 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2933459 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   21802230 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   21802235 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2933193 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2933193 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18869047 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18869047 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18869047 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10901120 |  329 | `}` |
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
|      95378 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      95383 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      95383 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      95383 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      95383 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   15023959 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14928581 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14928581 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14928581 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14928581 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    7142515 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3570916 |  375 | `		}` |
|   14928581 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14928581 |  378 | `		pEntry = pEntry->pNext;` |
|    7464293 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      95383 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      95383 |  382 | `	pHash->apBucket = apNew;` |
|      95383 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      95383 |  384 | `	return SXRET_OK;` |
|      47694 |  385 | `}` |
|   18054278 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   18054283 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   18054283 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   18054283 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11335761 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5668005 |  393 | `	}` |
|   18054283 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   18054283 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   18054231 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   18054283 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     973599 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     973599 |  408 | `		pHash->pLast = pEntry;` |
|     486797 |  409 | `	}` |
|   18054283 |  410 | `	pHash->nEntry++;` |
|   18054283 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   18054278 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   18054283 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      95383 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      95383 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      47689 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   18054283 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   18054283 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   18054283 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   18054283 |  435 | `	pEntry->pHash = pHash;` |
|   18054283 |  436 | `	pEntry->pKey = pKey;` |
|   18054283 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   18054283 |  438 | `	pEntry->pUserData = pUserData;` |
|   18054283 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   18054283 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   18054283 |  442 | `	return rc;` |
|    9027144 |  443 | `}` |
|   18054146 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   18054151 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     287610 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     287615 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
