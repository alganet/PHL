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
|   9555494 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|   9555496 |   16 | `	pSet->nSize = 0 ;` |
|   9555496 |   17 | `	pSet->nUsed = 0;` |
|   9555496 |   18 | `	pSet->nCursor = 0;` |
|   9555496 |   19 | `	pSet->eSize = ElemSize;` |
|   9555496 |   20 | `	pSet->pAllocator = pAllocator;` |
|   9555496 |   21 | `	pSet->pBase =  0;` |
|   9555496 |   22 | `	pSet->pUserData = 0;` |
|   9555496 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  15059736 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  15059738 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3165796 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3165796 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3109966 |   34 | `			pSet->nSize = 4;` |
|   1554982 |   35 | `		}` |
|   3165796 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3165796 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3165796 |   40 | `		pSet->pBase = pNew;` |
|   3165796 |   41 | `		pSet->nSize <<= 1;` |
|   1582897 |   42 | `	}` |
|  15059738 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 114260698 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  15059738 |   45 | `	pSet->nUsed++;` |
|  15059738 |   46 | `	return SXRET_OK;` |
|   7529892 |   47 |  |
|    400362 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    400364 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    400364 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    400364 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    400364 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    400364 |   60 | `	pSet->nSize = nItem;` |
|    400364 |   61 | `	return SXRET_OK;` |
|    200183 |   62 |  |
|    800094 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|    800096 |   65 | `	pSet->nUsed   = 0;` |
|    800096 |   66 | `	pSet->nCursor = 0;` |
|    800096 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     32910 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     32912 |   71 | `	pSet->nCursor = 0;` |
|     32912 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     35994 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     35996 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     13142 |   79 | `		pSet->nCursor = 0;` |
|     13142 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     22856 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     22856 |   83 | `	if( ppEntry ){` |
|     22856 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     11427 |   85 | `	}` |
|     22856 |   86 | `	pSet->nCursor++;` |
|     22856 |   87 | `	return SXRET_OK;` |
|     17999 |   88 |  |
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
|     50452 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     50454 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     50454 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   6551628 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   6551630 |  109 | `	sxi32 rc = SXRET_OK;` |
|   6551630 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3361976 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1680987 |  112 | `	}` |
|   6551630 |  113 | `	pSet->pBase = 0;` |
|   6551630 |  114 | `	pSet->nUsed = 0;` |
|   6551630 |  115 | `	pSet->nCursor = 0;` |
|   6551630 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3199032 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3199034 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3198944 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3198944 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1599518 |  126 |  |
|   2876862 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   2876864 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2118962 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    757904 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    757904 |  135 | `	pSet->nUsed--;` |
|    757904 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    757904 |  137 | `	return pData;` |
|   1438433 |  138 |  |
|   8030887 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   8030889 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   8030889 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   8030889 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4015650 |  148 |  |
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
|     71868 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     71870 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     71870 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     71870 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     71870 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     71870 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     71870 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     71870 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     71870 |  180 | `	pHash->nEntry = 0;` |
|     71870 |  181 | `	pHash->apBucket = apNew;` |
|     71870 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     71870 |  183 | `	return SXRET_OK;` |
|     35936 |  184 |  |
|      9254 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|      9256 |  193 | `	pEntry = pHash->pList;` |
|      5251 |  194 | `	for(;;){` |
|     10504 |  195 | `		if( pHash->nEntry == 0 ){` |
|      9256 |  196 | `			break;` |
|         - |  197 | `		}` |
|      1250 |  198 | `		pNext = pEntry->pNext;` |
|      1250 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      1250 |  200 | `		pEntry = pNext;` |
|      1250 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|      9256 |  203 | `	if( pHash->apBucket ){` |
|      9256 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      4627 |  205 | `	}` |
|      9256 |  206 | `	pHash->apBucket = 0;` |
|      9256 |  207 | `	pHash->nBucketSize = 0;` |
|      9256 |  208 | `	pHash->pAllocator = 0;` |
|      9256 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   7306352 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   7306354 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   7306354 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   6318846 |  218 | `	for(;;){` |
|  12692933 |  219 | `		if( pEntry == 0 ){` |
|   3950056 |  220 | `			break;` |
|         - |  221 | `		}` |
|  10420898 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   3356302 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   3356300 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   5386581 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   3950056 |  229 | `	return 0;` |
|   3653442 |  230 |  |
|   7347086 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   7347088 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     40742 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   7306348 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   7306348 |  244 | `	if( pEntry == 0 ){` |
|   3950056 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   3356294 |  247 | `	return (SyHashEntry *)pEntry;` |
|   3673809 |  248 |  |
|     59756 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     59758 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     44700 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     22351 |  254 | `	}else{` |
|     15060 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     59758 |  257 | `	if( pEntry->pNextCollide ){` |
|      3591 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      1795 |  259 | `	}` |
|     59758 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     59758 |  261 | `	pHash->nEntry--;` |
|     59758 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     59758 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     59758 |  268 | `	return rc;` |
|         2 |  269 |  |
|         6 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         1 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|         7 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|         7 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|         7 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|         7 |  284 | `	return rc;` |
|         4 |  285 |  |
|     59750 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     59752 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     59752 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     59752 |  296 | `	return rc;` |
|         2 |  297 |  |
|    106220 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    106222 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    106222 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    741780 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    741782 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    105788 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    105788 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    635996 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    635996 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    635996 |  324 | `	return (SyHashEntry *)pEntry;` |
|    370892 |  325 |  |
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
|      1579 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1569 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1569 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1569 |  344 | `		pEntry = pEntry->pNext;` |
|       785 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|      9964 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|      9966 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|      9966 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      9966 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|      9966 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1362318 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1352354 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1352354 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1352354 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1352354 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    649427 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    324712 |  371 | `		}` |
|   1352354 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1352354 |  374 | `		pEntry = pEntry->pNext;` |
|    676178 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|      9966 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      9966 |  378 | `	pHash->apBucket = apNew;` |
|      9966 |  379 | `	pHash->nBucketSize = nNewSize;` |
|      9966 |  380 | `	return SXRET_OK;` |
|      4984 |  381 |  |
|   1236598 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1236600 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1236600 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1236600 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    821614 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    410798 |  389 | `	}` |
|   1236600 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1236600 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1236600 |  393 | `	if( pHash->nEntry == 0 ){` |
|     51478 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     25738 |  395 | `	}` |
|   1236600 |  396 | `	pHash->nEntry++;` |
|   1236600 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1236598 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1236600 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      9966 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|      9966 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      4982 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1236600 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1236600 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1236600 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1236600 |  421 | `	pEntry->pHash = pHash;` |
|   1236600 |  422 | `	pEntry->pKey = pKey;` |
|   1236600 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1236600 |  424 | `	pEntry->pUserData = pUserData;` |
|   1236600 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1236600 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1236600 |  428 | `	return rc;` |
|    618301 |  429 |  |
|     72316 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     72318 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
