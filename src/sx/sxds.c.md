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
|  14341892 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  14341894 |   16 | `	pSet->nSize = 0 ;` |
|  14341894 |   17 | `	pSet->nUsed = 0;` |
|  14341894 |   18 | `	pSet->nCursor = 0;` |
|  14341894 |   19 | `	pSet->eSize = ElemSize;` |
|  14341894 |   20 | `	pSet->pAllocator = pAllocator;` |
|  14341894 |   21 | `	pSet->pBase =  0;` |
|  14341894 |   22 | `	pSet->pUserData = 0;` |
|  14341894 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  23739878 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  23739880 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3907858 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3907858 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3798364 |   34 | `			pSet->nSize = 4;` |
|   1899181 |   35 | `		}` |
|   3907858 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3907858 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3907858 |   40 | `		pSet->pBase = pNew;` |
|   3907858 |   41 | `		pSet->nSize <<= 1;` |
|   1953928 |   42 | `	}` |
|  23739880 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 176224436 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  23739880 |   45 | `	pSet->nUsed++;` |
|  23739880 |   46 | `	return SXRET_OK;` |
|  11869963 |   47 |  |
|    862890 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    862892 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    862892 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    862892 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    862892 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    862892 |   60 | `	pSet->nSize = nItem;` |
|    862892 |   61 | `	return SXRET_OK;` |
|    431447 |   62 |  |
|   1326316 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1326318 |   65 | `	pSet->nUsed   = 0;` |
|   1326318 |   66 | `	pSet->nCursor = 0;` |
|   1326318 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     46008 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     46010 |   71 | `	pSet->nCursor = 0;` |
|     46010 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     50090 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     50092 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     18874 |   79 | `		pSet->nCursor = 0;` |
|     18874 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     31220 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     31220 |   83 | `	if( ppEntry ){` |
|     31220 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     15609 |   85 | `	}` |
|     31220 |   86 | `	pSet->nCursor++;` |
|     31220 |   87 | `	return SXRET_OK;` |
|     25047 |   88 |  |
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
|    143392 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    143394 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|    143394 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8449828 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8449830 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8449830 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4312102 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2156050 |  112 | `	}` |
|   8449830 |  113 | `	pSet->pBase = 0;` |
|   8449830 |  114 | `	pSet->nUsed = 0;` |
|   8449830 |  115 | `	pSet->nCursor = 0;` |
|   8449830 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4593300 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4593302 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       108 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4593196 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4593196 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2296652 |  126 |  |
|   3292662 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3292664 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2143602 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1149064 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1149064 |  135 | `	pSet->nUsed--;` |
|   1149064 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1149064 |  137 | `	return pData;` |
|   1646333 |  138 |  |
|  10991772 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  10991774 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  10991774 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  10991774 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5496015 |  148 |  |
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
|    256532 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    256534 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    256534 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    256534 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    256534 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    256534 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    256534 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    256534 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    256534 |  180 | `	pHash->nEntry = 0;` |
|    256534 |  181 | `	pHash->apBucket = apNew;` |
|    256534 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    256534 |  183 | `	return SXRET_OK;` |
|    128268 |  184 |  |
|     77234 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     77236 |  193 | `	pEntry = pHash->pList;` |
|     40509 |  194 | `	for(;;){` |
|     81020 |  195 | `		if( pHash->nEntry == 0 ){` |
|     77236 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3786 |  198 | `		pNext = pEntry->pNext;` |
|      3786 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3786 |  200 | `		pEntry = pNext;` |
|      3786 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     77236 |  203 | `	if( pHash->apBucket ){` |
|     77236 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     38617 |  205 | `	}` |
|     77236 |  206 | `	pHash->apBucket = 0;` |
|     77236 |  207 | `	pHash->nBucketSize = 0;` |
|     77236 |  208 | `	pHash->pAllocator = 0;` |
|     77236 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  11944630 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  11944632 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  11944632 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  10821674 |  218 | `	for(;;){` |
|  21657427 |  219 | `		if( pEntry == 0 ){` |
|   6583064 |  220 | `			break;` |
|         - |  221 | `		}` |
|  17755019 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5361572 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5361570 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   9712797 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   6583064 |  229 | `	return 0;` |
|   5972581 |  230 |  |
|  12409258 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  12409260 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    464652 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  11944610 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  11944610 |  244 | `	if( pEntry == 0 ){` |
|   6583064 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5361548 |  247 | `	return (SyHashEntry *)pEntry;` |
|   6204895 |  248 |  |
|     88668 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     88670 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     67272 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     33637 |  254 | `	}else{` |
|     21400 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     88670 |  257 | `	if( pEntry->pNextCollide ){` |
|      4623 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2311 |  259 | `	}` |
|     88670 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     88670 |  261 | `	pHash->nEntry--;` |
|     88670 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     88670 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     88670 |  268 | `	return rc;` |
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
|     88646 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     88648 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     88648 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     88648 |  296 | `	return rc;` |
|         2 |  297 |  |
|    309280 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    309282 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    309282 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2427638 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2427640 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    308848 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    308848 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2118794 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2118794 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2118794 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1213821 |  325 |  |
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
|      1773 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1763 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1763 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1763 |  344 | `		pEntry = pEntry->pNext;` |
|       882 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     22410 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     22412 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     22412 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     22412 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     22412 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2845292 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2822882 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2822882 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2822882 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2822882 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1349085 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    674567 |  371 | `		}` |
|   2822882 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2822882 |  374 | `		pEntry = pEntry->pNext;` |
|   1411442 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     22412 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     22412 |  378 | `	pHash->apBucket = apNew;` |
|     22412 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     22412 |  380 | `	return SXRET_OK;` |
|     11207 |  381 |  |
|   2893404 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2893406 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2893406 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2893406 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1873811 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    936942 |  389 | `	}` |
|   2893406 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2893406 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2893406 |  393 | `	if( pHash->nEntry == 0 ){` |
|    128938 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     64468 |  395 | `	}` |
|   2893406 |  396 | `	pHash->nEntry++;` |
|   2893406 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2893404 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2893406 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     22412 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     22412 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     11205 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2893406 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2893406 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2893406 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2893406 |  421 | `	pEntry->pHash = pHash;` |
|   2893406 |  422 | `	pEntry->pKey = pKey;` |
|   2893406 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2893406 |  424 | `	pEntry->pUserData = pUserData;` |
|   2893406 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2893406 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2893406 |  428 | `	return rc;` |
|   1446704 |  429 |  |
|    113790 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    113792 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
