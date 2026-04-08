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
|  13897820 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  13897822 |   16 | `	pSet->nSize = 0 ;` |
|  13897822 |   17 | `	pSet->nUsed = 0;` |
|  13897822 |   18 | `	pSet->nCursor = 0;` |
|  13897822 |   19 | `	pSet->eSize = ElemSize;` |
|  13897822 |   20 | `	pSet->pAllocator = pAllocator;` |
|  13897822 |   21 | `	pSet->pBase =  0;` |
|  13897822 |   22 | `	pSet->pUserData = 0;` |
|  13897822 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  22961562 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  22961564 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3843532 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3843532 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3738966 |   34 | `			pSet->nSize = 4;` |
|   1869482 |   35 | `		}` |
|   3843532 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3843532 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3843532 |   40 | `		pSet->pBase = pNew;` |
|   3843532 |   41 | `		pSet->nSize <<= 1;` |
|   1921765 |   42 | `	}` |
|  22961564 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 170722956 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  22961564 |   45 | `	pSet->nUsed++;` |
|  22961564 |   46 | `	return SXRET_OK;` |
|  11480805 |   47 |  |
|    824326 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    824328 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    824328 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    824328 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    824328 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    824328 |   60 | `	pSet->nSize = nItem;` |
|    824328 |   61 | `	return SXRET_OK;` |
|    412165 |   62 |  |
|   1268684 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1268686 |   65 | `	pSet->nUsed   = 0;` |
|   1268686 |   66 | `	pSet->nCursor = 0;` |
|   1268686 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     43814 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     43816 |   71 | `	pSet->nCursor = 0;` |
|     43816 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     47868 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     47870 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     17946 |   79 | `		pSet->nCursor = 0;` |
|     17946 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     29926 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     29926 |   83 | `	if( ppEntry ){` |
|     29926 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     14962 |   85 | `	}` |
|     29926 |   86 | `	pSet->nCursor++;` |
|     29926 |   87 | `	return SXRET_OK;` |
|     23936 |   88 |  |
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
|    136940 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    136942 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|    136942 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8281486 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8281488 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8281488 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4229856 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2114927 |  112 | `	}` |
|   8281488 |  113 | `	pSet->pBase = 0;` |
|   8281488 |  114 | `	pSet->nUsed = 0;` |
|   8281488 |  115 | `	pSet->nCursor = 0;` |
|   8281488 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4480936 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4480938 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4480848 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4480848 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2240470 |  126 |  |
|   3255558 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3255560 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2141824 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1113738 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1113738 |  135 | `	pSet->nUsed--;` |
|   1113738 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1113738 |  137 | `	return pData;` |
|   1627781 |  138 |  |
|  10360664 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  10360666 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  10360666 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  10360666 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5180538 |  148 |  |
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
|    177770 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    177772 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    177772 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    177772 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    177772 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    177772 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    177772 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    177772 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    177772 |  180 | `	pHash->nEntry = 0;` |
|    177772 |  181 | `	pHash->apBucket = apNew;` |
|    177772 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    177772 |  183 | `	return SXRET_OK;` |
|     88887 |  184 |  |
|     29748 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     29750 |  193 | `	pEntry = pHash->pList;` |
|     16617 |  194 | `	for(;;){` |
|     33236 |  195 | `		if( pHash->nEntry == 0 ){` |
|     29750 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3488 |  198 | `		pNext = pEntry->pNext;` |
|      3488 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3488 |  200 | `		pEntry = pNext;` |
|      3488 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     29750 |  203 | `	if( pHash->apBucket ){` |
|     29750 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     14874 |  205 | `	}` |
|     29750 |  206 | `	pHash->apBucket = 0;` |
|     29750 |  207 | `	pHash->nBucketSize = 0;` |
|     29750 |  208 | `	pHash->pAllocator = 0;` |
|     29750 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  11274012 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  11274014 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  11274014 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  10110988 |  218 | `	for(;;){` |
|  20235758 |  219 | `		if( pEntry == 0 ){` |
|   6233944 |  220 | `			break;` |
|         - |  221 | `		}` |
|  16521721 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5040074 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5040072 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   8961746 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   6233944 |  229 | `	return 0;` |
|   5637272 |  230 |  |
|  11379426 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  11379428 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    105438 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  11273992 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  11273992 |  244 | `	if( pEntry == 0 ){` |
|   6233944 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5040050 |  247 | `	return (SyHashEntry *)pEntry;` |
|   5689979 |  248 |  |
|     84910 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     84912 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     64512 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     32257 |  254 | `	}else{` |
|     20402 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     84912 |  257 | `	if( pEntry->pNextCollide ){` |
|      4469 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2234 |  259 | `	}` |
|     84912 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     84912 |  261 | `	pHash->nEntry--;` |
|     84912 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     84912 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     84912 |  268 | `	return rc;` |
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
|     84888 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     84890 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     84890 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     84890 |  296 | `	return rc;` |
|         2 |  297 |  |
|    256626 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    256628 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    256628 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   1935862 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   1935864 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    256194 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    256194 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   1679672 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   1679672 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   1679672 |  324 | `	return (SyHashEntry *)pEntry;` |
|    967933 |  325 |  |
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
|     21420 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     21422 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     21422 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     21422 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     21422 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2717198 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2695778 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2695778 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2695778 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2695778 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1288232 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    644168 |  371 | `		}` |
|   2695778 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2695778 |  374 | `		pEntry = pEntry->pNext;` |
|   1347890 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     21422 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     21422 |  378 | `	pHash->apBucket = apNew;` |
|     21422 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     21422 |  380 | `	return SXRET_OK;` |
|     10712 |  381 |  |
|   2672942 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2672944 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2672944 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2672944 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1778719 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    889333 |  389 | `	}` |
|   2672944 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2672944 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2672944 |  393 | `	if( pHash->nEntry == 0 ){` |
|    110202 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     55100 |  395 | `	}` |
|   2672944 |  396 | `	pHash->nEntry++;` |
|   2672944 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2672942 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2672944 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     21422 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     21422 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     10710 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2672944 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2672944 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2672944 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2672944 |  421 | `	pEntry->pHash = pHash;` |
|   2672944 |  422 | `	pEntry->pKey = pKey;` |
|   2672944 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2672944 |  424 | `	pEntry->pUserData = pUserData;` |
|   2672944 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2672944 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2672944 |  428 | `	return rc;` |
|   1336473 |  429 |  |
|    108956 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    108958 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
