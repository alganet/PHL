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
|  13934466 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  13934468 |   16 | `	pSet->nSize = 0 ;` |
|  13934468 |   17 | `	pSet->nUsed = 0;` |
|  13934468 |   18 | `	pSet->nCursor = 0;` |
|  13934468 |   19 | `	pSet->eSize = ElemSize;` |
|  13934468 |   20 | `	pSet->pAllocator = pAllocator;` |
|  13934468 |   21 | `	pSet->pBase =  0;` |
|  13934468 |   22 | `	pSet->pUserData = 0;` |
|  13934468 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  23029612 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  23029614 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3848164 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3848164 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3743132 |   34 | `			pSet->nSize = 4;` |
|   1871565 |   35 | `		}` |
|   3848164 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3848164 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3848164 |   40 | `		pSet->pBase = pNew;` |
|   3848164 |   41 | `		pSet->nSize <<= 1;` |
|   1924081 |   42 | `	}` |
|  23029614 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 171213214 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  23029614 |   45 | `	pSet->nUsed++;` |
|  23029614 |   46 | `	return SXRET_OK;` |
|  11514830 |   47 |  |
|    828034 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    828036 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    828036 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    828036 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    828036 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    828036 |   60 | `	pSet->nSize = nItem;` |
|    828036 |   61 | `	return SXRET_OK;` |
|    414019 |   62 |  |
|   1273326 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1273328 |   65 | `	pSet->nUsed   = 0;` |
|   1273328 |   66 | `	pSet->nCursor = 0;` |
|   1273328 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     43962 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     43964 |   71 | `	pSet->nCursor = 0;` |
|     43964 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     48016 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     48018 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     18010 |   79 | `		pSet->nCursor = 0;` |
|     18010 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     30010 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     30010 |   83 | `	if( ppEntry ){` |
|     30010 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     15004 |   85 | `	}` |
|     30010 |   86 | `	pSet->nCursor++;` |
|     30010 |   87 | `	return SXRET_OK;` |
|     24010 |   88 |  |
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
|    137586 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    137588 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|    137588 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8293660 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8293662 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8293662 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4236168 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2118083 |  112 | `	}` |
|   8293662 |  113 | `	pSet->pBase = 0;` |
|   8293662 |  114 | `	pSet->nUsed = 0;` |
|   8293662 |  115 | `	pSet->nCursor = 0;` |
|   8293662 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4491878 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4491880 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4491790 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4491790 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2245941 |  126 |  |
|   3257636 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3257638 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2141990 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1115650 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1115650 |  135 | `	pSet->nUsed--;` |
|   1115650 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1115650 |  137 | `	return pData;` |
|   1628820 |  138 |  |
|  10374890 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  10374892 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  10374892 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  10374892 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5187654 |  148 |  |
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
|    178620 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    178622 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    178622 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    178622 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    178622 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    178622 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    178622 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    178622 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    178622 |  180 | `	pHash->nEntry = 0;` |
|    178622 |  181 | `	pHash->apBucket = apNew;` |
|    178622 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    178622 |  183 | `	return SXRET_OK;` |
|     89312 |  184 |  |
|     29860 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     29862 |  193 | `	pEntry = pHash->pList;` |
|     16675 |  194 | `	for(;;){` |
|     33352 |  195 | `		if( pHash->nEntry == 0 ){` |
|     29862 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3492 |  198 | `		pNext = pEntry->pNext;` |
|      3492 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3492 |  200 | `		pEntry = pNext;` |
|      3492 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     29862 |  203 | `	if( pHash->apBucket ){` |
|     29862 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     14930 |  205 | `	}` |
|     29862 |  206 | `	pHash->apBucket = 0;` |
|     29862 |  207 | `	pHash->nBucketSize = 0;` |
|     29862 |  208 | `	pHash->pAllocator = 0;` |
|     29862 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  11305842 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  11305844 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  11305844 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  10290084 |  218 | `	for(;;){` |
|  20577175 |  219 | `		if( pEntry == 0 ){` |
|   6252958 |  220 | `			break;` |
|         - |  221 | `		}` |
|  16850532 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5052890 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5052888 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   9271333 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   6252958 |  229 | `	return 0;` |
|   5653187 |  230 |  |
|  11411756 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  11411758 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    105938 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  11305822 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  11305822 |  244 | `	if( pEntry == 0 ){` |
|   6252958 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5052866 |  247 | `	return (SyHashEntry *)pEntry;` |
|   5706144 |  248 |  |
|     85144 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     85146 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     64682 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     32342 |  254 | `	}else{` |
|     20466 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     85146 |  257 | `	if( pEntry->pNextCollide ){` |
|      4479 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2239 |  259 | `	}` |
|     85146 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     85146 |  261 | `	pHash->nEntry--;` |
|     85146 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     85146 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     85146 |  268 | `	return rc;` |
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
|     85122 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     85124 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     85124 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     85124 |  296 | `	return rc;` |
|         2 |  297 |  |
|    261192 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    261194 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    261194 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   1959696 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   1959698 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    260760 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    260760 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   1698940 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   1698940 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   1698940 |  324 | `	return (SyHashEntry *)pEntry;` |
|    979850 |  325 |  |
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
|      1761 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1751 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1751 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1751 |  344 | `		pEntry = pEntry->pNext;` |
|       876 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     21528 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     21530 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     21530 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     21530 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     21530 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2731130 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2709602 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2709602 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2709602 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2709602 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1294837 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    647463 |  371 | `		}` |
|   2709602 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2709602 |  374 | `		pEntry = pEntry->pNext;` |
|   1354802 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     21530 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     21530 |  378 | `	pHash->apBucket = apNew;` |
|     21530 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     21530 |  380 | `	return SXRET_OK;` |
|     10766 |  381 |  |
|   2686132 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2686134 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2686134 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2686134 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1787783 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    893871 |  389 | `	}` |
|   2686134 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2686134 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2686134 |  393 | `	if( pHash->nEntry == 0 ){` |
|    110698 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     55348 |  395 | `	}` |
|   2686134 |  396 | `	pHash->nEntry++;` |
|   2686134 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2686132 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2686134 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     21530 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     21530 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     10764 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2686134 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2686134 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2686134 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2686134 |  421 | `	pEntry->pHash = pHash;` |
|   2686134 |  422 | `	pEntry->pKey = pKey;` |
|   2686134 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2686134 |  424 | `	pEntry->pUserData = pUserData;` |
|   2686134 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2686134 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2686134 |  428 | `	return rc;` |
|   1343068 |  429 |  |
|    109310 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    109312 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
