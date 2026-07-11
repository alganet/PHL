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
|  20751204 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 | `{` |
|  20751209 |   16 | `	pSet->nSize = 0 ;` |
|  20751209 |   17 | `	pSet->nUsed = 0;` |
|  20751209 |   18 | `	pSet->nCursor = 0;` |
|  20751209 |   19 | `	pSet->eSize = ElemSize;` |
|  20751209 |   20 | `	pSet->pAllocator = pAllocator;` |
|  20751209 |   21 | `	pSet->pBase =  0;` |
|  20751209 |   22 | `	pSet->pUserData = 0;` |
|  20751209 |   23 | `	return SXRET_OK;` |
|         5 |   24 | `}` |
|  34394669 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 | `{` |
|         - |   27 | `	unsigned char *zbase;` |
|  34394674 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4855755 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4855755 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4684779 |   34 | `			pSet->nSize = 4;` |
|   2342387 |   35 | `		}` |
|   4855755 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4855755 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4855755 |   40 | `		pSet->pBase = pNew;` |
|   4855755 |   41 | `		pSet->nSize <<= 1;` |
|   2427875 |   42 | `	}` |
|  34394674 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 257131176 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  34394674 |   45 | `	pSet->nUsed++;` |
|  34394674 |   46 | `	return SXRET_OK;` |
|  17197382 |   47 | `}` |
|   1432902 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 | `{` |
|   1432907 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1432907 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1432907 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1432907 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1432907 |   60 | `	pSet->nSize = nItem;` |
|   1432907 |   61 | `	return SXRET_OK;` |
|    716456 |   62 | `}` |
|   2310671 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 | `{` |
|   2310676 |   65 | `	pSet->nUsed   = 0;` |
|   2310676 |   66 | `	pSet->nCursor = 0;` |
|   2310676 |   67 | `	return SXRET_OK;` |
|         5 |   68 | `}` |
|     66786 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 | `{` |
|     66791 |   71 | `	pSet->nCursor = 0;` |
|     66791 |   72 | `	return SXRET_OK;` |
|         5 |   73 | `}` |
|     70980 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 | `{` |
|         - |   76 | `	register unsigned char *zSrc;` |
|     70985 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     28517 |   79 | `		pSet->nCursor = 0;` |
|     28517 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     42473 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     42473 |   83 | `	if( ppEntry ){` |
|     42473 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     21234 |   85 | `	}` |
|     42473 |   86 | `	pSet->nCursor++;` |
|     42473 |   87 | `	return SXRET_OK;` |
|     35495 |   88 | `}` |
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
|    241072 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 | `{` |
|    241077 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       677 |  103 | `		pSet->nUsed = nNewSize;` |
|       336 |  104 | `	}` |
|    241077 |  105 | `	return SXRET_OK;` |
|         5 |  106 | `}` |
|  10586882 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 | `{` |
|  10586887 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10586887 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5318527 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2659261 |  112 | `	}` |
|  10586887 |  113 | `	pSet->pBase = 0;` |
|  10586887 |  114 | `	pSet->nUsed = 0;` |
|  10586887 |  115 | `	pSet->nCursor = 0;` |
|  10586887 |  116 | `	return rc;` |
|         5 |  117 | `}` |
|   6154364 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 | `{` |
|         - |  120 | `	const char *zBase;` |
|   6154369 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       133 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   6154241 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   6154241 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   3077187 |  126 | `}` |
|   3721732 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 | `{` |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3721737 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2193135 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1528607 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1528607 |  135 | `	pSet->nUsed--;` |
|   1528607 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1528607 |  137 | `	return pData;` |
|   1860871 |  138 | `}` |
|  14145265 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	const char *zBase;` |
|  14145270 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  14145270 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  14145270 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   7073003 |  148 | `}` |
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
|    682546 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 | `{` |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    682551 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    682551 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    682551 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    682551 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    682551 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    682551 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    682551 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    682551 |  180 | `	pHash->nEntry = 0;` |
|    682551 |  181 | `	pHash->apBucket = apNew;` |
|    682551 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    682551 |  183 | `	return SXRET_OK;` |
|    341278 |  184 | `}` |
|    154886 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 | `{` |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    154891 |  193 | `	pEntry = pHash->pList;` |
|     82113 |  194 | `	for(;;){` |
|    164231 |  195 | `		if( pHash->nEntry == 0 ){` |
|    154891 |  196 | `			break;` |
|         - |  197 | `		}` |
|      9345 |  198 | `		pNext = pEntry->pNext;` |
|      9345 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      9345 |  200 | `		pEntry = pNext;` |
|      9345 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    154891 |  203 | `	if( pHash->apBucket ){` |
|    154891 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     77443 |  205 | `	}` |
|    154891 |  206 | `	pHash->apBucket = 0;` |
|    154891 |  207 | `	pHash->nBucketSize = 0;` |
|    154891 |  208 | `	pHash->pAllocator = 0;` |
|    154891 |  209 | `	return SXRET_OK;` |
|         5 |  210 | `}` |
|  19190716 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 | `{` |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  19190721 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  19190721 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  17329646 |  218 | `	for(;;){` |
|  34671319 |  219 | `		if( pEntry == 0 ){` |
|  10179669 |  220 | `			break;` |
|         - |  221 | `		}` |
|  28996931 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   9011062 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   9011057 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  15480603 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|  10179669 |  229 | `	return 0;` |
|   9595873 |  230 | `}` |
|  20161348 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 | `{` |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  20161353 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    970861 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  19190497 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  19190497 |  244 | `	if( pEntry == 0 ){` |
|  10179669 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   9010833 |  247 | `	return (SyHashEntry *)pEntry;` |
|  10081189 |  248 | `}` |
|    176392 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 | `{` |
|         - |  251 | `	sxi32 rc;` |
|    176397 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    137865 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     68935 |  254 | `	}else{` |
|     38537 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    176397 |  257 | `	if( pEntry->pNextCollide ){` |
|      5200 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2599 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    176397 |  261 | `	if( pHash->pLast == pEntry ){` |
|    169911 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     84953 |  263 | `	}` |
|    176397 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    176397 |  265 | `	pHash->nEntry--;` |
|    176397 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    176397 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    176397 |  272 | `	return rc;` |
|         5 |  273 | `}` |
|       224 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 | `{` |
|         - |  276 | `	SyHashEntry_Pr *pEntry;` |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( INVALID_HASH(pHash) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|       229 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       229 |  284 | `	if( pEntry == 0 ){` |
|       ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|         - |  286 | `	}` |
|       229 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       229 |  288 | `	return rc;` |
|       117 |  289 | `}` |
|    176168 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 | `{` |
|    176173 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    176173 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    176173 |  300 | `	return rc;` |
|         5 |  301 | `}` |
|   1336958 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 | `{` |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1336963 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1336963 |  310 | `	return SXRET_OK;` |
|         5 |  311 | `}` |
|   8392060 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 | `{` |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   8392065 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1336701 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1336701 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   7055369 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   7055369 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   7055369 |  328 | `	return (SyHashEntry *)pEntry;` |
|   4196035 |  329 | `}` |
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
|      2077 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|      2067 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      2067 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|      2067 |  348 | `		pEntry = pEntry->pNext;` |
|      1034 |  349 | `	}` |
|        11 |  350 | `	return SXRET_OK;` |
|         6 |  351 | `}` |
|     33092 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  353 | `{` |
|     33097 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  355 | `	SyHashEntry_Pr *pEntry;` |
|         - |  356 | `	SyHashEntry_Pr **apNew;` |
|         - |  357 | `	sxu32 n,iBucket;` |
|         - |  358 |  |
|         - |  359 | `	/* Allocate a new larger table */` |
|     33097 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     33097 |  361 | `	if( apNew == 0 ){` |
|         - |  362 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  363 | `		return SXRET_OK;` |
|         - |  364 | `	}` |
|         - |  365 | `	/* Zero the new table */` |
|     33097 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  367 | `	/* Rehash all entries */` |
|   4171561 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   4138469 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  370 | `		/* Install in the new bucket */` |
|   4138469 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   4138469 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   4138469 |  373 | `		if( apNew[iBucket] != 0 ){` |
|   1986250 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    993233 |  375 | `		}` |
|   4138469 |  376 | `		apNew[iBucket] = pEntry;` |
|         - |  377 | `		/* Point to the next entry */` |
|   4138469 |  378 | `		pEntry = pEntry->pNext;` |
|   2069237 |  379 | `	}` |
|         - |  380 | `	/* Release the old table and reflect the change */` |
|     33097 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     33097 |  382 | `	pHash->apBucket = apNew;` |
|     33097 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     33097 |  384 | `	return SXRET_OK;` |
|     16551 |  385 | `}` |
|   5589166 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 | `{` |
|   5589171 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   5589171 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   5589171 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   3126657 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1563332 |  393 | `	}` |
|   5589171 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   5589171 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        51 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        51 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        51 |  401 | `		pHash->pLast = pEntry;` |
|        26 |  402 | `	}else{` |
|   5589121 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   5589171 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    360605 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    360605 |  408 | `		pHash->pLast = pEntry;` |
|    180300 |  409 | `	}` |
|   5589171 |  410 | `	pHash->nEntry++;` |
|   5589171 |  411 | `	return SXRET_OK;` |
|         5 |  412 | `}` |
|   5589166 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 | `{` |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   5589171 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     33097 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     33097 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     16546 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   5589171 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   5589171 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   5589171 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   5589171 |  435 | `	pEntry->pHash = pHash;` |
|   5589171 |  436 | `	pEntry->pKey = pKey;` |
|   5589171 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   5589171 |  438 | `	pEntry->pUserData = pUserData;` |
|   5589171 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   5589171 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   5589171 |  442 | `	return rc;` |
|   2794588 |  443 | `}` |
|   5589050 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 | `{` |
|   5589055 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|    216266 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 | `{` |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    216271 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 | `}` |
|         - |  468 |  |
