# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 296/315 lines (93.97%)

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
|  266380607 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  266380612 |   16 | `	pSet->nSize = 0 ;` |
|  266380612 |   17 | `	pSet->nUsed = 0;` |
|  266380612 |   18 | `	pSet->nCursor = 0;` |
|  266380612 |   19 | `	pSet->eSize = ElemSize;` |
|  266380612 |   20 | `	pSet->pAllocator = pAllocator;` |
|  266380612 |   21 | `	pSet->pBase =  0;` |
|  266380612 |   22 | `	pSet->pUserData = 0;` |
|  266380612 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  500609975 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  500609980 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   43619243 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   43619243 |   33 | `		if( pSet->nSize <= 0 ){` |
|   39797701 |   34 | `			pSet->nSize = 4;` |
|   19900310 |   35 | `		}` |
|   43619243 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   43619243 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   43619243 |   40 | `		pSet->pBase = pNew;` |
|   43619243 |   41 | `		pSet->nSize <<= 1;` |
|   21811081 |   42 | `	}` |
|  500609980 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3772721380 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  500609980 |   45 | `	pSet->nUsed++;` |
|  500609980 |   46 | `	return SXRET_OK;` |
|  250309981 |   47 | `}` |
|   22217008 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   22217013 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   22217013 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   22217013 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   22217013 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   22217013 |   60 | `	pSet->nSize = nItem;` |
|   22217013 |   61 | `	return SXRET_OK;` |
|   11108509 |   62 | `}` |
|   36822801 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   36822806 |   65 | `	pSet->nUsed   = 0;` |
|   36822806 |   66 | `	pSet->nCursor = 0;` |
|   36822806 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      99702 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      99707 |   71 | `	pSet->nCursor = 0;` |
|      99707 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|     100004 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|     100009 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      45319 |   79 | `		pSet->nCursor = 0;` |
|      45319 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      54695 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      54695 |   83 | `	if( ppEntry ){` |
|      54695 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      27345 |   85 | `	}` |
|      54695 |   86 | `	pSet->nCursor++;` |
|      54695 |   87 | `	return SXRET_OK;` |
|      50007 |   88 | `}` |
|          - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        ! 0 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|        ! 0 |   91 | `{` |
|          - |   92 | `	register unsigned char *zSrc;` |
|        ! 0 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|        ! 0 |   94 | `		return 0;` |
|          - |   95 | `	}` |
|        ! 0 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|        ! 0 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|        ! 0 |   98 | `}` |
|          - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    3499848 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3499853 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1263 |  103 | `		pSet->nUsed = nNewSize;` |
|        629 |  104 | `	}` |
|    3499853 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|  129308099 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|  129308104 |  109 | `	sxi32 rc = SXRET_OK;` |
|  129308104 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   54325713 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   27164316 |  112 | `	}` |
|  129308104 |  113 | `	pSet->pBase = 0;` |
|  129308104 |  114 | `	pSet->nUsed = 0;` |
|  129308104 |  115 | `	pSet->nCursor = 0;` |
|  129308104 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   83015558 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   83015563 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       4267 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   83011301 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   83011301 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   41507784 |  126 | `}` |
|   32307097 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|   32307102 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2849919 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|   29457188 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   29457188 |  135 | `	pSet->nUsed--;` |
|   29457188 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   29457188 |  137 | `	return pData;` |
|   16154528 |  138 | `}` |
|  118103846 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|  118103851 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         55 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|  118103797 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  118103797 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   59058925 |  148 | `}` |
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
|    9375937 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    9375942 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    9375942 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    9375942 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    9375942 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    9375942 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    9375942 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    9375942 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    9375942 |  180 | `	pHash->nEntry = 0;` |
|    9375942 |  181 | `	pHash->apBucket = apNew;` |
|    9375942 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    9375942 |  183 | `	return SXRET_OK;` |
|    4688136 |  184 | `}` |
|    6935347 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|    6935352 |  193 | `	pEntry = pHash->pList;` |
|    7514787 |  194 | `	for(;;){` |
|   15029254 |  195 | `		if( pHash->nEntry == 0 ){` |
|    6935352 |  196 | `			break;` |
|          - |  197 | `		}` |
|    8093907 |  198 | `		pNext = pEntry->pNext;` |
|    8093907 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    8093907 |  200 | `		pEntry = pNext;` |
|    8093907 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|    6935352 |  203 | `	if( pHash->apBucket ){` |
|    6935352 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|    3467836 |  205 | `	}` |
|    6935352 |  206 | `	pHash->apBucket = 0;` |
|    6935352 |  207 | `	pHash->nBucketSize = 0;` |
|    6935352 |  208 | `	pHash->pAllocator = 0;` |
|    6935352 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|  153183218 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|  153183223 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  153183223 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  131815787 |  218 | `	for(;;){` |
|  264036467 |  219 | `		if( pEntry == 0 ){` |
|   65019584 |  220 | `			break;` |
|          - |  221 | `		}` |
|  243096759 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   88168019 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   88163644 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|  110853249 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   65019584 |  229 | `	return 0;` |
|   76601578 |  230 | `}` |
|  165773735 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|  165773740 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|   12591020 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|  153182725 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  153182725 |  244 | `	if( pEntry == 0 ){` |
|   65019566 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   88163164 |  247 | `	return (SyHashEntry *)pEntry;` |
|   82896999 |  248 | `}` |
|    6755556 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|    6755561 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    6644536 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|    3323083 |  254 | `	}else{` |
|     111030 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|    6755561 |  257 | `	if( pEntry->pNextCollide ){` |
|       8450 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       4229 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    6755561 |  261 | `	if( pHash->pLast == pEntry ){` |
|    6731689 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|    3366817 |  263 | `	}` |
|    6755561 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    6755561 |  265 | `	pHash->nEntry--;` |
|    6755561 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|    6755561 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    6755561 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        498 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        503 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        503 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        485 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        485 |  288 | `	return rc;` |
|        254 |  289 | `}` |
|    6755076 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|    6755081 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|    6755081 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    6755081 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    7677438 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    7677443 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    7677443 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   48835142 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   48835147 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    7677161 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    7677161 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   41157991 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   41157991 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   41157991 |  328 | `	return (SyHashEntry *)pEntry;` |
|   24417576 |  329 | `}` |
|         10 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|          2 |  331 | `{` |
|          - |  332 | `	SyHashEntry_Pr *pEntry;` |
|          - |  333 | `	sxi32 rc;` |
|          - |  334 | `	sxu32 n;` |
|          - |  335 | `#if defined(UNTRUST)` |
|          - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|          - |  337 | `		return 0;` |
|          - |  338 | `	}` |
|          - |  339 | `#endif` |
|         12 |  340 | `	pEntry = pHash->pList;` |
|       4576 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4566 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4566 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4566 |  348 | `		pEntry = pEntry->pNext;` |
|       2284 |  349 | `	}` |
|         12 |  350 | `	return SXRET_OK;` |
|          7 |  351 | `}` |
|          - |  352 | `/*` |
|          - |  353 | ` * Like SyHashForEach but walks the entries from the tail (pLast) back to the` |
|          - |  354 | ` * head via pPrev. The frame's local-variable table is built with SyHashInsert` |
|          - |  355 | ` * (head-push), so its forward pList order is reverse-insertion (LIFO); walking` |
|          - |  356 | ` * it backward yields DECLARATION order, which is what php's get_defined_vars()` |
|          - |  357 | ` * reports. Kept as its own primitive so the shared head-push insert path — and` |
|          - |  358 | ` * the SyHashLastEntry()==pList head contract every RefObj install relies on —` |
|          - |  359 | ` * stays untouched.` |
|          - |  360 | ` */` |
|         58 |  361 | `PH7_PRIVATE sxi32 SyHashForEachReverse(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|          4 |  362 | `{` |
|          - |  363 | `	SyHashEntry_Pr *pEntry;` |
|          - |  364 | `	sxi32 rc;` |
|          - |  365 | `	sxu32 n;` |
|          - |  366 | `#if defined(UNTRUST)` |
|          - |  367 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|          - |  368 | `		return 0;` |
|          - |  369 | `	}` |
|          - |  370 | `#endif` |
|         62 |  371 | `	pEntry = pHash->pLast;` |
|        808 |  372 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  373 | `		/* Invoke the callback */` |
|        750 |  374 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|        750 |  375 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  376 | `			return rc;` |
|          - |  377 | `		}` |
|          - |  378 | `		/* Point to the previous entry */` |
|        750 |  379 | `		pEntry = pEntry->pPrev;` |
|        377 |  380 | `	}` |
|         62 |  381 | `	return SXRET_OK;` |
|         33 |  382 | `}` |
|     107360 |  383 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  384 | `{` |
|     107365 |  385 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  386 | `	SyHashEntry_Pr *pEntry;` |
|          - |  387 | `	SyHashEntry_Pr **apNew;` |
|          - |  388 | `	sxu32 n,iBucket;` |
|          - |  389 |  |
|          - |  390 | `	/* Allocate a new larger table */` |
|     107365 |  391 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     107365 |  392 | `	if( apNew == 0 ){` |
|          - |  393 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  394 | `		return SXRET_OK;` |
|          - |  395 | `	}` |
|          - |  396 | `	/* Zero the new table */` |
|     107365 |  397 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  398 | `	/* Rehash all entries */` |
|   19884709 |  399 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   19777349 |  400 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  401 | `		/* Install in the new bucket */` |
|   19777349 |  402 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   19777349 |  403 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   19777349 |  404 | `		if( apNew[iBucket] != 0 ){` |
|    9566201 |  405 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4782764 |  406 | `		}` |
|   19777349 |  407 | `		apNew[iBucket] = pEntry;` |
|          - |  408 | `		/* Point to the next entry */` |
|   19777349 |  409 | `		pEntry = pEntry->pNext;` |
|    9888677 |  410 | `	}` |
|          - |  411 | `	/* Release the old table and reflect the change */` |
|     107365 |  412 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     107365 |  413 | `	pHash->apBucket = apNew;` |
|     107365 |  414 | `	pHash->nBucketSize = nNewSize;` |
|     107365 |  415 | `	return SXRET_OK;` |
|      53685 |  416 | `}` |
|   39028132 |  417 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  418 | `{` |
|   39028137 |  419 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  420 | `	/* Insert the entry in its corresponding bucket */` |
|   39028137 |  421 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   39028137 |  422 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   15047219 |  423 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    7523761 |  424 | `	}` |
|   39028137 |  425 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  426 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  427 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  428 | `	 * callers that need a FIFO traversal. */` |
|   39028137 |  429 | `	if( bTail && pHash->pLast != 0 ){` |
|    8227787 |  430 | `		pHash->pLast->pNext = pEntry;` |
|    8227787 |  431 | `		pEntry->pPrev = pHash->pLast;` |
|    8227787 |  432 | `		pHash->pLast = pEntry;` |
|    4113896 |  433 | `	}else{` |
|   30800355 |  434 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  435 | `	}` |
|   39028137 |  436 | `	if( pHash->nEntry == 0 ){` |
|          - |  437 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    4694112 |  438 | `		pHash->pCurrent = pHash->pList;` |
|    4694112 |  439 | `		pHash->pLast = pEntry;` |
|    2347216 |  440 | `	}` |
|   39028137 |  441 | `	pHash->nEntry++;` |
|   39028137 |  442 | `	return SXRET_OK;` |
|          5 |  443 | `}` |
|   39028132 |  444 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  445 | `{` |
|          - |  446 | `	SyHashEntry_Pr *pEntry;` |
|          - |  447 | `	sxi32 rc;` |
|          - |  448 | `#if defined(UNTRUST)` |
|          - |  449 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  450 | `		return SXERR_CORRUPT;` |
|          - |  451 | `	}` |
|          - |  452 | `#endif` |
|   39028137 |  453 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     107365 |  454 | `		rc = HashGrowTable(&(*pHash));` |
|     107365 |  455 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  456 | `			return rc;` |
|          - |  457 | `		}` |
|      53680 |  458 | `	}` |
|          - |  459 | `	/* Allocate a new hash entry */` |
|   39028137 |  460 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   39028137 |  461 | `	if( pEntry == 0 ){` |
|        ! 0 |  462 | `		return SXERR_MEM;` |
|          - |  463 | `	}` |
|          - |  464 | `	/* Zero the entry */` |
|   39028137 |  465 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   39028137 |  466 | `	pEntry->pHash = pHash;` |
|   39028137 |  467 | `	pEntry->pKey = pKey;` |
|   39028137 |  468 | `	pEntry->nKeyLen = nKeyLen;` |
|   39028137 |  469 | `	pEntry->pUserData = pUserData;` |
|   39028137 |  470 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  471 | `	/* Finally insert the entry in its corresponding bucket */` |
|   39028137 |  472 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   39028137 |  473 | `	return rc;` |
|   19515046 |  474 | `}` |
|   28977426 |  475 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  476 | `{` |
|   28977431 |  477 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  478 | `}` |
|          - |  479 | `/*` |
|          - |  480 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  481 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  482 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  483 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  484 | ` */` |
|   10050706 |  485 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  486 | `{` |
|   10050711 |  487 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          5 |  488 | `}` |
|    6798740 |  489 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  490 | `{` |
|          - |  491 | `#if defined(UNTRUST)` |
|          - |  492 | `	if( INVALID_HASH(pHash) ){` |
|          - |  493 | `		return 0;` |
|          - |  494 | `	}` |
|          - |  495 | `#endif` |
|          - |  496 | `	/* Last inserted entry */` |
|    6798745 |  497 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  498 | `}` |
|          - |  499 |  |
