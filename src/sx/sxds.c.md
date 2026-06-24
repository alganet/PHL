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
|  18438326 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 |  |
|  18438331 |   16 | `	pSet->nSize = 0 ;` |
|  18438331 |   17 | `	pSet->nUsed = 0;` |
|  18438331 |   18 | `	pSet->nCursor = 0;` |
|  18438331 |   19 | `	pSet->eSize = ElemSize;` |
|  18438331 |   20 | `	pSet->pAllocator = pAllocator;` |
|  18438331 |   21 | `	pSet->pBase =  0;` |
|  18438331 |   22 | `	pSet->pUserData = 0;` |
|  18438331 |   23 | `	return SXRET_OK;` |
|         5 |   24 |  |
|  30208571 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  30208576 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4467593 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4467593 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4319141 |   34 | `			pSet->nSize = 4;` |
|   2159568 |   35 | `		}` |
|   4467593 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4467593 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4467593 |   40 | `		pSet->pBase = pNew;` |
|   4467593 |   41 | `		pSet->nSize <<= 1;` |
|   2233794 |   42 | `	}` |
|  30208576 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 225814436 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  30208576 |   45 | `	pSet->nUsed++;` |
|  30208576 |   46 | `	return SXRET_OK;` |
|  15104334 |   47 |  |
|   1224842 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 |  |
|   1224847 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1224847 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1224847 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1224847 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1224847 |   60 | `	pSet->nSize = nItem;` |
|   1224847 |   61 | `	return SXRET_OK;` |
|    612426 |   62 |  |
|   1724845 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 |  |
|   1724850 |   65 | `	pSet->nUsed   = 0;` |
|   1724850 |   66 | `	pSet->nCursor = 0;` |
|   1724850 |   67 | `	return SXRET_OK;` |
|         5 |   68 |  |
|     56472 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 |  |
|     56477 |   71 | `	pSet->nCursor = 0;` |
|     56477 |   72 | `	return SXRET_OK;` |
|         5 |   73 |  |
|     60678 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     60683 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     23325 |   79 | `		pSet->nCursor = 0;` |
|     23325 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     37363 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     37363 |   83 | `	if( ppEntry ){` |
|     37363 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     18679 |   85 | `	}` |
|     37363 |   86 | `	pSet->nCursor++;` |
|     37363 |   87 | `	return SXRET_OK;` |
|     30344 |   88 |  |
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
|    207048 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 |  |
|    207053 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       118 |  103 | `		pSet->nUsed = nNewSize;` |
|        57 |  104 | `	}` |
|    207053 |  105 | `	return SXRET_OK;` |
|         5 |  106 |  |
|   9743176 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 |  |
|   9743181 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9743181 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4878061 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2439028 |  112 | `	}` |
|   9743181 |  113 | `	pSet->pBase = 0;` |
|   9743181 |  114 | `	pSet->nUsed = 0;` |
|   9743181 |  115 | `	pSet->nCursor = 0;` |
|   9743181 |  116 | `	return rc;` |
|         5 |  117 |  |
|   5530862 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5530867 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       131 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5530741 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5530741 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2765436 |  126 |  |
|   3548800 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3548805 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2175435 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1373375 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1373375 |  135 | `	pSet->nUsed--;` |
|   1373375 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1373375 |  137 | `	return pData;` |
|   1774405 |  138 |  |
|  13120372 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  13120377 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13120377 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13120377 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6560537 |  148 |  |
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
|    532532 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    532537 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    532537 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    532537 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    532537 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    532537 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    532537 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    532537 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    532537 |  180 | `	pHash->nEntry = 0;` |
|    532537 |  181 | `	pHash->apBucket = apNew;` |
|    532537 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    532537 |  183 | `	return SXRET_OK;` |
|    266271 |  184 |  |
|     98440 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     98445 |  193 | `	pEntry = pHash->pList;` |
|     52600 |  194 | `	for(;;){` |
|    105205 |  195 | `		if( pHash->nEntry == 0 ){` |
|     98445 |  196 | `			break;` |
|         - |  197 | `		}` |
|      6765 |  198 | `		pNext = pEntry->pNext;` |
|      6765 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      6765 |  200 | `		pEntry = pNext;` |
|      6765 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|     98445 |  203 | `	if( pHash->apBucket ){` |
|     98445 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     49220 |  205 | `	}` |
|     98445 |  206 | `	pHash->apBucket = 0;` |
|     98445 |  207 | `	pHash->nBucketSize = 0;` |
|     98445 |  208 | `	pHash->pAllocator = 0;` |
|     98445 |  209 | `	return SXRET_OK;` |
|         5 |  210 |  |
|  16770144 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  16770149 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  16770149 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  14973870 |  218 | `	for(;;){` |
|  30028197 |  219 | `		if( pEntry == 0 ){` |
|   8883485 |  220 | `			break;` |
|         - |  221 | `		}` |
|  25087790 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   7886668 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   7886669 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  13258053 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   8883485 |  229 | `	return 0;` |
|   8385599 |  230 |  |
|  17554678 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  17554683 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    784735 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  16769953 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  16769953 |  244 | `	if( pEntry == 0 ){` |
|   8883485 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   7886473 |  247 | `	return (SyHashEntry *)pEntry;` |
|   8777866 |  248 |  |
|    118780 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    118785 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     91193 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     45599 |  254 | `	}else{` |
|     27597 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    118785 |  257 | `	if( pEntry->pNextCollide ){` |
|      5041 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2520 |  259 | `	}` |
|    118785 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    118785 |  261 | `	pHash->nEntry--;` |
|    118785 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    118785 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    118785 |  268 | `	return rc;` |
|         5 |  269 |  |
|       196 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       201 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       201 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       201 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       201 |  284 | `	return rc;` |
|       103 |  285 |  |
|    118584 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  287 |  |
|    118589 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    118589 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    118589 |  296 | `	return rc;` |
|         5 |  297 |  |
|   1084262 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|   1084267 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   1084267 |  306 | `	return SXRET_OK;` |
|         5 |  307 |  |
|   6857490 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   6857495 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1083817 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   1083817 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   5773683 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   5773683 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   5773683 |  324 | `	return (SyHashEntry *)pEntry;` |
|   3428750 |  325 |  |
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
|      1991 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1981 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1981 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1981 |  344 | `		pEntry = pEntry->pNext;` |
|       991 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     27524 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  349 |  |
|     27529 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     27529 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     27529 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     27529 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3495913 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3468389 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3468389 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3468389 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3468389 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1663849 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    831977 |  371 | `		}` |
|   3468389 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3468389 |  374 | `		pEntry = pEntry->pNext;` |
|   1734197 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     27529 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     27529 |  378 | `	pHash->apBucket = apNew;` |
|     27529 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     27529 |  380 | `	return SXRET_OK;` |
|     13767 |  381 |  |
|   4619886 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         5 |  383 |  |
|   4619891 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   4619891 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4619891 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2605288 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1302635 |  389 | `	}` |
|   4619891 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   4619891 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   4619891 |  393 | `	if( pHash->nEntry == 0 ){` |
|    290531 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    145263 |  395 | `	}` |
|   4619891 |  396 | `	pHash->nEntry++;` |
|   4619891 |  397 | `	return SXRET_OK;` |
|         5 |  398 |  |
|   4619886 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   4619891 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     27529 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     27529 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     13762 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   4619891 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4619891 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   4619891 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4619891 |  421 | `	pEntry->pHash = pHash;` |
|   4619891 |  422 | `	pEntry->pKey = pKey;` |
|   4619891 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   4619891 |  424 | `	pEntry->pUserData = pUserData;` |
|   4619891 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4619891 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   4619891 |  428 | `	return rc;` |
|   2309948 |  429 |  |
|    152646 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    152651 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  439 |  |
|         - |  440 |  |
