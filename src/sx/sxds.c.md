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
|  161997632 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  161997637 |   16 | `	pSet->nSize = 0 ;` |
|  161997637 |   17 | `	pSet->nUsed = 0;` |
|  161997637 |   18 | `	pSet->nCursor = 0;` |
|  161997637 |   19 | `	pSet->eSize = ElemSize;` |
|  161997637 |   20 | `	pSet->pAllocator = pAllocator;` |
|  161997637 |   21 | `	pSet->pBase =  0;` |
|  161997637 |   22 | `	pSet->pUserData = 0;` |
|  161997637 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  368847913 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  368847918 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   21519757 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   21519757 |   33 | `		if( pSet->nSize <= 0 ){` |
|   18360535 |   34 | `			pSet->nSize = 4;` |
|    9180265 |   35 | `		}` |
|   21519757 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   21519757 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   21519757 |   40 | `		pSet->pBase = pNew;` |
|   21519757 |   41 | `		pSet->nSize <<= 1;` |
|   10759876 |   42 | `	}` |
|  368847918 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2909563336 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  368847918 |   45 | `	pSet->nUsed++;` |
|  368847918 |   46 | `	return SXRET_OK;` |
|  184423985 |   47 | `}` |
|   18298818 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   18298823 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   18298823 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   18298823 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   18298823 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   18298823 |   60 | `	pSet->nSize = nItem;` |
|   18298823 |   61 | `	return SXRET_OK;` |
|    9149414 |   62 | `}` |
|   27217231 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   27217236 |   65 | `	pSet->nUsed   = 0;` |
|   27217236 |   66 | `	pSet->nCursor = 0;` |
|   27217236 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      70144 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      70149 |   71 | `	pSet->nCursor = 0;` |
|      70149 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      74324 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      74329 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30343 |   79 | `		pSet->nCursor = 0;` |
|      30343 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43991 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43991 |   83 | `	if( ppEntry ){` |
|      43991 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21993 |   85 | `	}` |
|      43991 |   86 | `	pSet->nCursor++;` |
|      43991 |   87 | `	return SXRET_OK;` |
|      37167 |   88 | `}` |
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
|    2751552 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2751557 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1199 |  103 | `		pSet->nUsed = nNewSize;` |
|        597 |  104 | `	}` |
|    2751557 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   56262510 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   56262515 |  109 | `	sxi32 rc = SXRET_OK;` |
|   56262515 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   30641711 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   15320853 |  112 | `	}` |
|   56262515 |  113 | `	pSet->pBase = 0;` |
|   56262515 |  114 | `	pSet->nUsed = 0;` |
|   56262515 |  115 | `	pSet->nCursor = 0;` |
|   56262515 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   65483270 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   65483275 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19217 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   65464063 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   65464063 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   32741640 |  126 | `}` |
|    9401208 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9401213 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2232795 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7168423 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7168423 |  135 | `	pSet->nUsed--;` |
|    7168423 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7168423 |  137 | `	return pData;` |
|    4700609 |  138 | `}` |
|   34376344 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   34376349 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   34376327 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   34376327 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   17188341 |  148 | `}` |
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
|    1769824 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1769829 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1769829 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1769829 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1769829 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1769829 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1769829 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1769829 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1769829 |  180 | `	pHash->nEntry = 0;` |
|    1769829 |  181 | `	pHash->apBucket = apNew;` |
|    1769829 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1769829 |  183 | `	return SXRET_OK;` |
|     884917 |  184 | `}` |
|     412374 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     412379 |  193 | `	pEntry = pHash->pList;` |
|     220678 |  194 | `	for(;;){` |
|     441361 |  195 | `		if( pHash->nEntry == 0 ){` |
|     412379 |  196 | `			break;` |
|          - |  197 | `		}` |
|      28987 |  198 | `		pNext = pEntry->pNext;` |
|      28987 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      28987 |  200 | `		pEntry = pNext;` |
|      28987 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     412379 |  203 | `	if( pHash->apBucket ){` |
|     412379 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     206187 |  205 | `	}` |
|     412379 |  206 | `	pHash->apBucket = 0;` |
|     412379 |  207 | `	pHash->nBucketSize = 0;` |
|     412379 |  208 | `	pHash->pAllocator = 0;` |
|     412379 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   67292789 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   67292794 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   67292794 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   61072218 |  218 | `	for(;;){` |
|  122111887 |  219 | `		if( pEntry == 0 ){` |
|   24355218 |  220 | `			break;` |
|          - |  221 | `		}` |
|  119227333 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   42941600 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   42937581 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   54819098 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   24355218 |  229 | `	return 0;` |
|   33646683 |  230 | `}` |
|   74298837 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   74298842 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    7006505 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   67292342 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   67292342 |  244 | `	if( pEntry == 0 ){` |
|   24355200 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   42937147 |  247 | `	return (SyHashEntry *)pEntry;` |
|   37149707 |  248 | `}` |
|     437090 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     437095 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     356741 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     178373 |  254 | `	}else{` |
|      80359 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     437095 |  257 | `	if( pEntry->pNextCollide ){` |
|       4374 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2187 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     437095 |  261 | `	if( pHash->pLast == pEntry ){` |
|     427117 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     213556 |  263 | `	}` |
|     437095 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     437095 |  265 | `	pHash->nEntry--;` |
|     437095 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     437095 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     437095 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        452 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        457 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        457 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        439 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        439 |  288 | `	return rc;` |
|        231 |  289 | `}` |
|     436656 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     436661 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     436661 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     436661 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2831748 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2831753 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2831753 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   21179336 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   21179341 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2831487 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2831487 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18347859 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18347859 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18347859 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10589673 |  329 | `}` |
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
|       4083 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4073 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4073 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4073 |  348 | `		pEntry = pEntry->pNext;` |
|       2037 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      91876 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      91881 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      91881 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      91881 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      91881 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14423529 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14331653 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14331653 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14331653 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14331653 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    6870705 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3435260 |  375 | `		}` |
|   14331653 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14331653 |  378 | `		pEntry = pEntry->pNext;` |
|    7165829 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      91881 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      91881 |  382 | `	pHash->apBucket = apNew;` |
|      91881 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      91881 |  384 | `	return SXRET_OK;` |
|      45943 |  385 | `}` |
|   18479314 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   18479319 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   18479319 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   18479319 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11715498 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5857998 |  393 | `	}` |
|   18479319 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   18479319 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   18479267 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   18479319 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     976075 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     976075 |  408 | `		pHash->pLast = pEntry;` |
|     488035 |  409 | `	}` |
|   18479319 |  410 | `	pHash->nEntry++;` |
|   18479319 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   18479314 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   18479319 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      91881 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      91881 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45938 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   18479319 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   18479319 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   18479319 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   18479319 |  435 | `	pEntry->pHash = pHash;` |
|   18479319 |  436 | `	pEntry->pKey = pKey;` |
|   18479319 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   18479319 |  438 | `	pEntry->pUserData = pUserData;` |
|   18479319 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   18479319 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   18479319 |  442 | `	return rc;` |
|    9239662 |  443 | `}` |
|   18479180 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   18479185 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        134 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          2 |  455 | `{` |
|        136 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          2 |  457 | `}` |
|     476390 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     476395 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
