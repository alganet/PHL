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
|  140595412 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  140595417 |   16 | `	pSet->nSize = 0 ;` |
|  140595417 |   17 | `	pSet->nUsed = 0;` |
|  140595417 |   18 | `	pSet->nCursor = 0;` |
|  140595417 |   19 | `	pSet->eSize = ElemSize;` |
|  140595417 |   20 | `	pSet->pAllocator = pAllocator;` |
|  140595417 |   21 | `	pSet->pBase =  0;` |
|  140595417 |   22 | `	pSet->pUserData = 0;` |
|  140595417 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  314880532 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  314880537 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   18653699 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   18653699 |   33 | `		if( pSet->nSize <= 0 ){` |
|   15965041 |   34 | `			pSet->nSize = 4;` |
|    7982518 |   35 | `		}` |
|   18653699 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   18653699 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   18653699 |   40 | `		pSet->pBase = pNew;` |
|   18653699 |   41 | `		pSet->nSize <<= 1;` |
|    9326847 |   42 | `	}` |
|  314880537 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2477559911 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  314880537 |   45 | `	pSet->nUsed++;` |
|  314880537 |   46 | `	return SXRET_OK;` |
|  157440293 |   47 | `}` |
|   15429388 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   15429393 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   15429393 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   15429393 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   15429393 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   15429393 |   60 | `	pSet->nSize = nItem;` |
|   15429393 |   61 | `	return SXRET_OK;` |
|    7714699 |   62 | `}` |
|   22263730 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   22263735 |   65 | `	pSet->nUsed   = 0;` |
|   22263735 |   66 | `	pSet->nCursor = 0;` |
|   22263735 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      68988 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      68993 |   71 | `	pSet->nCursor = 0;` |
|      68993 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73088 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73093 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29853 |   79 | `		pSet->nCursor = 0;` |
|      29853 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43245 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43245 |   83 | `	if( ppEntry ){` |
|      43245 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21620 |   85 | `	}` |
|      43245 |   86 | `	pSet->nCursor++;` |
|      43245 |   87 | `	return SXRET_OK;` |
|      36549 |   88 | `}` |
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
|    2531540 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2531545 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1181 |  103 | `		pSet->nUsed = nNewSize;` |
|        588 |  104 | `	}` |
|    2531545 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   48654500 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   48654505 |  109 | `	sxi32 rc = SXRET_OK;` |
|   48654505 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   26025211 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   13012603 |  112 | `	}` |
|   48654505 |  113 | `	pSet->pBase = 0;` |
|   48654505 |  114 | `	pSet->nUsed = 0;` |
|   48654505 |  115 | `	pSet->nCursor = 0;` |
|   48654505 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   56566480 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   56566485 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15293 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   56551197 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   56551197 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   28283245 |  126 | `}` |
|    7995226 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    7995231 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2212027 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    5783209 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    5783209 |  135 | `	pSet->nUsed--;` |
|    5783209 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    5783209 |  137 | `	return pData;` |
|    3997618 |  138 | `}` |
|   29753613 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   29753618 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   29753596 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   29753596 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   14876888 |  148 | `}` |
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
|    1753182 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1753187 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1753187 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1753187 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1753187 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1753187 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1753187 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1753187 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1753187 |  180 | `	pHash->nEntry = 0;` |
|    1753187 |  181 | `	pHash->apBucket = apNew;` |
|    1753187 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1753187 |  183 | `	return SXRET_OK;` |
|     876596 |  184 | `}` |
|     405388 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     405393 |  193 | `	pEntry = pHash->pList;` |
|     215568 |  194 | `	for(;;){` |
|     431141 |  195 | `		if( pHash->nEntry == 0 ){` |
|     405393 |  196 | `			break;` |
|          - |  197 | `		}` |
|      25753 |  198 | `		pNext = pEntry->pNext;` |
|      25753 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      25753 |  200 | `		pEntry = pNext;` |
|      25753 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     405393 |  203 | `	if( pHash->apBucket ){` |
|     405393 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     202694 |  205 | `	}` |
|     405393 |  206 | `	pHash->apBucket = 0;` |
|     405393 |  207 | `	pHash->nBucketSize = 0;` |
|     405393 |  208 | `	pHash->pAllocator = 0;` |
|     405393 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   61463811 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   61463816 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   61463816 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   56360855 |  218 | `	for(;;){` |
|  112703574 |  219 | `		if( pEntry == 0 ){` |
|   23079612 |  220 | `			break;` |
|          - |  221 | `		}` |
|  108815984 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   38384298 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   38384209 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   51239763 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   23079612 |  229 | `	return 0;` |
|   30732176 |  230 | `}` |
|   67598647 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   67598652 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    6135193 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   61463464 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   61463464 |  244 | `	if( pEntry == 0 ){` |
|   23079594 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   38383875 |  247 | `	return (SyHashEntry *)pEntry;` |
|   33799594 |  248 | `}` |
|     419222 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     419227 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     344325 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     172165 |  254 | `	}else{` |
|      74907 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     419227 |  257 | `	if( pEntry->pNextCollide ){` |
|       4378 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2188 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     419227 |  261 | `	if( pHash->pLast == pEntry ){` |
|     412181 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     206088 |  263 | `	}` |
|     419227 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     419227 |  265 | `	pHash->nEntry--;` |
|     419227 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     419227 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     419227 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        352 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        357 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        357 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        339 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        339 |  288 | `	return rc;` |
|        181 |  289 | `}` |
|     418888 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     418893 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     418893 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     418893 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2805632 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2805637 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2805637 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   20846410 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   20846415 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2805371 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2805371 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18041049 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18041049 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18041049 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10423210 |  329 | `}` |
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
|       3823 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3813 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3813 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3813 |  348 | `		pEntry = pEntry->pNext;` |
|       1907 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      90766 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      90771 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      90771 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      90771 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      90771 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14275347 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14184581 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14184581 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14184581 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14184581 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    6786255 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3393084 |  375 | `		}` |
|   14184581 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14184581 |  378 | `		pEntry = pEntry->pNext;` |
|    7092293 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      90771 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      90771 |  382 | `	pHash->apBucket = apNew;` |
|      90771 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      90771 |  384 | `	return SXRET_OK;` |
|      45388 |  385 | `}` |
|   17385292 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   17385297 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   17385297 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   17385297 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   10808099 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5404118 |  393 | `	}` |
|   17385297 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   17385297 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   17385245 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   17385297 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     965905 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     965905 |  408 | `		pHash->pLast = pEntry;` |
|     482950 |  409 | `	}` |
|   17385297 |  410 | `	pHash->nEntry++;` |
|   17385297 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   17385292 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   17385297 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      90771 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      90771 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45383 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   17385297 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   17385297 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   17385297 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   17385297 |  435 | `	pEntry->pHash = pHash;` |
|   17385297 |  436 | `	pEntry->pKey = pKey;` |
|   17385297 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   17385297 |  438 | `	pEntry->pUserData = pUserData;` |
|   17385297 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   17385297 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   17385297 |  442 | `	return rc;` |
|    8692651 |  443 | `}` |
|   17385160 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   17385165 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        132 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          2 |  455 | `{` |
|        134 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          2 |  457 | `}` |
|     458282 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     458287 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
