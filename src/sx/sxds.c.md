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
|  12214968 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  12214970 |   16 | `	pSet->nSize = 0 ;` |
|  12214970 |   17 | `	pSet->nUsed = 0;` |
|  12214970 |   18 | `	pSet->nCursor = 0;` |
|  12214970 |   19 | `	pSet->eSize = ElemSize;` |
|  12214970 |   20 | `	pSet->pAllocator = pAllocator;` |
|  12214970 |   21 | `	pSet->pBase =  0;` |
|  12214970 |   22 | `	pSet->pUserData = 0;` |
|  12214970 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  20033234 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  20033236 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3630084 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3630084 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3535680 |   34 | `			pSet->nSize = 4;` |
|   1767839 |   35 | `		}` |
|   3630084 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3630084 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3630084 |   40 | `		pSet->pBase = pNew;` |
|   3630084 |   41 | `		pSet->nSize <<= 1;` |
|   1815041 |   42 | `	}` |
|  20033236 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 149004036 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  20033236 |   45 | `	pSet->nUsed++;` |
|  20033236 |   46 | `	return SXRET_OK;` |
|  10016641 |   47 |  |
|    643774 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    643776 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    643776 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    643776 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    643776 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    643776 |   60 | `	pSet->nSize = nItem;` |
|    643776 |   61 | `	return SXRET_OK;` |
|    321889 |   62 |  |
|   1104940 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1104942 |   65 | `	pSet->nUsed   = 0;` |
|   1104942 |   66 | `	pSet->nCursor = 0;` |
|   1104942 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     40678 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     40680 |   71 | `	pSet->nCursor = 0;` |
|     40680 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     44560 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     44562 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     16598 |   79 | `		pSet->nCursor = 0;` |
|     16598 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     27966 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     27966 |   83 | `	if( ppEntry ){` |
|     27966 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     13982 |   85 | `	}` |
|     27966 |   86 | `	pSet->nCursor++;` |
|     27966 |   87 | `	return SXRET_OK;` |
|     22282 |   88 |  |
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
|     79046 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     79048 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     79048 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   7618064 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   7618066 |  109 | `	sxi32 rc = SXRET_OK;` |
|   7618066 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3945688 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1972843 |  112 | `	}` |
|   7618066 |  113 | `	pSet->pBase = 0;` |
|   7618066 |  114 | `	pSet->nUsed = 0;` |
|   7618066 |  115 | `	pSet->nCursor = 0;` |
|   7618066 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4041654 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4041656 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4041566 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4041566 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2020829 |  126 |  |
|   3165870 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3165872 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2137292 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1028582 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1028582 |  135 | `	pSet->nUsed--;` |
|   1028582 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1028582 |  137 | `	return pData;` |
|   1582937 |  138 |  |
|   9669076 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   9669078 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   9669078 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   9669078 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4834770 |  148 |  |
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
|    136454 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    136456 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    136456 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    136456 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    136456 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    136456 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    136456 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    136456 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    136456 |  180 | `	pHash->nEntry = 0;` |
|    136456 |  181 | `	pHash->apBucket = apNew;` |
|    136456 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    136456 |  183 | `	return SXRET_OK;` |
|     68229 |  184 |  |
|     26960 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     26962 |  193 | `	pEntry = pHash->pList;` |
|     15150 |  194 | `	for(;;){` |
|     30302 |  195 | `		if( pHash->nEntry == 0 ){` |
|     26962 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3342 |  198 | `		pNext = pEntry->pNext;` |
|      3342 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3342 |  200 | `		pEntry = pNext;` |
|      3342 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     26962 |  203 | `	if( pHash->apBucket ){` |
|     26962 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     13480 |  205 | `	}` |
|     26962 |  206 | `	pHash->apBucket = 0;` |
|     26962 |  207 | `	pHash->nBucketSize = 0;` |
|     26962 |  208 | `	pHash->pAllocator = 0;` |
|     26962 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   9996930 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   9996932 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   9996932 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   8844797 |  218 | `	for(;;){` |
|  17637807 |  219 | `		if( pEntry == 0 ){` |
|   5478002 |  220 | `			break;` |
|         - |  221 | `		}` |
|  14419142 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   4518934 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   4518932 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   7640877 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   5478002 |  229 | `	return 0;` |
|   4998731 |  230 |  |
|  10598330 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  10598332 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    601408 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   9996926 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   9996926 |  244 | `	if( pEntry == 0 ){` |
|   5478002 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   4518926 |  247 | `	return (SyHashEntry *)pEntry;` |
|   5299453 |  248 |  |
|     78084 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     78086 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     59080 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     29541 |  254 | `	}else{` |
|     19008 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     78086 |  257 | `	if( pEntry->pNextCollide ){` |
|      4133 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2066 |  259 | `	}` |
|     78086 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     78086 |  261 | `	pHash->nEntry--;` |
|     78086 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     78086 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     78086 |  268 | `	return rc;` |
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
|     78078 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     78080 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     78080 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     78080 |  296 | `	return rc;` |
|         2 |  297 |  |
|    166344 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    166346 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    166346 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   1190760 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   1190762 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    165912 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    165912 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   1024852 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   1024852 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   1024852 |  324 | `	return (SyHashEntry *)pEntry;` |
|    595382 |  325 |  |
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
|      1619 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1609 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1609 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1609 |  344 | `		pEntry = pEntry->pNext;` |
|       805 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     16850 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     16852 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     16852 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     16852 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     16852 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2321812 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2304962 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2304962 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2304962 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2304962 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1106743 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    553368 |  371 | `		}` |
|   2304962 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2304962 |  374 | `		pEntry = pEntry->pNext;` |
|   1152482 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     16852 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     16852 |  378 | `	pHash->apBucket = apNew;` |
|     16852 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     16852 |  380 | `	return SXRET_OK;` |
|      8427 |  381 |  |
|   2104506 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2104508 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2104508 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2104508 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1405453 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    702724 |  389 | `	}` |
|   2104508 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2104508 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2104508 |  393 | `	if( pHash->nEntry == 0 ){` |
|     84054 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     42026 |  395 | `	}` |
|   2104508 |  396 | `	pHash->nEntry++;` |
|   2104508 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2104506 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2104508 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     16852 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     16852 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      8425 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2104508 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2104508 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2104508 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2104508 |  421 | `	pEntry->pHash = pHash;` |
|   2104508 |  422 | `	pEntry->pKey = pKey;` |
|   2104508 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2104508 |  424 | `	pEntry->pUserData = pUserData;` |
|   2104508 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2104508 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2104508 |  428 | `	return rc;` |
|   1052255 |  429 |  |
|     99534 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     99536 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
