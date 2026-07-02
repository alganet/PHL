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
|  20041056 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 | `{` |
|  20041061 |   16 | `	pSet->nSize = 0 ;` |
|  20041061 |   17 | `	pSet->nUsed = 0;` |
|  20041061 |   18 | `	pSet->nCursor = 0;` |
|  20041061 |   19 | `	pSet->eSize = ElemSize;` |
|  20041061 |   20 | `	pSet->pAllocator = pAllocator;` |
|  20041061 |   21 | `	pSet->pBase =  0;` |
|  20041061 |   22 | `	pSet->pUserData = 0;` |
|  20041061 |   23 | `	return SXRET_OK;` |
|         5 |   24 | `}` |
|  33146129 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 | `{` |
|         - |   27 | `	unsigned char *zbase;` |
|  33146134 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4703589 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4703589 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4539401 |   34 | `			pSet->nSize = 4;` |
|   2269698 |   35 | `		}` |
|   4703589 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4703589 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4703589 |   40 | `		pSet->pBase = pNew;` |
|   4703589 |   41 | `		pSet->nSize <<= 1;` |
|   2351792 |   42 | `	}` |
|  33146134 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 248399602 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  33146134 |   45 | `	pSet->nUsed++;` |
|  33146134 |   46 | `	return SXRET_OK;` |
|  16573112 |   47 | `}` |
|   1375916 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 | `{` |
|   1375921 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1375921 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1375921 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1375921 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1375921 |   60 | `	pSet->nSize = nItem;` |
|   1375921 |   61 | `	return SXRET_OK;` |
|    687963 |   62 | `}` |
|   1905891 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 | `{` |
|   1905896 |   65 | `	pSet->nUsed   = 0;` |
|   1905896 |   66 | `	pSet->nCursor = 0;` |
|   1905896 |   67 | `	return SXRET_OK;` |
|         5 |   68 | `}` |
|     58924 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 | `{` |
|     58929 |   71 | `	pSet->nCursor = 0;` |
|     58929 |   72 | `	return SXRET_OK;` |
|         5 |   73 | `}` |
|     63134 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 | `{` |
|         - |   76 | `	register unsigned char *zSrc;` |
|     63139 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     24453 |   79 | `		pSet->nCursor = 0;` |
|     24453 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     38691 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     38691 |   83 | `	if( ppEntry ){` |
|     38691 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     19343 |   85 | `	}` |
|     38691 |   86 | `	pSet->nCursor++;` |
|     38691 |   87 | `	return SXRET_OK;` |
|     31572 |   88 | `}` |
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
|    230540 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 | `{` |
|    230545 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       295 |  103 | `		pSet->nUsed = nNewSize;` |
|       145 |  104 | `	}` |
|    230545 |  105 | `	return SXRET_OK;` |
|         5 |  106 | `}` |
|  10282644 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 | `{` |
|  10282649 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10282649 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5154107 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2577051 |  112 | `	}` |
|  10282649 |  113 | `	pSet->pBase = 0;` |
|  10282649 |  114 | `	pSet->nUsed = 0;` |
|  10282649 |  115 | `	pSet->nCursor = 0;` |
|  10282649 |  116 | `	return rc;` |
|         5 |  117 | `}` |
|   5981996 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 | `{` |
|         - |  120 | `	const char *zBase;` |
|   5982001 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       133 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5981873 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5981873 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2991003 |  126 | `}` |
|   3638364 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 | `{` |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3638369 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2185999 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1452375 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1452375 |  135 | `	pSet->nUsed--;` |
|   1452375 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1452375 |  137 | `	return pData;` |
|   1819187 |  138 | `}` |
|  13737330 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	const char *zBase;` |
|  13737335 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13737335 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13737335 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6869048 |  148 | `}` |
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
|    643128 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 | `{` |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    643133 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    643133 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    643133 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    643133 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    643133 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    643133 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    643133 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    643133 |  180 | `	pHash->nEntry = 0;` |
|    643133 |  181 | `	pHash->apBucket = apNew;` |
|    643133 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    643133 |  183 | `	return SXRET_OK;` |
|    321569 |  184 | `}` |
|    137386 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 | `{` |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    137391 |  193 | `	pEntry = pHash->pList;` |
|     72659 |  194 | `	for(;;){` |
|    145323 |  195 | `		if( pHash->nEntry == 0 ){` |
|    137391 |  196 | `			break;` |
|         - |  197 | `		}` |
|      7937 |  198 | `		pNext = pEntry->pNext;` |
|      7937 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      7937 |  200 | `		pEntry = pNext;` |
|      7937 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    137391 |  203 | `	if( pHash->apBucket ){` |
|    137391 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     68693 |  205 | `	}` |
|    137391 |  206 | `	pHash->apBucket = 0;` |
|    137391 |  207 | `	pHash->nBucketSize = 0;` |
|    137391 |  208 | `	pHash->pAllocator = 0;` |
|    137391 |  209 | `	return SXRET_OK;` |
|         5 |  210 | `}` |
|  18335836 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 | `{` |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  18335841 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  18335841 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  16680491 |  218 | `	for(;;){` |
|  33292952 |  219 | `		if( pEntry == 0 ){` |
|   9757747 |  220 | `			break;` |
|         - |  221 | `		}` |
|  27824007 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   8578104 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   8578099 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  14957116 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   9757747 |  229 | `	return 0;` |
|   9168433 |  230 | `}` |
|  19253684 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 | `{` |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  19253689 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    918063 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  18335631 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  18335631 |  244 | `	if( pEntry == 0 ){` |
|   9757747 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   8577889 |  247 | `	return (SyHashEntry *)pEntry;` |
|   9627357 |  248 | `}` |
|    134010 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 | `{` |
|         - |  251 | `	sxi32 rc;` |
|    134015 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    103953 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     51979 |  254 | `	}else{` |
|     30067 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    134015 |  257 | `	if( pEntry->pNextCollide ){` |
|      5241 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2620 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    134015 |  261 | `	if( pHash->pLast == pEntry ){` |
|    127707 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     63851 |  263 | `	}` |
|    134015 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    134015 |  265 | `	pHash->nEntry--;` |
|    134015 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    134015 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    134015 |  272 | `	return rc;` |
|         5 |  273 | `}` |
|       210 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 | `{` |
|         - |  276 | `	SyHashEntry_Pr *pEntry;` |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( INVALID_HASH(pHash) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|       215 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       215 |  284 | `	if( pEntry == 0 ){` |
|       ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|         - |  286 | `	}` |
|       215 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       215 |  288 | `	return rc;` |
|       110 |  289 | `}` |
|    133800 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 | `{` |
|    133805 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    133805 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    133805 |  300 | `	return rc;` |
|         5 |  301 | `}` |
|   1290382 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 | `{` |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1290387 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1290387 |  310 | `	return SXRET_OK;` |
|         5 |  311 | `}` |
|   8114962 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 | `{` |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   8114967 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1290125 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1290125 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   6824847 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   6824847 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   6824847 |  328 | `	return (SyHashEntry *)pEntry;` |
|   4057486 |  329 | `}` |
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
|      2039 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|      2029 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      2029 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|      2029 |  348 | `		pEntry = pEntry->pNext;` |
|      1015 |  349 | `	}` |
|        11 |  350 | `	return SXRET_OK;` |
|         6 |  351 | `}` |
|     31632 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  353 | `{` |
|     31637 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  355 | `	SyHashEntry_Pr *pEntry;` |
|         - |  356 | `	SyHashEntry_Pr **apNew;` |
|         - |  357 | `	sxu32 n,iBucket;` |
|         - |  358 |  |
|         - |  359 | `	/* Allocate a new larger table */` |
|     31637 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     31637 |  361 | `	if( apNew == 0 ){` |
|         - |  362 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  363 | `		return SXRET_OK;` |
|         - |  364 | `	}` |
|         - |  365 | `	/* Zero the new table */` |
|     31637 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  367 | `	/* Rehash all entries */` |
|   3987989 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3956357 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  370 | `		/* Install in the new bucket */` |
|   3956357 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3956357 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3956357 |  373 | `		if( apNew[iBucket] != 0 ){` |
|   1898896 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    949445 |  375 | `		}` |
|   3956357 |  376 | `		apNew[iBucket] = pEntry;` |
|         - |  377 | `		/* Point to the next entry */` |
|   3956357 |  378 | `		pEntry = pEntry->pNext;` |
|   1978181 |  379 | `	}` |
|         - |  380 | `	/* Release the old table and reflect the change */` |
|     31637 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     31637 |  382 | `	pHash->apBucket = apNew;` |
|     31637 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     31637 |  384 | `	return SXRET_OK;` |
|     15821 |  385 | `}` |
|   5313980 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 | `{` |
|   5313985 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   5313985 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   5313985 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2984637 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1492377 |  393 | `	}` |
|   5313985 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   5313985 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        51 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        51 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        51 |  401 | `		pHash->pLast = pEntry;` |
|        26 |  402 | `	}else{` |
|   5313935 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   5313985 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    332655 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    332655 |  408 | `		pHash->pLast = pEntry;` |
|    166325 |  409 | `	}` |
|   5313985 |  410 | `	pHash->nEntry++;` |
|   5313985 |  411 | `	return SXRET_OK;` |
|         5 |  412 | `}` |
|   5313980 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 | `{` |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   5313985 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     31637 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     31637 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     15816 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   5313985 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   5313985 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   5313985 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   5313985 |  435 | `	pEntry->pHash = pHash;` |
|   5313985 |  436 | `	pEntry->pKey = pKey;` |
|   5313985 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   5313985 |  438 | `	pEntry->pUserData = pUserData;` |
|   5313985 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   5313985 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   5313985 |  442 | `	return rc;` |
|   2656995 |  443 | `}` |
|   5313864 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 | `{` |
|   5313869 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|    172004 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 | `{` |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    172009 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 | `}` |
|         - |  468 |  |
