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
|  13943264 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  13943266 |   16 | `	pSet->nSize = 0 ;` |
|  13943266 |   17 | `	pSet->nUsed = 0;` |
|  13943266 |   18 | `	pSet->nCursor = 0;` |
|  13943266 |   19 | `	pSet->eSize = ElemSize;` |
|  13943266 |   20 | `	pSet->pAllocator = pAllocator;` |
|  13943266 |   21 | `	pSet->pBase =  0;` |
|  13943266 |   22 | `	pSet->pUserData = 0;` |
|  13943266 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  23040758 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  23040760 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3850064 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3850064 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3744980 |   34 | `			pSet->nSize = 4;` |
|   1872489 |   35 | `		}` |
|   3850064 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3850064 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3850064 |   40 | `		pSet->pBase = pNew;` |
|   3850064 |   41 | `		pSet->nSize <<= 1;` |
|   1925031 |   42 | `	}` |
|  23040760 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 171268448 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  23040760 |   45 | `	pSet->nUsed++;` |
|  23040760 |   46 | `	return SXRET_OK;` |
|  11520403 |   47 |  |
|    828296 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    828298 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    828298 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    828298 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    828298 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    828298 |   60 | `	pSet->nSize = nItem;` |
|    828298 |   61 | `	return SXRET_OK;` |
|    414150 |   62 |  |
|   1275822 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1275824 |   65 | `	pSet->nUsed   = 0;` |
|   1275824 |   66 | `	pSet->nCursor = 0;` |
|   1275824 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     44180 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     44182 |   71 | `	pSet->nCursor = 0;` |
|     44182 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     48234 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     48236 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     18098 |   79 | `		pSet->nCursor = 0;` |
|     18098 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     30140 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     30140 |   83 | `	if( ppEntry ){` |
|     30140 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     15069 |   85 | `	}` |
|     30140 |   86 | `	pSet->nCursor++;` |
|     30140 |   87 | `	return SXRET_OK;` |
|     24119 |   88 |  |
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
|    137592 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    137594 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|    137594 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8297346 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8297348 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8297348 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4238244 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2119121 |  112 | `	}` |
|   8297348 |  113 | `	pSet->pBase = 0;` |
|   8297348 |  114 | `	pSet->nUsed = 0;` |
|   8297348 |  115 | `	pSet->nCursor = 0;` |
|   8297348 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4492702 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4492704 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4492614 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4492614 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2246353 |  126 |  |
|   3259310 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3259312 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2142012 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1117302 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1117302 |  135 | `	pSet->nUsed--;` |
|   1117302 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1117302 |  137 | `	return pData;` |
|   1629657 |  138 |  |
|  10702132 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  10702134 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  10702134 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  10702134 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5351248 |  148 |  |
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
|    230786 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    230788 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    230788 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    230788 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    230788 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    230788 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    230788 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    230788 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    230788 |  180 | `	pHash->nEntry = 0;` |
|    230788 |  181 | `	pHash->apBucket = apNew;` |
|    230788 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    230788 |  183 | `	return SXRET_OK;` |
|    115395 |  184 |  |
|     74292 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     74294 |  193 | `	pEntry = pHash->pList;` |
|     38907 |  194 | `	for(;;){` |
|     77816 |  195 | `		if( pHash->nEntry == 0 ){` |
|     74294 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3524 |  198 | `		pNext = pEntry->pNext;` |
|      3524 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3524 |  200 | `		pEntry = pNext;` |
|      3524 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     74294 |  203 | `	if( pHash->apBucket ){` |
|     74294 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     37146 |  205 | `	}` |
|     74294 |  206 | `	pHash->apBucket = 0;` |
|     74294 |  207 | `	pHash->nBucketSize = 0;` |
|     74294 |  208 | `	pHash->pAllocator = 0;` |
|     74294 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  11323624 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  11323626 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  11323626 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  10247715 |  218 | `	for(;;){` |
|  20479786 |  219 | `		if( pEntry == 0 ){` |
|   6261972 |  220 | `			break;` |
|         - |  221 | `		}` |
|  16748513 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5061658 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5061656 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   9156162 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   6261972 |  229 | `	return 0;` |
|   5662078 |  230 |  |
|  11754168 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  11754170 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    430568 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  11323604 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  11323604 |  244 | `	if( pEntry == 0 ){` |
|   6261972 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5061634 |  247 | `	return (SyHashEntry *)pEntry;` |
|   5877350 |  248 |  |
|     85342 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     85344 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     64792 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     32397 |  254 | `	}else{` |
|     20554 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     85344 |  257 | `	if( pEntry->pNextCollide ){` |
|      4479 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2239 |  259 | `	}` |
|     85344 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     85344 |  261 | `	pHash->nEntry--;` |
|     85344 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     85344 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     85344 |  268 | `	return rc;` |
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
|     85320 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     85322 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     85322 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     85322 |  296 | `	return rc;` |
|         2 |  297 |  |
|    261538 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    261540 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    261540 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   1963156 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   1963158 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    261106 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    261106 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   1702054 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   1702054 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   1702054 |  324 | `	return (SyHashEntry *)pEntry;` |
|    981580 |  325 |  |
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
|      1769 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1759 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1759 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1759 |  344 | `		pEntry = pEntry->pNext;` |
|       880 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     21532 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     21534 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     21534 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     21534 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     21534 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2734398 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2712866 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2712866 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2712866 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2712866 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1296461 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    648119 |  371 | `		}` |
|   2712866 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2712866 |  374 | `		pEntry = pEntry->pNext;` |
|   1356434 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     21534 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     21534 |  378 | `	pHash->apBucket = apNew;` |
|     21534 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     21534 |  380 | `	return SXRET_OK;` |
|     10768 |  381 |  |
|   2686412 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2686414 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2686414 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2686414 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1787890 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    893883 |  389 | `	}` |
|   2686414 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2686414 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2686414 |  393 | `	if( pHash->nEntry == 0 ){` |
|    110754 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     55376 |  395 | `	}` |
|   2686414 |  396 | `	pHash->nEntry++;` |
|   2686414 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2686412 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2686414 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     21534 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     21534 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     10766 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2686414 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2686414 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2686414 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2686414 |  421 | `	pEntry->pHash = pHash;` |
|   2686414 |  422 | `	pEntry->pKey = pKey;` |
|   2686414 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2686414 |  424 | `	pEntry->pUserData = pUserData;` |
|   2686414 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2686414 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2686414 |  428 | `	return rc;` |
|   1343208 |  429 |  |
|    109508 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    109510 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
