# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 290/304 lines (95.39%)

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
|  126681312 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  126681317 |   16 | `	pSet->nSize = 0 ;` |
|  126681317 |   17 | `	pSet->nUsed = 0;` |
|  126681317 |   18 | `	pSet->nCursor = 0;` |
|  126681317 |   19 | `	pSet->eSize = ElemSize;` |
|  126681317 |   20 | `	pSet->pAllocator = pAllocator;` |
|  126681317 |   21 | `	pSet->pBase =  0;` |
|  126681317 |   22 | `	pSet->pUserData = 0;` |
|  126681317 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  280516113 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  280516118 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   16784753 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   16784753 |   33 | `		if( pSet->nSize <= 0 ){` |
|   14441107 |   34 | `			pSet->nSize = 4;` |
|    7220551 |   35 | `		}` |
|   16784753 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   16784753 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   16784753 |   40 | `		pSet->pBase = pNew;` |
|   16784753 |   41 | `		pSet->nSize <<= 1;` |
|    8392374 |   42 | `	}` |
|  280516118 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2083991366 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  280516118 |   45 | `	pSet->nUsed++;` |
|  280516118 |   46 | `	return SXRET_OK;` |
|  140258105 |   47 | `}` |
|   13555092 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   13555097 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   13555097 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   13555097 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   13555097 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   13555097 |   60 | `	pSet->nSize = nItem;` |
|   13555097 |   61 | `	return SXRET_OK;` |
|    6777551 |   62 | `}` |
|   20016123 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   20016128 |   65 | `	pSet->nUsed   = 0;` |
|   20016128 |   66 | `	pSet->nCursor = 0;` |
|   20016128 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69286 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69291 |   71 | `	pSet->nCursor = 0;` |
|      69291 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73528 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73533 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29821 |   79 | `		pSet->nCursor = 0;` |
|      29821 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43717 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43717 |   83 | `	if( ppEntry ){` |
|      43717 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21856 |   85 | `	}` |
|      43717 |   86 | `	pSet->nCursor++;` |
|      43717 |   87 | `	return SXRET_OK;` |
|      36769 |   88 | `}` |
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
|    2353548 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2353553 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1179 |  103 | `		pSet->nUsed = nNewSize;` |
|        587 |  104 | `	}` |
|    2353553 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   43536968 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   43536973 |  109 | `	sxi32 rc = SXRET_OK;` |
|   43536973 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   22982429 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   11491212 |  112 | `	}` |
|   43536973 |  113 | `	pSet->pBase = 0;` |
|   43536973 |  114 | `	pSet->nUsed = 0;` |
|   43536973 |  115 | `	pSet->nCursor = 0;` |
|   43536973 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   50236538 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   50236543 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15621 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   50220927 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   50220927 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   25118274 |  126 | `}` |
|    7235898 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    7235903 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2198979 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    5036929 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    5036929 |  135 | `	pSet->nUsed--;` |
|    5036929 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    5036929 |  137 | `	return pData;` |
|    3617954 |  138 | `}` |
|   26252262 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   26252267 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   26252245 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   26252245 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   13126490 |  148 | `}` |
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
|    1639166 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1639171 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1639171 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1639171 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1639171 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1639171 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1639171 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1639171 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1639171 |  180 | `	pHash->nEntry = 0;` |
|    1639171 |  181 | `	pHash->apBucket = apNew;` |
|    1639171 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1639171 |  183 | `	return SXRET_OK;` |
|     819588 |  184 | `}` |
|     347782 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     347787 |  193 | `	pEntry = pHash->pList;` |
|     186325 |  194 | `	for(;;){` |
|     372655 |  195 | `		if( pHash->nEntry == 0 ){` |
|     347787 |  196 | `			break;` |
|          - |  197 | `		}` |
|      24873 |  198 | `		pNext = pEntry->pNext;` |
|      24873 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      24873 |  200 | `		pEntry = pNext;` |
|      24873 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     347787 |  203 | `	if( pHash->apBucket ){` |
|     347787 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     173891 |  205 | `	}` |
|     347787 |  206 | `	pHash->apBucket = 0;` |
|     347787 |  207 | `	pHash->nBucketSize = 0;` |
|     347787 |  208 | `	pHash->pAllocator = 0;` |
|     347787 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   55287457 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   55287462 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   55287462 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   51504909 |  218 | `	for(;;){` |
|  103003416 |  219 | `		if( pEntry == 0 ){` |
|   20836266 |  220 | `			break;` |
|          - |  221 | `		}` |
|   99392538 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   34451288 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   34451201 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   47715959 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   20836266 |  229 | `	return 0;` |
|   27644257 |  230 | `}` |
|   60590715 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   60590720 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    5303585 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   55287140 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   55287140 |  244 | `	if( pEntry == 0 ){` |
|   20836266 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   34450879 |  247 | `	return (SyHashEntry *)pEntry;` |
|   30295886 |  248 | `}` |
|     225260 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     225265 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     181473 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      90739 |  254 | `	}else{` |
|      43797 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     225265 |  257 | `	if( pEntry->pNextCollide ){` |
|       4334 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2167 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     225265 |  261 | `	if( pHash->pLast == pEntry ){` |
|     218279 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     109137 |  263 | `	}` |
|     225265 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     225265 |  265 | `	pHash->nEntry--;` |
|     225265 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|        ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        ! 0 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     225265 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     225265 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        322 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        327 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        327 |  284 | `	if( pEntry == 0 ){` |
|        ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        327 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        327 |  288 | `	return rc;` |
|        166 |  289 | `}` |
|     224938 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     224943 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     224943 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     224943 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2732564 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2732569 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2732569 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   20436696 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   20436701 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2732303 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2732303 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   17704403 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   17704403 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   17704403 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10218353 |  329 | `}` |
|         10 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|          1 |  331 | `{` |
|          - |  332 | `	SyHashEntry_Pr *pEntry;` |
|          - |  333 | `	sxi32 rc;` |
|          - |  334 | `	sxu32 n;` |
|          - |  335 | `#if defined(UNTRUST)` |
|          - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|          - |  337 | `		return 0;` |
|          - |  338 | `	}` |
|          - |  339 | `#endif` |
|         11 |  340 | `	pEntry = pHash->pList;` |
|       3641 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3631 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3631 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3631 |  348 | `		pEntry = pEntry->pNext;` |
|       1816 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      93428 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      93433 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      93433 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      93433 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      93433 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14715001 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14621573 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14621573 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14621573 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14621573 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    7001299 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3500594 |  375 | `		}` |
|   14621573 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14621573 |  378 | `		pEntry = pEntry->pNext;` |
|    7310789 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      93433 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      93433 |  382 | `	pHash->apBucket = apNew;` |
|      93433 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      93433 |  384 | `	return SXRET_OK;` |
|      46719 |  385 | `}` |
|   16348976 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   16348981 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   16348981 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   16348981 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   10155649 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5077886 |  393 | `	}` |
|   16348981 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   16348981 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   16348929 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   16348981 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     904459 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     904459 |  408 | `		pHash->pLast = pEntry;` |
|     452227 |  409 | `	}` |
|   16348981 |  410 | `	pHash->nEntry++;` |
|   16348981 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   16348976 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   16348981 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      93433 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      93433 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      46714 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   16348981 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   16348981 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   16348981 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   16348981 |  435 | `	pEntry->pHash = pHash;` |
|   16348981 |  436 | `	pEntry->pKey = pKey;` |
|   16348981 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   16348981 |  438 | `	pEntry->pUserData = pUserData;` |
|   16348981 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   16348981 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   16348981 |  442 | `	return rc;` |
|    8174493 |  443 | `}` |
|   16348848 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   16348853 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        128 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          2 |  455 | `{` |
|        130 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          2 |  457 | `}` |
|     265914 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     265919 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
