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
|  20052874 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 | `{` |
|  20052879 |   16 | `	pSet->nSize = 0 ;` |
|  20052879 |   17 | `	pSet->nUsed = 0;` |
|  20052879 |   18 | `	pSet->nCursor = 0;` |
|  20052879 |   19 | `	pSet->eSize = ElemSize;` |
|  20052879 |   20 | `	pSet->pAllocator = pAllocator;` |
|  20052879 |   21 | `	pSet->pBase =  0;` |
|  20052879 |   22 | `	pSet->pUserData = 0;` |
|  20052879 |   23 | `	return SXRET_OK;` |
|         5 |   24 | `}` |
|  33159623 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 | `{` |
|         - |   27 | `	unsigned char *zbase;` |
|  33159628 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4708543 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4708543 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4544303 |   34 | `			pSet->nSize = 4;` |
|   2272149 |   35 | `		}` |
|   4708543 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4708543 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4708543 |   40 | `		pSet->pBase = pNew;` |
|   4708543 |   41 | `		pSet->nSize <<= 1;` |
|   2354269 |   42 | `	}` |
|  33159628 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 248458476 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  33159628 |   45 | `	pSet->nUsed++;` |
|  33159628 |   46 | `	return SXRET_OK;` |
|  16579860 |   47 | `}` |
|   1376034 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 | `{` |
|   1376039 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1376039 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1376039 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1376039 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1376039 |   60 | `	pSet->nSize = nItem;` |
|   1376039 |   61 | `	return SXRET_OK;` |
|    688022 |   62 | `}` |
|   1906763 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 | `{` |
|   1906768 |   65 | `	pSet->nUsed   = 0;` |
|   1906768 |   66 | `	pSet->nCursor = 0;` |
|   1906768 |   67 | `	return SXRET_OK;` |
|         5 |   68 | `}` |
|     59052 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 | `{` |
|     59057 |   71 | `	pSet->nCursor = 0;` |
|     59057 |   72 | `	return SXRET_OK;` |
|         5 |   73 | `}` |
|     63250 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 | `{` |
|         - |   76 | `	register unsigned char *zSrc;` |
|     63255 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     24521 |   79 | `		pSet->nCursor = 0;` |
|     24521 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     38739 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     38739 |   83 | `	if( ppEntry ){` |
|     38739 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     19367 |   85 | `	}` |
|     38739 |   86 | `	pSet->nCursor++;` |
|     38739 |   87 | `	return SXRET_OK;` |
|     31630 |   88 | `}` |
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
|    230540 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 | `{` |
|    230545 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       295 |  103 | `		pSet->nUsed = nNewSize;` |
|       145 |  104 | `	}` |
|    230545 |  105 | `	return SXRET_OK;` |
|         5 |  106 | `}` |
|  10292266 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 | `{` |
|  10292271 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10292271 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5159111 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2579553 |  112 | `	}` |
|  10292271 |  113 | `	pSet->pBase = 0;` |
|  10292271 |  114 | `	pSet->nUsed = 0;` |
|  10292271 |  115 | `	pSet->nCursor = 0;` |
|  10292271 |  116 | `	return rc;` |
|         5 |  117 | `}` |
|   5982860 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 | `{` |
|         - |  120 | `	const char *zBase;` |
|   5982865 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       133 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5982737 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5982737 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2991435 |  126 | `}` |
|   3643058 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 | `{` |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3643063 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2186015 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1457053 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1457053 |  135 | `	pSet->nUsed--;` |
|   1457053 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1457053 |  137 | `	return pData;` |
|   1821534 |  138 | `}` |
|  13754778 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	const char *zBase;` |
|  13754783 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13754783 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13754783 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6877787 |  148 | `}` |
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
|    643104 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 | `{` |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    643109 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    643109 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    643109 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    643109 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    643109 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    643109 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    643109 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    643109 |  180 | `	pHash->nEntry = 0;` |
|    643109 |  181 | `	pHash->apBucket = apNew;` |
|    643109 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    643109 |  183 | `	return SXRET_OK;` |
|    321557 |  184 | `}` |
|    137362 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 | `{` |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    137367 |  193 | `	pEntry = pHash->pList;` |
|     72647 |  194 | `	for(;;){` |
|    145299 |  195 | `		if( pHash->nEntry == 0 ){` |
|    137367 |  196 | `			break;` |
|         - |  197 | `		}` |
|      7937 |  198 | `		pNext = pEntry->pNext;` |
|      7937 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      7937 |  200 | `		pEntry = pNext;` |
|      7937 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    137367 |  203 | `	if( pHash->apBucket ){` |
|    137367 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     68681 |  205 | `	}` |
|    137367 |  206 | `	pHash->apBucket = 0;` |
|    137367 |  207 | `	pHash->nBucketSize = 0;` |
|    137367 |  208 | `	pHash->pAllocator = 0;` |
|    137367 |  209 | `	return SXRET_OK;` |
|         5 |  210 | `}` |
|  18340106 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 | `{` |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  18340111 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  18340111 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  16579440 |  218 | `	for(;;){` |
|  33197220 |  219 | `		if( pEntry == 0 ){` |
|   9759819 |  220 | `			break;` |
|         - |  221 | `		}` |
|  27727296 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   8580302 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   8580297 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  14857114 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   9759819 |  229 | `	return 0;` |
|   9170580 |  230 | `}` |
|  19258170 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 | `{` |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  19258175 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    918279 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  18339901 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  18339901 |  244 | `	if( pEntry == 0 ){` |
|   9759819 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   8580087 |  247 | `	return (SyHashEntry *)pEntry;` |
|   9629612 |  248 | `}` |
|    134104 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 | `{` |
|         - |  251 | `	sxi32 rc;` |
|    134109 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    103991 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     51998 |  254 | `	}else{` |
|     30123 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    134109 |  257 | `	if( pEntry->pNextCollide ){` |
|      5235 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2617 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    134109 |  261 | `	if( pHash->pLast == pEntry ){` |
|    127803 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     63899 |  263 | `	}` |
|    134109 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    134109 |  265 | `	pHash->nEntry--;` |
|    134109 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    134109 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    134109 |  272 | `	return rc;` |
|         5 |  273 | `}` |
|       210 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 | `{` |
|         - |  276 | `	SyHashEntry_Pr *pEntry;` |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( INVALID_HASH(pHash) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|       215 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       215 |  284 | `	if( pEntry == 0 ){` |
|       ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|         - |  286 | `	}` |
|       215 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       215 |  288 | `	return rc;` |
|       110 |  289 | `}` |
|    133894 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 | `{` |
|    133899 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    133899 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    133899 |  300 | `	return rc;` |
|         5 |  301 | `}` |
|   1290292 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 | `{` |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1290297 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1290297 |  310 | `	return SXRET_OK;` |
|         5 |  311 | `}` |
|   8114640 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 | `{` |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   8114645 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1290035 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1290035 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   6824615 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   6824615 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   6824615 |  328 | `	return (SyHashEntry *)pEntry;` |
|   4057325 |  329 | `}` |
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
|      2039 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|      2029 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      2029 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|      2029 |  348 | `		pEntry = pEntry->pNext;` |
|      1015 |  349 | `	}` |
|        11 |  350 | `	return SXRET_OK;` |
|         6 |  351 | `}` |
|     31632 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  353 | `{` |
|     31637 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  355 | `	SyHashEntry_Pr *pEntry;` |
|         - |  356 | `	SyHashEntry_Pr **apNew;` |
|         - |  357 | `	sxu32 n,iBucket;` |
|         - |  358 |  |
|         - |  359 | `	/* Allocate a new larger table */` |
|     31637 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     31637 |  361 | `	if( apNew == 0 ){` |
|         - |  362 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  363 | `		return SXRET_OK;` |
|         - |  364 | `	}` |
|         - |  365 | `	/* Zero the new table */` |
|     31637 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  367 | `	/* Rehash all entries */` |
|   3987989 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3956357 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  370 | `		/* Install in the new bucket */` |
|   3956357 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3956357 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3956357 |  373 | `		if( apNew[iBucket] != 0 ){` |
|   1898810 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    949436 |  375 | `		}` |
|   3956357 |  376 | `		apNew[iBucket] = pEntry;` |
|         - |  377 | `		/* Point to the next entry */` |
|   3956357 |  378 | `		pEntry = pEntry->pNext;` |
|   1978181 |  379 | `	}` |
|         - |  380 | `	/* Release the old table and reflect the change */` |
|     31637 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     31637 |  382 | `	pHash->apBucket = apNew;` |
|     31637 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     31637 |  384 | `	return SXRET_OK;` |
|     15821 |  385 | `}` |
|   5314066 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 | `{` |
|   5314071 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   5314071 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   5314071 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2984742 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1492326 |  393 | `	}` |
|   5314071 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   5314071 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        51 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        51 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        51 |  401 | `		pHash->pLast = pEntry;` |
|        26 |  402 | `	}else{` |
|   5314021 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   5314071 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    332663 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    332663 |  408 | `		pHash->pLast = pEntry;` |
|    166329 |  409 | `	}` |
|   5314071 |  410 | `	pHash->nEntry++;` |
|   5314071 |  411 | `	return SXRET_OK;` |
|         5 |  412 | `}` |
|   5314066 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 | `{` |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   5314071 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     31637 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     31637 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     15816 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   5314071 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   5314071 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   5314071 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   5314071 |  435 | `	pEntry->pHash = pHash;` |
|   5314071 |  436 | `	pEntry->pKey = pKey;` |
|   5314071 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   5314071 |  438 | `	pEntry->pUserData = pUserData;` |
|   5314071 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   5314071 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   5314071 |  442 | `	return rc;` |
|   2657038 |  443 | `}` |
|   5313950 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 | `{` |
|   5313955 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|    172098 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 | `{` |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    172103 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 | `}` |
|         - |  468 |  |
