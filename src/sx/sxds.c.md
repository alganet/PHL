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
|  160450916 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  160450921 |   16 | `	pSet->nSize = 0 ;` |
|  160450921 |   17 | `	pSet->nUsed = 0;` |
|  160450921 |   18 | `	pSet->nCursor = 0;` |
|  160450921 |   19 | `	pSet->eSize = ElemSize;` |
|  160450921 |   20 | `	pSet->pAllocator = pAllocator;` |
|  160450921 |   21 | `	pSet->pBase =  0;` |
|  160450921 |   22 | `	pSet->pUserData = 0;` |
|  160450921 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  366209659 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  366209664 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   21022703 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   21022703 |   33 | `		if( pSet->nSize <= 0 ){` |
|   17883519 |   34 | `			pSet->nSize = 4;` |
|    8941757 |   35 | `		}` |
|   21022703 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   21022703 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   21022703 |   40 | `		pSet->pBase = pNew;` |
|   21022703 |   41 | `		pSet->nSize <<= 1;` |
|   10511349 |   42 | `	}` |
|  366209664 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2892349020 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  366209664 |   45 | `	pSet->nUsed++;` |
|  366209664 |   46 | `	return SXRET_OK;` |
|  183104858 |   47 | `}` |
|   18197476 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   18197481 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   18197481 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   18197481 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   18197481 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   18197481 |   60 | `	pSet->nSize = nItem;` |
|   18197481 |   61 | `	return SXRET_OK;` |
|    9098743 |   62 | `}` |
|   27026081 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   27026086 |   65 | `	pSet->nUsed   = 0;` |
|   27026086 |   66 | `	pSet->nCursor = 0;` |
|   27026086 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69984 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69989 |   71 | `	pSet->nCursor = 0;` |
|      69989 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      74152 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      74157 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30275 |   79 | `		pSet->nCursor = 0;` |
|      30275 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43887 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43887 |   83 | `	if( ppEntry ){` |
|      43887 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21941 |   85 | `	}` |
|      43887 |   86 | `	pSet->nCursor++;` |
|      43887 |   87 | `	return SXRET_OK;` |
|      37081 |   88 | `}` |
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
|    2743408 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2743413 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1199 |  103 | `		pSet->nUsed = nNewSize;` |
|        597 |  104 | `	}` |
|    2743413 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   55183756 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   55183761 |  109 | `	sxi32 rc = SXRET_OK;` |
|   55183761 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   30077239 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   15038617 |  112 | `	}` |
|   55183761 |  113 | `	pSet->pBase = 0;` |
|   55183761 |  114 | `	pSet->nUsed = 0;` |
|   55183761 |  115 | `	pSet->nCursor = 0;` |
|   55183761 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   65192012 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   65192017 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19187 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   65172835 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   65172835 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   32596011 |  126 | `}` |
|    8942308 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    8942313 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2232279 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    6710039 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    6710039 |  135 | `	pSet->nUsed--;` |
|    6710039 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    6710039 |  137 | `	return pData;` |
|    4471159 |  138 | `}` |
|   33414760 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   33414765 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   33414743 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   33414743 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   16707532 |  148 | `}` |
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
|    1766922 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1766927 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1766927 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1766927 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1766927 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1766927 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1766927 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1766927 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1766927 |  180 | `	pHash->nEntry = 0;` |
|    1766927 |  181 | `	pHash->apBucket = apNew;` |
|    1766927 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1766927 |  183 | `	return SXRET_OK;` |
|     883466 |  184 | `}` |
|     411636 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     411641 |  193 | `	pEntry = pHash->pList;` |
|     220199 |  194 | `	for(;;){` |
|     440403 |  195 | `		if( pHash->nEntry == 0 ){` |
|     411641 |  196 | `			break;` |
|          - |  197 | `		}` |
|      28767 |  198 | `		pNext = pEntry->pNext;` |
|      28767 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      28767 |  200 | `		pEntry = pNext;` |
|      28767 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     411641 |  203 | `	if( pHash->apBucket ){` |
|     411641 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     205818 |  205 | `	}` |
|     411641 |  206 | `	pHash->apBucket = 0;` |
|     411641 |  207 | `	pHash->nBucketSize = 0;` |
|     411641 |  208 | `	pHash->pAllocator = 0;` |
|     411641 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   66982479 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   66982484 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   66982484 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   60498815 |  218 | `	for(;;){` |
|  121017028 |  219 | `		if( pEntry == 0 ){` |
|   24253534 |  220 | `			break;` |
|          - |  221 | `		}` |
|  118129818 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   42732920 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   42728955 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   54034549 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   24253534 |  229 | `	return 0;` |
|   33491528 |  230 | `}` |
|   73950703 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   73950708 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    6968681 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   66982032 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   66982032 |  244 | `	if( pEntry == 0 ){` |
|   24253516 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   42728521 |  247 | `	return (SyHashEntry *)pEntry;` |
|   36975640 |  248 | `}` |
|     432736 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     432741 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     356247 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     178126 |  254 | `	}else{` |
|      76499 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     432741 |  257 | `	if( pEntry->pNextCollide ){` |
|       4234 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2116 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     432741 |  261 | `	if( pHash->pLast == pEntry ){` |
|     422867 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     211431 |  263 | `	}` |
|     432741 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     432741 |  265 | `	pHash->nEntry--;` |
|     432741 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     432741 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     432741 |  272 | `	return rc;` |
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
|     432302 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     432307 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     432307 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     432307 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2826752 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2826757 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2826757 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   21125434 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   21125439 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2826491 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2826491 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18298953 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18298953 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18298953 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10562722 |  329 | `}` |
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
|       4067 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4057 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4057 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4057 |  348 | `		pEntry = pEntry->pNext;` |
|       2029 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      91726 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      91731 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      91731 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      91731 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      91731 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14399763 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14308037 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14308037 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14308037 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14308037 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    6866880 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3433360 |  375 | `		}` |
|   14308037 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14308037 |  378 | `		pEntry = pEntry->pNext;` |
|    7154021 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      91731 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      91731 |  382 | `	pHash->apBucket = apNew;` |
|      91731 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      91731 |  384 | `	return SXRET_OK;` |
|      45868 |  385 | `}` |
|   18419482 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   18419487 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   18419487 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   18419487 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11662724 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5831179 |  393 | `	}` |
|   18419487 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   18419487 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   18419435 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   18419487 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     974469 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     974469 |  408 | `		pHash->pLast = pEntry;` |
|     487232 |  409 | `	}` |
|   18419487 |  410 | `	pHash->nEntry++;` |
|   18419487 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   18419482 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   18419487 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      91731 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      91731 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45863 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   18419487 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   18419487 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   18419487 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   18419487 |  435 | `	pEntry->pHash = pHash;` |
|   18419487 |  436 | `	pEntry->pKey = pKey;` |
|   18419487 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   18419487 |  438 | `	pEntry->pUserData = pUserData;` |
|   18419487 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   18419487 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   18419487 |  442 | `	return rc;` |
|    9209746 |  443 | `}` |
|   18419348 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   18419353 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     471954 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     471959 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
