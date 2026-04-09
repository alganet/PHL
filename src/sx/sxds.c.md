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
|  14072256 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  14072258 |   16 | `	pSet->nSize = 0 ;` |
|  14072258 |   17 | `	pSet->nUsed = 0;` |
|  14072258 |   18 | `	pSet->nCursor = 0;` |
|  14072258 |   19 | `	pSet->eSize = ElemSize;` |
|  14072258 |   20 | `	pSet->pAllocator = pAllocator;` |
|  14072258 |   21 | `	pSet->pBase =  0;` |
|  14072258 |   22 | `	pSet->pUserData = 0;` |
|  14072258 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  23244082 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  23244084 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3870566 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3870566 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3764414 |   34 | `			pSet->nSize = 4;` |
|   1882206 |   35 | `		}` |
|   3870566 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3870566 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3870566 |   40 | `		pSet->pBase = pNew;` |
|   3870566 |   41 | `		pSet->nSize <<= 1;` |
|   1935282 |   42 | `	}` |
|  23244084 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 172713100 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  23244084 |   45 | `	pSet->nUsed++;` |
|  23244084 |   46 | `	return SXRET_OK;` |
|  11622065 |   47 |  |
|    836674 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    836676 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    836676 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    836676 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    836676 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    836676 |   60 | `	pSet->nSize = nItem;` |
|    836676 |   61 | `	return SXRET_OK;` |
|    418339 |   62 |  |
|   1288474 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1288476 |   65 | `	pSet->nUsed   = 0;` |
|   1288476 |   66 | `	pSet->nCursor = 0;` |
|   1288476 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     44656 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     44658 |   71 | `	pSet->nCursor = 0;` |
|     44658 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     48738 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     48740 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     18298 |   79 | `		pSet->nCursor = 0;` |
|     18298 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     30444 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     30444 |   83 | `	if( ppEntry ){` |
|     30444 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     15221 |   85 | `	}` |
|     30444 |   86 | `	pSet->nCursor++;` |
|     30444 |   87 | `	return SXRET_OK;` |
|     24371 |   88 |  |
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
|    138970 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    138972 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|    138972 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8354878 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8354880 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8354880 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4262714 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2131356 |  112 | `	}` |
|   8354880 |  113 | `	pSet->pBase = 0;` |
|   8354880 |  114 | `	pSet->nUsed = 0;` |
|   8354880 |  115 | `	pSet->nCursor = 0;` |
|   8354880 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4516566 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4516568 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       106 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4516464 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4516464 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2258285 |  126 |  |
|   3273900 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3273902 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2142510 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1131394 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1131394 |  135 | `	pSet->nUsed--;` |
|   1131394 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1131394 |  137 | `	return pData;` |
|   1636952 |  138 |  |
|  10790838 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  10790840 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  10790840 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  10790840 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5395613 |  148 |  |
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
|    248668 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    248670 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    248670 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    248670 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    248670 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    248670 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    248670 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    248670 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    248670 |  180 | `	pHash->nEntry = 0;` |
|    248670 |  181 | `	pHash->apBucket = apNew;` |
|    248670 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    248670 |  183 | `	return SXRET_OK;` |
|    124336 |  184 |  |
|     75002 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     75004 |  193 | `	pEntry = pHash->pList;` |
|     39286 |  194 | `	for(;;){` |
|     78574 |  195 | `		if( pHash->nEntry == 0 ){` |
|     75004 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3572 |  198 | `		pNext = pEntry->pNext;` |
|      3572 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3572 |  200 | `		pEntry = pNext;` |
|      3572 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     75004 |  203 | `	if( pHash->apBucket ){` |
|     75004 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     37501 |  205 | `	}` |
|     75004 |  206 | `	pHash->apBucket = 0;` |
|     75004 |  207 | `	pHash->nBucketSize = 0;` |
|     75004 |  208 | `	pHash->pAllocator = 0;` |
|     75004 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  11611980 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  11611982 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  11611982 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  10445999 |  218 | `	for(;;){` |
|  21021801 |  219 | `		if( pEntry == 0 ){` |
|   6399198 |  220 | `			break;` |
|         - |  221 | `		}` |
|  17228867 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5212788 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5212786 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   9409821 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   6399198 |  229 | `	return 0;` |
|   5806256 |  230 |  |
|  12062358 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  12062360 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    450402 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  11611960 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  11611960 |  244 | `	if( pEntry == 0 ){` |
|   6399198 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5212764 |  247 | `	return (SyHashEntry *)pEntry;` |
|   6031445 |  248 |  |
|     86216 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     86218 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     65442 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     32722 |  254 | `	}else{` |
|     20778 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     86218 |  257 | `	if( pEntry->pNextCollide ){` |
|      4519 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2259 |  259 | `	}` |
|     86218 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     86218 |  261 | `	pHash->nEntry--;` |
|     86218 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     86218 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     86218 |  268 | `	return rc;` |
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
|     86194 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     86196 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     86196 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     86196 |  296 | `	return rc;` |
|         2 |  297 |  |
|    299512 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    299514 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    299514 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2317360 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2317362 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    299080 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    299080 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2018284 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2018284 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2018284 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1158682 |  325 |  |
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
|     21754 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     21756 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     21756 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     21756 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     21756 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2762652 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2740898 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2740898 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2740898 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2740898 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1309813 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    654898 |  371 | `		}` |
|   2740898 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2740898 |  374 | `		pEntry = pEntry->pNext;` |
|   1370450 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     21756 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     21756 |  378 | `	pHash->apBucket = apNew;` |
|     21756 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     21756 |  380 | `	return SXRET_OK;` |
|     10879 |  381 |  |
|   2807020 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2807022 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2807022 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2807022 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1818738 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    909328 |  389 | `	}` |
|   2807022 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2807022 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2807022 |  393 | `	if( pHash->nEntry == 0 ){` |
|    124854 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     62426 |  395 | `	}` |
|   2807022 |  396 | `	pHash->nEntry++;` |
|   2807022 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2807020 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2807022 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     21756 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     21756 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     10877 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2807022 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2807022 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2807022 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2807022 |  421 | `	pEntry->pHash = pHash;` |
|   2807022 |  422 | `	pEntry->pKey = pKey;` |
|   2807022 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2807022 |  424 | `	pEntry->pUserData = pUserData;` |
|   2807022 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2807022 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2807022 |  428 | `	return rc;` |
|   1403512 |  429 |  |
|    110622 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    110624 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
