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
|  11439158 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  11439160 |   16 | `	pSet->nSize = 0 ;` |
|  11439160 |   17 | `	pSet->nUsed = 0;` |
|  11439160 |   18 | `	pSet->nCursor = 0;` |
|  11439160 |   19 | `	pSet->eSize = ElemSize;` |
|  11439160 |   20 | `	pSet->pAllocator = pAllocator;` |
|  11439160 |   21 | `	pSet->pBase =  0;` |
|  11439160 |   22 | `	pSet->pUserData = 0;` |
|  11439160 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  18480334 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  18480336 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3518482 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3518482 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3436270 |   34 | `			pSet->nSize = 4;` |
|   1718134 |   35 | `		}` |
|   3518482 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3518482 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3518482 |   40 | `		pSet->pBase = pNew;` |
|   3518482 |   41 | `		pSet->nSize <<= 1;` |
|   1759240 |   42 | `	}` |
|  18480336 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 137624944 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  18480336 |   45 | `	pSet->nUsed++;` |
|  18480336 |   46 | `	return SXRET_OK;` |
|   9240191 |   47 |  |
|    559860 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    559862 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    559862 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    559862 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    559862 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    559862 |   60 | `	pSet->nSize = nItem;` |
|    559862 |   61 | `	return SXRET_OK;` |
|    279932 |   62 |  |
|   1034992 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1034994 |   65 | `	pSet->nUsed   = 0;` |
|   1034994 |   66 | `	pSet->nCursor = 0;` |
|   1034994 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     39162 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     39164 |   71 | `	pSet->nCursor = 0;` |
|     39164 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     43018 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     43020 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     15854 |   79 | `		pSet->nCursor = 0;` |
|     15854 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     27168 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     27168 |   83 | `	if( ppEntry ){` |
|     27168 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     13583 |   85 | `	}` |
|     27168 |   86 | `	pSet->nCursor++;` |
|     27168 |   87 | `	return SXRET_OK;` |
|     21511 |   88 |  |
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
|     67994 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     67996 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     67996 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   7339352 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   7339354 |  109 | `	sxi32 rc = SXRET_OK;` |
|   7339354 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3795830 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1897914 |  112 | `	}` |
|   7339354 |  113 | `	pSet->pBase = 0;` |
|   7339354 |  114 | `	pSet->nUsed = 0;` |
|   7339354 |  115 | `	pSet->nCursor = 0;` |
|   7339354 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3696080 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3696082 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3695992 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3695992 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1848042 |  126 |  |
|   3112358 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3112360 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2131254 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    981108 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    981108 |  135 | `	pSet->nUsed--;` |
|    981108 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    981108 |  137 | `	return pData;` |
|   1556181 |  138 |  |
|   9676894 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   9676896 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   9676896 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   9676896 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4838691 |  148 |  |
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
|     95966 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     95968 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     95968 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     95968 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     95968 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     95968 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     95968 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     95968 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     95968 |  180 | `	pHash->nEntry = 0;` |
|     95968 |  181 | `	pHash->apBucket = apNew;` |
|     95968 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     95968 |  183 | `	return SXRET_OK;` |
|     47985 |  184 |  |
|     11740 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     11742 |  193 | `	pEntry = pHash->pList;` |
|      7282 |  194 | `	for(;;){` |
|     14566 |  195 | `		if( pHash->nEntry == 0 ){` |
|     11742 |  196 | `			break;` |
|         - |  197 | `		}` |
|      2826 |  198 | `		pNext = pEntry->pNext;` |
|      2826 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      2826 |  200 | `		pEntry = pNext;` |
|      2826 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     11742 |  203 | `	if( pHash->apBucket ){` |
|     11742 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      5870 |  205 | `	}` |
|     11742 |  206 | `	pHash->apBucket = 0;` |
|     11742 |  207 | `	pHash->nBucketSize = 0;` |
|     11742 |  208 | `	pHash->pAllocator = 0;` |
|     11742 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   9705186 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   9705188 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   9705188 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   8302667 |  218 | `	for(;;){` |
|  16610076 |  219 | `		if( pEntry == 0 ){` |
|   5262872 |  220 | `			break;` |
|         - |  221 | `		}` |
|  13568234 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   4442320 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   4442318 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   6904890 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   5262872 |  229 | `	return 0;` |
|   4852859 |  230 |  |
|   9759142 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   9759144 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     53964 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   9705182 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   9705182 |  244 | `	if( pEntry == 0 ){` |
|   5262872 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   4442312 |  247 | `	return (SyHashEntry *)pEntry;` |
|   4879837 |  248 |  |
|     73480 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     73482 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     55312 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     27657 |  254 | `	}else{` |
|     18172 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     73482 |  257 | `	if( pEntry->pNextCollide ){` |
|      4125 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2062 |  259 | `	}` |
|     73482 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     73482 |  261 | `	pHash->nEntry--;` |
|     73482 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     73482 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     73482 |  268 | `	return rc;` |
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
|     73474 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     73476 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     73476 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     73476 |  296 | `	return rc;` |
|         2 |  297 |  |
|    136274 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    136276 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    136276 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    946988 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    946990 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    135842 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    135842 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    811150 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    811150 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    811150 |  324 | `	return (SyHashEntry *)pEntry;` |
|    473496 |  325 |  |
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
|      1617 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1607 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1607 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1607 |  344 | `		pEntry = pEntry->pNext;` |
|       804 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     14216 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     14218 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     14218 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     14218 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     14218 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1955146 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1940930 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1940930 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1940930 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1940930 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    931973 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    465984 |  371 | `		}` |
|   1940930 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1940930 |  374 | `		pEntry = pEntry->pNext;` |
|    970466 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     14218 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     14218 |  378 | `	pHash->apBucket = apNew;` |
|     14218 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     14218 |  380 | `	return SXRET_OK;` |
|      7110 |  381 |  |
|   1754930 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1754932 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1754932 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1754932 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1186199 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    593126 |  389 | `	}` |
|   1754932 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1754932 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1754932 |  393 | `	if( pHash->nEntry == 0 ){` |
|     68982 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     34490 |  395 | `	}` |
|   1754932 |  396 | `	pHash->nEntry++;` |
|   1754932 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1754930 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1754932 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     14218 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     14218 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      7108 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1754932 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1754932 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1754932 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1754932 |  421 | `	pEntry->pHash = pHash;` |
|   1754932 |  422 | `	pEntry->pKey = pKey;` |
|   1754932 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1754932 |  424 | `	pEntry->pUserData = pUserData;` |
|   1754932 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1754932 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1754932 |  428 | `	return rc;` |
|    877467 |  429 |  |
|     91450 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     91452 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
