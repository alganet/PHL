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
|  143902382 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  143902387 |   16 | `	pSet->nSize = 0 ;` |
|  143902387 |   17 | `	pSet->nUsed = 0;` |
|  143902387 |   18 | `	pSet->nCursor = 0;` |
|  143902387 |   19 | `	pSet->eSize = ElemSize;` |
|  143902387 |   20 | `	pSet->pAllocator = pAllocator;` |
|  143902387 |   21 | `	pSet->pBase =  0;` |
|  143902387 |   22 | `	pSet->pUserData = 0;` |
|  143902387 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  322290595 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  322290600 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   18779197 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   18779197 |   33 | `		if( pSet->nSize <= 0 ){` |
|   16039449 |   34 | `			pSet->nSize = 4;` |
|    8019722 |   35 | `		}` |
|   18779197 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   18779197 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   18779197 |   40 | `		pSet->pBase = pNew;` |
|   18779197 |   41 | `		pSet->nSize <<= 1;` |
|    9389596 |   42 | `	}` |
|  322290600 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2389996088 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  322290600 |   45 | `	pSet->nUsed++;` |
|  322290600 |   46 | `	return SXRET_OK;` |
|  161145345 |   47 | `}` |
|   15899626 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   15899631 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   15899631 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   15899631 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   15899631 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   15899631 |   60 | `	pSet->nSize = nItem;` |
|   15899631 |   61 | `	return SXRET_OK;` |
|    7949818 |   62 | `}` |
|   22825209 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   22825214 |   65 | `	pSet->nUsed   = 0;` |
|   22825214 |   66 | `	pSet->nCursor = 0;` |
|   22825214 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69558 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69563 |   71 | `	pSet->nCursor = 0;` |
|      69563 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73800 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73805 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29947 |   79 | `		pSet->nCursor = 0;` |
|      29947 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43863 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43863 |   83 | `	if( ppEntry ){` |
|      43863 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21929 |   85 | `	}` |
|      43863 |   86 | `	pSet->nCursor++;` |
|      43863 |   87 | `	return SXRET_OK;` |
|      36905 |   88 | `}` |
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
|    2612750 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2612755 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1179 |  103 | `		pSet->nUsed = nNewSize;` |
|        587 |  104 | `	}` |
|    2612755 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   49407860 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   49407865 |  109 | `	sxi32 rc = SXRET_OK;` |
|   49407865 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   26398809 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   13199402 |  112 | `	}` |
|   49407865 |  113 | `	pSet->pBase = 0;` |
|   49407865 |  114 | `	pSet->nUsed = 0;` |
|   49407865 |  115 | `	pSet->nCursor = 0;` |
|   49407865 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   58208244 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   58208249 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15781 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   58192473 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   58192473 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   29104127 |  126 | `}` |
|    7936266 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    7936271 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2222715 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    5713561 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    5713561 |  135 | `	pSet->nUsed--;` |
|    5713561 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    5713561 |  137 | `	return pData;` |
|    3968138 |  138 | `}` |
|   28636928 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   28636933 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   28636911 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   28636911 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   14318785 |  148 | `}` |
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
|    1775014 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1775019 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1775019 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1775019 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1775019 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1775019 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1775019 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1775019 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1775019 |  180 | `	pHash->nEntry = 0;` |
|    1775019 |  181 | `	pHash->apBucket = apNew;` |
|    1775019 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1775019 |  183 | `	return SXRET_OK;` |
|     887512 |  184 | `}` |
|     384238 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     384243 |  193 | `	pEntry = pHash->pList;` |
|     204819 |  194 | `	for(;;){` |
|     409643 |  195 | `		if( pHash->nEntry == 0 ){` |
|     384243 |  196 | `			break;` |
|          - |  197 | `		}` |
|      25405 |  198 | `		pNext = pEntry->pNext;` |
|      25405 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      25405 |  200 | `		pEntry = pNext;` |
|      25405 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     384243 |  203 | `	if( pHash->apBucket ){` |
|     384243 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     192119 |  205 | `	}` |
|     384243 |  206 | `	pHash->apBucket = 0;` |
|     384243 |  207 | `	pHash->nBucketSize = 0;` |
|     384243 |  208 | `	pHash->pAllocator = 0;` |
|     384243 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   60690521 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   60690526 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   60690526 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   56287376 |  218 | `	for(;;){` |
|  112725404 |  219 | `		if( pEntry == 0 ){` |
|   22340692 |  220 | `			break;` |
|          - |  221 | `		}` |
|  109559431 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   38349938 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   38349839 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   52034883 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   22340692 |  229 | `	return 0;` |
|   30345777 |  230 | `}` |
|   67000525 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   67000530 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    6310361 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   60690174 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   60690174 |  244 | `	if( pEntry == 0 ){` |
|   22340674 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   38349505 |  247 | `	return (SyHashEntry *)pEntry;` |
|   33500779 |  248 | `}` |
|     232440 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     232445 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     187837 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      93921 |  254 | `	}else{` |
|      44613 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     232445 |  257 | `	if( pEntry->pNextCollide ){` |
|       4396 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2197 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     232445 |  261 | `	if( pHash->pLast == pEntry ){` |
|     225385 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     112690 |  263 | `	}` |
|     232445 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     232445 |  265 | `	pHash->nEntry--;` |
|     232445 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     232445 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     232445 |  272 | `	return rc;` |
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
|     232106 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     232111 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     232111 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     232111 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2913172 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2913177 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2913177 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   21669642 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   21669647 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2912911 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2912911 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18756741 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18756741 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18756741 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10834826 |  329 | `}` |
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
|       3817 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3807 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3807 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3807 |  348 | `		pEntry = pEntry->pNext;` |
|       1904 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      94428 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      94433 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      94433 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      94433 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      94433 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14873441 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14779013 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14779013 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14779013 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14779013 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    7070765 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3535229 |  375 | `		}` |
|   14779013 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14779013 |  378 | `		pEntry = pEntry->pNext;` |
|    7389509 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      94433 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      94433 |  382 | `	pHash->apBucket = apNew;` |
|      94433 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      94433 |  384 | `	return SXRET_OK;` |
|      47219 |  385 | `}` |
|   17843220 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   17843225 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   17843225 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   17843225 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11199563 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5599838 |  393 | `	}` |
|   17843225 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   17843225 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   17843173 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   17843225 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     962571 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     962571 |  408 | `		pHash->pLast = pEntry;` |
|     481283 |  409 | `	}` |
|   17843225 |  410 | `	pHash->nEntry++;` |
|   17843225 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   17843220 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   17843225 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      94433 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      94433 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      47214 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   17843225 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   17843225 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   17843225 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   17843225 |  435 | `	pEntry->pHash = pHash;` |
|   17843225 |  436 | `	pEntry->pKey = pKey;` |
|   17843225 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   17843225 |  438 | `	pEntry->pUserData = pUserData;` |
|   17843225 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   17843225 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   17843225 |  442 | `	return rc;` |
|    8921615 |  443 | `}` |
|   17843088 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   17843093 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     273544 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     273549 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
