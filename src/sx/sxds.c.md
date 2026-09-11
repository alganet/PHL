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
|  162085408 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  162085413 |   16 | `	pSet->nSize = 0 ;` |
|  162085413 |   17 | `	pSet->nUsed = 0;` |
|  162085413 |   18 | `	pSet->nCursor = 0;` |
|  162085413 |   19 | `	pSet->eSize = ElemSize;` |
|  162085413 |   20 | `	pSet->pAllocator = pAllocator;` |
|  162085413 |   21 | `	pSet->pBase =  0;` |
|  162085413 |   22 | `	pSet->pUserData = 0;` |
|  162085413 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  369044155 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  369044160 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   21530985 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   21530985 |   33 | `		if( pSet->nSize <= 0 ){` |
|   18370077 |   34 | `			pSet->nSize = 4;` |
|    9185036 |   35 | `		}` |
|   21530985 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   21530985 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   21530985 |   40 | `		pSet->pBase = pNew;` |
|   21530985 |   41 | `		pSet->nSize <<= 1;` |
|   10765490 |   42 | `	}` |
|  369044160 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2911092854 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  369044160 |   45 | `	pSet->nUsed++;` |
|  369044160 |   46 | `	return SXRET_OK;` |
|  184522106 |   47 | `}` |
|   18308584 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   18308589 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   18308589 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   18308589 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   18308589 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   18308589 |   60 | `	pSet->nSize = nItem;` |
|   18308589 |   61 | `	return SXRET_OK;` |
|    9154297 |   62 | `}` |
|   27232821 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   27232826 |   65 | `	pSet->nUsed   = 0;` |
|   27232826 |   66 | `	pSet->nCursor = 0;` |
|   27232826 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      70290 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      70295 |   71 | `	pSet->nCursor = 0;` |
|      70295 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      74472 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      74477 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30409 |   79 | `		pSet->nCursor = 0;` |
|      30409 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      44073 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      44073 |   83 | `	if( ppEntry ){` |
|      44073 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      22034 |   85 | `	}` |
|      44073 |   86 | `	pSet->nCursor++;` |
|      44073 |   87 | `	return SXRET_OK;` |
|      37241 |   88 | `}` |
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
|    2753020 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2753025 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1199 |  103 | `		pSet->nUsed = nNewSize;` |
|        597 |  104 | `	}` |
|    2753025 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   56291830 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   56291835 |  109 | `	sxi32 rc = SXRET_OK;` |
|   56291835 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   30657781 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   15328888 |  112 | `	}` |
|   56291835 |  113 | `	pSet->pBase = 0;` |
|   56291835 |  114 | `	pSet->nUsed = 0;` |
|   56291835 |  115 | `	pSet->nCursor = 0;` |
|   56291835 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   65517188 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   65517193 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19227 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   65497971 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   65497971 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   32758599 |  126 | `}` |
|    9405922 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9405927 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2232889 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7173043 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7173043 |  135 | `	pSet->nUsed--;` |
|    7173043 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7173043 |  137 | `	return pData;` |
|    4702966 |  138 | `}` |
|   34401136 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   34401141 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   34401119 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   34401119 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   17200741 |  148 | `}` |
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
|    1770926 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1770931 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1770931 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1770931 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1770931 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1770931 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1770931 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1770931 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1770931 |  180 | `	pHash->nEntry = 0;` |
|    1770931 |  181 | `	pHash->apBucket = apNew;` |
|    1770931 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1770931 |  183 | `	return SXRET_OK;` |
|     885468 |  184 | `}` |
|     412676 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     412681 |  193 | `	pEntry = pHash->pList;` |
|     220834 |  194 | `	for(;;){` |
|     441673 |  195 | `		if( pHash->nEntry == 0 ){` |
|     412681 |  196 | `			break;` |
|          - |  197 | `		}` |
|      28997 |  198 | `		pNext = pEntry->pNext;` |
|      28997 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      28997 |  200 | `		pEntry = pNext;` |
|      28997 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     412681 |  203 | `	if( pHash->apBucket ){` |
|     412681 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     206338 |  205 | `	}` |
|     412681 |  206 | `	pHash->apBucket = 0;` |
|     412681 |  207 | `	pHash->nBucketSize = 0;` |
|     412681 |  208 | `	pHash->pAllocator = 0;` |
|     412681 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   67334515 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   67334520 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   67334520 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   61173043 |  218 | `	for(;;){` |
|  122459646 |  219 | `		if( pEntry == 0 ){` |
|   24371138 |  220 | `			break;` |
|          - |  221 | `		}` |
|  119572076 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   42967408 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   42963387 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   55125131 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   24371138 |  229 | `	return 0;` |
|   33667546 |  230 | `}` |
|   74344425 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   74344430 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    7010391 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   67334044 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   67334044 |  244 | `	if( pEntry == 0 ){` |
|   24371120 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   42962929 |  247 | `	return (SyHashEntry *)pEntry;` |
|   37172501 |  248 | `}` |
|     437366 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     437371 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     356953 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     178479 |  254 | `	}else{` |
|      80423 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     437371 |  257 | `	if( pEntry->pNextCollide ){` |
|       4386 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2192 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     437371 |  261 | `	if( pHash->pLast == pEntry ){` |
|     427383 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     213689 |  263 | `	}` |
|     437371 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     437371 |  265 | `	pHash->nEntry--;` |
|     437371 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     437371 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     437371 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        476 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        481 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        481 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        463 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        463 |  288 | `	return rc;` |
|        243 |  289 | `}` |
|     436908 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     436913 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     436913 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     436913 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2838038 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2838043 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2838043 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   21219478 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   21219483 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2837777 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2837777 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18381711 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18381711 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18381711 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10609744 |  329 | `}` |
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
|       4099 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4085 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4085 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4085 |  348 | `		pEntry = pEntry->pNext;` |
|       2043 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|      91928 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      91933 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      91933 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      91933 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      91933 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14432989 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14341061 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14341061 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14341061 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14341061 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    6875289 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3437540 |  375 | `		}` |
|   14341061 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14341061 |  378 | `		pEntry = pEntry->pNext;` |
|    7170533 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      91933 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      91933 |  382 | `	pHash->apBucket = apNew;` |
|      91933 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      91933 |  384 | `	return SXRET_OK;` |
|      45969 |  385 | `}` |
|   18489540 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   18489545 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   18489545 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   18489545 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11722451 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5861256 |  393 | `	}` |
|   18489545 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   18489545 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   18489493 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   18489545 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     976657 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     976657 |  408 | `		pHash->pLast = pEntry;` |
|     488326 |  409 | `	}` |
|   18489545 |  410 | `	pHash->nEntry++;` |
|   18489545 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   18489540 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   18489545 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      91933 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      91933 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45964 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   18489545 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   18489545 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   18489545 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   18489545 |  435 | `	pEntry->pHash = pHash;` |
|   18489545 |  436 | `	pEntry->pKey = pKey;` |
|   18489545 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   18489545 |  438 | `	pEntry->pUserData = pUserData;` |
|   18489545 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   18489545 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   18489545 |  442 | `	return rc;` |
|    9244775 |  443 | `}` |
|   18489406 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   18489411 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     476670 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     476675 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
