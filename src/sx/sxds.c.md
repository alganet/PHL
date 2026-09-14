# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 293/304 lines (96.38%)

[Root index](../../index.md) | [Directory index](index.md)

|       Hits | Line | Source |
| ---------: | ---: | :--- |
|          - |    1 | `/**` |
|          - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|          - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|          - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|          - |    5 | ` */` |
|          - |    6 | `#include "sxtypes.h"` |
|          - |    7 | `#include "sxmacros.h"` |
|          - |    8 | `#include "sxset.h"` |
|          - |    9 | `#include "sxmem.h"` |
|          - |   10 | `#include "sxhashtable.h"` |
|          - |   11 | `#include "sxhash.h"` |
|          - |   12 | `#include "sxstr.h"` |
|          - |   13 |  |
|  172770660 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  172770665 |   16 | `	pSet->nSize = 0 ;` |
|  172770665 |   17 | `	pSet->nUsed = 0;` |
|  172770665 |   18 | `	pSet->nCursor = 0;` |
|  172770665 |   19 | `	pSet->eSize = ElemSize;` |
|  172770665 |   20 | `	pSet->pAllocator = pAllocator;` |
|  172770665 |   21 | `	pSet->pBase =  0;` |
|  172770665 |   22 | `	pSet->pUserData = 0;` |
|  172770665 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  391499014 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  391499019 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   22560084 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   22560084 |   33 | `		if( pSet->nSize <= 0 ){` |
|   19219512 |   34 | `			pSet->nSize = 4;` |
|    9610428 |   35 | `		}` |
|   22560084 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   22560084 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   22560084 |   40 | `		pSet->pBase = pNew;` |
|   22560084 |   41 | `		pSet->nSize <<= 1;` |
|   11280714 |   42 | `	}` |
|  391499019 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3087634825 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  391499019 |   45 | `	pSet->nUsed++;` |
|  391499019 |   46 | `	return SXRET_OK;` |
|  195751972 |   47 | `}` |
|   19453570 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   19453575 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   19453575 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   19453575 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   19453575 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   19453575 |   60 | `	pSet->nSize = nItem;` |
|   19453575 |   61 | `	return SXRET_OK;` |
|    9726790 |   62 | `}` |
|   28940403 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   28940408 |   65 | `	pSet->nUsed   = 0;` |
|   28940408 |   66 | `	pSet->nCursor = 0;` |
|   28940408 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      71430 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      71435 |   71 | `	pSet->nCursor = 0;` |
|      71435 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      71852 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      71857 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30923 |   79 | `		pSet->nCursor = 0;` |
|      30923 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      40939 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      40939 |   83 | `	if( ppEntry ){` |
|      40939 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      20467 |   85 | `	}` |
|      40939 |   86 | `	pSet->nCursor++;` |
|      40939 |   87 | `	return SXRET_OK;` |
|      35931 |   88 | `}` |
|          - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|          8 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|          1 |   91 | `{` |
|          - |   92 | `	register unsigned char *zSrc;` |
|          9 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          3 |   94 | `		return 0;` |
|          - |   95 | `	}` |
|          7 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|          7 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|          5 |   98 | `}` |
|          - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    3007552 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3007557 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1203 |  103 | `		pSet->nUsed = nNewSize;` |
|        599 |  104 | `	}` |
|    3007557 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   59458096 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   59458101 |  109 | `	sxi32 rc = SXRET_OK;` |
|   59458101 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   32184254 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   16092799 |  112 | `	}` |
|   59458101 |  113 | `	pSet->pBase = 0;` |
|   59458101 |  114 | `	pSet->nUsed = 0;` |
|   59458101 |  115 | `	pSet->nCursor = 0;` |
|   59458101 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   69638012 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   69638017 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19539 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   69618483 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   69618483 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   34819011 |  126 | `}` |
|    9710705 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9710710 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2236309 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7474406 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7474406 |  135 | `	pSet->nUsed--;` |
|    7474406 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7474406 |  137 | `	return pData;` |
|    4855807 |  138 | `}` |
|   35946431 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   35946436 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   35946384 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   35946384 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   17977361 |  148 | `}` |
|          - |  149 | `/* Private hash entry */` |
|          - |  150 | `struct SyHashEntry_Pr` |
|          - |  151 | `{` |
|          - |  152 | `	const void *pKey; /* Hash key */` |
|          - |  153 | `	sxu32 nKeyLen;    /* Key length */` |
|          - |  154 | `	void *pUserData;  /* User private data */` |
|          - |  155 | `	/* Private fields */` |
|          - |  156 | `	sxu32 nHash;` |
|          - |  157 | `	SyHash *pHash;` |
|          - |  158 | `	SyHashEntry_Pr *pNext,*pPrev; /* Next and previous entry in the list */` |
|          - |  159 | `	SyHashEntry_Pr *pNextCollide,*pPrevCollide; /* Collision list */` |
|          - |  160 | `};` |
|          - |  161 | `#define INVALID_HASH(H) ((H)->apBucket == 0)` |
|    1904660 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1904665 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1904665 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1904665 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1904665 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1904665 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1904665 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1904665 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1904665 |  180 | `	pHash->nEntry = 0;` |
|    1904665 |  181 | `	pHash->apBucket = apNew;` |
|    1904665 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1904665 |  183 | `	return SXRET_OK;` |
|     952410 |  184 | `}` |
|     442658 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     442663 |  193 | `	pEntry = pHash->pList;` |
|     236123 |  194 | `	for(;;){` |
|     472101 |  195 | `		if( pHash->nEntry == 0 ){` |
|     442663 |  196 | `			break;` |
|          - |  197 | `		}` |
|      29443 |  198 | `		pNext = pEntry->pNext;` |
|      29443 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      29443 |  200 | `		pEntry = pNext;` |
|      29443 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     442663 |  203 | `	if( pHash->apBucket ){` |
|     442663 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     221404 |  205 | `	}` |
|     442663 |  206 | `	pHash->apBucket = 0;` |
|     442663 |  207 | `	pHash->nBucketSize = 0;` |
|     442663 |  208 | `	pHash->pAllocator = 0;` |
|     442663 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   72736422 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   72736427 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   72736427 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   65447958 |  218 | `	for(;;){` |
|  131035939 |  219 | `		if( pEntry == 0 ){` |
|   26753933 |  220 | `			break;` |
|          - |  221 | `		}` |
|  127272370 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   45986544 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   45982499 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   58299517 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   26753933 |  229 | `	return 0;` |
|   36374855 |  230 | `}` |
|   80167598 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   80167603 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    7431621 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   72735987 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   72735987 |  244 | `	if( pEntry == 0 ){` |
|   26753915 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   45982077 |  247 | `	return (SyHashEntry *)pEntry;` |
|   40090518 |  248 | `}` |
|     488926 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     488931 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     399802 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     200278 |  254 | `	}else{` |
|      89134 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     488931 |  257 | `	if( pEntry->pNextCollide ){` |
|       4672 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2334 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     488931 |  261 | `	if( pHash->pLast == pEntry ){` |
|     478849 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     239872 |  263 | `	}` |
|     488931 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     488931 |  265 | `	pHash->nEntry--;` |
|     488931 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     488931 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     488931 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        440 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        445 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        445 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        427 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        427 |  288 | `	return rc;` |
|        225 |  289 | `}` |
|     488504 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     488509 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     488509 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     488509 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3112354 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3112359 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3112359 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   23952856 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   23952861 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3112093 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3112093 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   20840773 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   20840773 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   20840773 |  328 | `	return (SyHashEntry *)pEntry;` |
|   11976433 |  329 | `}` |
|         14 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|          1 |  331 | `{` |
|          - |  332 | `	SyHashEntry_Pr *pEntry;` |
|          - |  333 | `	sxi32 rc;` |
|          - |  334 | `	sxu32 n;` |
|          - |  335 | `#if defined(UNTRUST)` |
|          - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|          - |  337 | `		return 0;` |
|          - |  338 | `	}` |
|          - |  339 | `#endif` |
|         15 |  340 | `	pEntry = pHash->pList;` |
|       4563 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4549 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4549 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4549 |  348 | `		pEntry = pEntry->pNext;` |
|       2275 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|     100332 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100337 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100337 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100337 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100337 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18631601 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18531269 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18531269 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18531269 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18531269 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8890161 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4445284 |  375 | `		}` |
|   18531269 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18531269 |  378 | `		pEntry = pEntry->pNext;` |
|    9265637 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100337 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100337 |  382 | `	pHash->apBucket = apNew;` |
|     100337 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100337 |  384 | `	return SXRET_OK;` |
|      50171 |  385 | `}` |
|   20566926 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   20566931 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   20566931 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   20566931 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   13112211 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    6556482 |  393 | `	}` |
|   20566931 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   20566931 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         63 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         63 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         63 |  401 | `		pHash->pLast = pEntry;` |
|         33 |  402 | `	}else{` |
|   20566871 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   20566931 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1066383 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1066383 |  408 | `		pHash->pLast = pEntry;` |
|     533264 |  409 | `	}` |
|   20566931 |  410 | `	pHash->nEntry++;` |
|   20566931 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   20566926 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   20566931 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100337 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100337 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50166 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   20566931 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   20566931 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   20566931 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   20566931 |  435 | `	pEntry->pHash = pHash;` |
|   20566931 |  436 | `	pEntry->pKey = pKey;` |
|   20566931 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   20566931 |  438 | `	pEntry->pUserData = pUserData;` |
|   20566931 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   20566931 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   20566931 |  442 | `	return rc;` |
|   10283918 |  443 | `}` |
|   20566774 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   20566779 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        152 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          4 |  455 | `{` |
|        156 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          4 |  457 | `}` |
|     528972 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     528977 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
