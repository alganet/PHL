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
|   9615454 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|   9615456 |   16 | `	pSet->nSize = 0 ;` |
|   9615456 |   17 | `	pSet->nUsed = 0;` |
|   9615456 |   18 | `	pSet->nCursor = 0;` |
|   9615456 |   19 | `	pSet->eSize = ElemSize;` |
|   9615456 |   20 | `	pSet->pAllocator = pAllocator;` |
|   9615456 |   21 | `	pSet->pBase =  0;` |
|   9615456 |   22 | `	pSet->pUserData = 0;` |
|   9615456 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  15184636 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  15184638 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3169816 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3169816 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3112950 |   34 | `			pSet->nSize = 4;` |
|   1556474 |   35 | `		}` |
|   3169816 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3169816 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3169816 |   40 | `		pSet->pBase = pNew;` |
|   3169816 |   41 | `		pSet->nSize <<= 1;` |
|   1584907 |   42 | `	}` |
|  15184638 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 115223342 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  15184638 |   45 | `	pSet->nUsed++;` |
|  15184638 |   46 | `	return SXRET_OK;` |
|   7592342 |   47 |  |
|    408024 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    408026 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    408026 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    408026 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    408026 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    408026 |   60 | `	pSet->nSize = nItem;` |
|    408026 |   61 | `	return SXRET_OK;` |
|    204014 |   62 |  |
|    806622 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|    806624 |   65 | `	pSet->nUsed   = 0;` |
|    806624 |   66 | `	pSet->nCursor = 0;` |
|    806624 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     32880 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     32882 |   71 | `	pSet->nCursor = 0;` |
|     32882 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     35950 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     35952 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     13134 |   79 | `		pSet->nCursor = 0;` |
|     13134 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     22820 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     22820 |   83 | `	if( ppEntry ){` |
|     22820 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     11409 |   85 | `	}` |
|     22820 |   86 | `	pSet->nCursor++;` |
|     22820 |   87 | `	return SXRET_OK;` |
|     17977 |   88 |  |
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
|     51540 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     51542 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     51542 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   6565314 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   6565316 |  109 | `	sxi32 rc = SXRET_OK;` |
|   6565316 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3369370 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1684684 |  112 | `	}` |
|   6565316 |  113 | `	pSet->pBase = 0;` |
|   6565316 |  114 | `	pSet->nUsed = 0;` |
|   6565316 |  115 | `	pSet->nCursor = 0;` |
|   6565316 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3220684 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3220686 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3220596 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3220596 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1610344 |  126 |  |
|   2875898 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   2875900 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2119498 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    756404 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    756404 |  135 | `	pSet->nUsed--;` |
|    756404 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    756404 |  137 | `	return pData;` |
|   1437951 |  138 |  |
|   8016619 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   8016621 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   8016621 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   8016621 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4008508 |  148 |  |
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
|     73256 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     73258 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     73258 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     73258 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     73258 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     73258 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     73258 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     73258 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     73258 |  180 | `	pHash->nEntry = 0;` |
|     73258 |  181 | `	pHash->apBucket = apNew;` |
|     73258 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     73258 |  183 | `	return SXRET_OK;` |
|     36630 |  184 |  |
|      9298 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|      9300 |  193 | `	pEntry = pHash->pList;` |
|      5303 |  194 | `	for(;;){` |
|     10608 |  195 | `		if( pHash->nEntry == 0 ){` |
|      9300 |  196 | `			break;` |
|         - |  197 | `		}` |
|      1310 |  198 | `		pNext = pEntry->pNext;` |
|      1310 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      1310 |  200 | `		pEntry = pNext;` |
|      1310 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|      9300 |  203 | `	if( pHash->apBucket ){` |
|      9300 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      4649 |  205 | `	}` |
|      9300 |  206 | `	pHash->apBucket = 0;` |
|      9300 |  207 | `	pHash->nBucketSize = 0;` |
|      9300 |  208 | `	pHash->pAllocator = 0;` |
|      9300 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   7336640 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   7336642 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   7336642 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   6390164 |  218 | `	for(;;){` |
|  12830768 |  219 | `		if( pEntry == 0 ){` |
|   3972744 |  220 | `			break;` |
|         - |  221 | `		}` |
|  10539845 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   3363902 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   3363900 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   5494128 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   3972744 |  229 | `	return 0;` |
|   3668586 |  230 |  |
|   7378082 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   7378084 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     41450 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   7336636 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   7336636 |  244 | `	if( pEntry == 0 ){` |
|   3972744 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   3363894 |  247 | `	return (SyHashEntry *)pEntry;` |
|   3689307 |  248 |  |
|     59886 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     59888 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     44840 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     22421 |  254 | `	}else{` |
|     15050 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     59888 |  257 | `	if( pEntry->pNextCollide ){` |
|      3587 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      1793 |  259 | `	}` |
|     59888 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     59888 |  261 | `	pHash->nEntry--;` |
|     59888 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     59888 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     59888 |  268 | `	return rc;` |
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
|     59880 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     59882 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     59882 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     59882 |  296 | `	return rc;` |
|         2 |  297 |  |
|    107784 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    107786 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    107786 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    753120 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    753122 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    107352 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    107352 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    645772 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    645772 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    645772 |  324 | `	return (SyHashEntry *)pEntry;` |
|    376562 |  325 |  |
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
|     10220 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     10222 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     10222 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     10222 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     10222 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1397902 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1387682 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1387682 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1387682 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1387682 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    666405 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    333207 |  371 | `		}` |
|   1387682 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1387682 |  374 | `		pEntry = pEntry->pNext;` |
|    693842 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     10222 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     10222 |  378 | `	pHash->apBucket = apNew;` |
|     10222 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     10222 |  380 | `	return SXRET_OK;` |
|      5112 |  381 |  |
|   1265964 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1265966 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1265966 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1265966 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    842053 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    421071 |  389 | `	}` |
|   1265966 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1265966 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1265966 |  393 | `	if( pHash->nEntry == 0 ){` |
|     52452 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     26225 |  395 | `	}` |
|   1265966 |  396 | `	pHash->nEntry++;` |
|   1265966 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1265964 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1265966 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     10222 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     10222 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      5110 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1265966 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1265966 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1265966 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1265966 |  421 | `	pEntry->pHash = pHash;` |
|   1265966 |  422 | `	pEntry->pKey = pKey;` |
|   1265966 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1265966 |  424 | `	pEntry->pUserData = pUserData;` |
|   1265966 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1265966 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1265966 |  428 | `	return rc;` |
|    632984 |  429 |  |
|     72768 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     72770 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
