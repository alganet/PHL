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
|  160392384 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  160392389 |   16 | `	pSet->nSize = 0 ;` |
|  160392389 |   17 | `	pSet->nUsed = 0;` |
|  160392389 |   18 | `	pSet->nCursor = 0;` |
|  160392389 |   19 | `	pSet->eSize = ElemSize;` |
|  160392389 |   20 | `	pSet->pAllocator = pAllocator;` |
|  160392389 |   21 | `	pSet->pBase =  0;` |
|  160392389 |   22 | `	pSet->pUserData = 0;` |
|  160392389 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  365904169 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  365904174 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   21054399 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   21054399 |   33 | `		if( pSet->nSize <= 0 ){` |
|   17917865 |   34 | `			pSet->nSize = 4;` |
|    8958930 |   35 | `		}` |
|   21054399 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   21054399 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   21054399 |   40 | `		pSet->pBase = pNew;` |
|   21054399 |   41 | `		pSet->nSize <<= 1;` |
|   10527197 |   42 | `	}` |
|  365904174 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2889030446 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  365904174 |   45 | `	pSet->nUsed++;` |
|  365904174 |   46 | `	return SXRET_OK;` |
|  182952113 |   47 | `}` |
|   18172348 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   18172353 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   18172353 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   18172353 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   18172353 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   18172353 |   60 | `	pSet->nSize = nItem;` |
|   18172353 |   61 | `	return SXRET_OK;` |
|    9086179 |   62 | `}` |
|   27000827 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   27000832 |   65 | `	pSet->nUsed   = 0;` |
|   27000832 |   66 | `	pSet->nCursor = 0;` |
|   27000832 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69868 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69873 |   71 | `	pSet->nCursor = 0;` |
|      69873 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      74036 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      74041 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30229 |   79 | `		pSet->nCursor = 0;` |
|      30229 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43817 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43817 |   83 | `	if( ppEntry ){` |
|      43817 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21906 |   85 | `	}` |
|      43817 |   86 | `	pSet->nCursor++;` |
|      43817 |   87 | `	return SXRET_OK;` |
|      37023 |   88 | `}` |
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
|    2740978 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2740983 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1183 |  103 | `		pSet->nUsed = nNewSize;` |
|        589 |  104 | `	}` |
|    2740983 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   55265810 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   55265815 |  109 | `	sxi32 rc = SXRET_OK;` |
|   55265815 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   30105181 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   15052588 |  112 | `	}` |
|   55265815 |  113 | `	pSet->pBase = 0;` |
|   55265815 |  114 | `	pSet->nUsed = 0;` |
|   55265815 |  115 | `	pSet->nCursor = 0;` |
|   55265815 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   65124342 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   65124347 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19197 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   65105155 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   65105155 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   32562176 |  126 | `}` |
|    9007624 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9007629 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2231841 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    6775793 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    6775793 |  135 | `	pSet->nUsed--;` |
|    6775793 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    6775793 |  137 | `	return pData;` |
|    4503817 |  138 | `}` |
|   33526366 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   33526371 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   33526349 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   33526349 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   16763323 |  148 | `}` |
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
|    1766540 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1766545 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1766545 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1766545 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1766545 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1766545 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1766545 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1766545 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1766545 |  180 | `	pHash->nEntry = 0;` |
|    1766545 |  181 | `	pHash->apBucket = apNew;` |
|    1766545 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1766545 |  183 | `	return SXRET_OK;` |
|     883275 |  184 | `}` |
|     410596 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     410601 |  193 | `	pEntry = pHash->pList;` |
|     219170 |  194 | `	for(;;){` |
|     438345 |  195 | `		if( pHash->nEntry == 0 ){` |
|     410601 |  196 | `			break;` |
|          - |  197 | `		}` |
|      27749 |  198 | `		pNext = pEntry->pNext;` |
|      27749 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      27749 |  200 | `		pEntry = pNext;` |
|      27749 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     410601 |  203 | `	if( pHash->apBucket ){` |
|     410601 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     205298 |  205 | `	}` |
|     410601 |  206 | `	pHash->apBucket = 0;` |
|     410601 |  207 | `	pHash->nBucketSize = 0;` |
|     410601 |  208 | `	pHash->pAllocator = 0;` |
|     410601 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   66906089 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   66906094 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   66906094 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   60697799 |  218 | `	for(;;){` |
|  121503433 |  219 | `		if( pEntry == 0 ){` |
|   24233324 |  220 | `			break;` |
|          - |  221 | `		}` |
|  118608344 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   42676742 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   42672775 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   54597344 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   24233324 |  229 | `	return 0;` |
|   33453333 |  230 | `}` |
|   73862557 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   73862562 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    6956925 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   66905642 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   66905642 |  244 | `	if( pEntry == 0 ){` |
|   24233306 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   42672341 |  247 | `	return (SyHashEntry *)pEntry;` |
|   36931567 |  248 | `}` |
|     430488 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     430493 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     354047 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     177026 |  254 | `	}else{` |
|      76451 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     430493 |  257 | `	if( pEntry->pNextCollide ){` |
|       4244 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2121 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     430493 |  261 | `	if( pHash->pLast == pEntry ){` |
|     422757 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     211376 |  263 | `	}` |
|     430493 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     430493 |  265 | `	pHash->nEntry--;` |
|     430493 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     430493 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     430493 |  272 | `	return rc;` |
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
|     430054 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     430059 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     430059 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     430059 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2829448 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2829453 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2829453 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   21125278 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   21125283 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2829187 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2829187 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18296101 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18296101 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18296101 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10562644 |  329 | `}` |
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
|       4061 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4051 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4051 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4051 |  348 | `		pEntry = pEntry->pNext;` |
|       2026 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      91776 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      91781 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      91781 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      91781 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      91781 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14407685 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14315909 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14315909 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14315909 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14315909 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    6878654 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3439160 |  375 | `		}` |
|   14315909 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14315909 |  378 | `		pEntry = pEntry->pNext;` |
|    7157957 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      91781 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      91781 |  382 | `	pHash->apBucket = apNew;` |
|      91781 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      91781 |  384 | `	return SXRET_OK;` |
|      45893 |  385 | `}` |
|   18411014 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   18411019 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   18411019 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   18411019 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11654031 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5826855 |  393 | `	}` |
|   18411019 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   18411019 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   18410967 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   18411019 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     973873 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     973873 |  408 | `		pHash->pLast = pEntry;` |
|     486934 |  409 | `	}` |
|   18411019 |  410 | `	pHash->nEntry++;` |
|   18411019 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   18411014 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   18411019 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      91781 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      91781 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45888 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   18411019 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   18411019 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   18411019 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   18411019 |  435 | `	pEntry->pHash = pHash;` |
|   18411019 |  436 | `	pEntry->pKey = pKey;` |
|   18411019 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   18411019 |  438 | `	pEntry->pUserData = pUserData;` |
|   18411019 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   18411019 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   18411019 |  442 | `	return rc;` |
|    9205512 |  443 | `}` |
|   18410880 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   18410885 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     469722 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     469727 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
