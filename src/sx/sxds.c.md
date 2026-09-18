# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 286/304 lines (94.08%)

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
|  183723502 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  183723507 |   16 | `	pSet->nSize = 0 ;` |
|  183723507 |   17 | `	pSet->nUsed = 0;` |
|  183723507 |   18 | `	pSet->nCursor = 0;` |
|  183723507 |   19 | `	pSet->eSize = ElemSize;` |
|  183723507 |   20 | `	pSet->pAllocator = pAllocator;` |
|  183723507 |   21 | `	pSet->pBase =  0;` |
|  183723507 |   22 | `	pSet->pUserData = 0;` |
|  183723507 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  416276287 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  416276292 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   24088376 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   24088376 |   33 | `		if( pSet->nSize <= 0 ){` |
|   20535258 |   34 | `			pSet->nSize = 4;` |
|   10268571 |   35 | `		}` |
|   24088376 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   24088376 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   24088376 |   40 | `		pSet->pBase = pNew;` |
|   24088376 |   41 | `		pSet->nSize <<= 1;` |
|   12045130 |   42 | `	}` |
|  416276292 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3286095970 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  416276292 |   45 | `	pSet->nUsed++;` |
|  416276292 |   46 | `	return SXRET_OK;` |
|  208141354 |   47 | `}` |
|   20682976 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   20682981 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   20682981 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   20682981 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   20682981 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   20682981 |   60 | `	pSet->nSize = nItem;` |
|   20682981 |   61 | `	return SXRET_OK;` |
|   10341493 |   62 | `}` |
|   30635070 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   30635075 |   65 | `	pSet->nUsed   = 0;` |
|   30635075 |   66 | `	pSet->nCursor = 0;` |
|   30635075 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69210 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69215 |   71 | `	pSet->nCursor = 0;` |
|      69215 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      69464 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      69469 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30175 |   79 | `		pSet->nCursor = 0;` |
|      30175 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      39299 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      39299 |   83 | `	if( ppEntry ){` |
|      39299 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      19647 |   85 | `	}` |
|      39299 |   86 | `	pSet->nCursor++;` |
|      39299 |   87 | `	return SXRET_OK;` |
|      34737 |   88 | `}` |
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
|    3268056 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3268061 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1253 |  103 | `		pSet->nUsed = nNewSize;` |
|        624 |  104 | `	}` |
|    3268061 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   62892102 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   62892107 |  109 | `	sxi32 rc = SXRET_OK;` |
|   62892107 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   34043176 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   17022530 |  112 | `	}` |
|   62892107 |  113 | `	pSet->pBase = 0;` |
|   62892107 |  114 | `	pSet->nUsed = 0;` |
|   62892107 |  115 | `	pSet->nCursor = 0;` |
|   62892107 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   73969824 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   73969829 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       3993 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   73965841 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   73965841 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   36984917 |  126 | `}` |
|    9843283 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9843288 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2237241 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7606052 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7606052 |  135 | `	pSet->nUsed--;` |
|    7606052 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7606052 |  137 | `	return pData;` |
|    4922276 |  138 | `}` |
|   37233475 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   37233480 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   37233428 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   37233428 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   18620906 |  148 | `}` |
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
|    2176674 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    2176679 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2176679 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    2176679 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2176679 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    2176679 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    2176679 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    2176679 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    2176679 |  180 | `	pHash->nEntry = 0;` |
|    2176679 |  181 | `	pHash->apBucket = apNew;` |
|    2176679 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    2176679 |  183 | `	return SXRET_OK;` |
|    1088447 |  184 | `}` |
|     519904 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     519909 |  193 | `	pEntry = pHash->pList;` |
|     275614 |  194 | `	for(;;){` |
|     551023 |  195 | `		if( pHash->nEntry == 0 ){` |
|     519909 |  196 | `			break;` |
|          - |  197 | `		}` |
|      31119 |  198 | `		pNext = pEntry->pNext;` |
|      31119 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      31119 |  200 | `		pEntry = pNext;` |
|      31119 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     519909 |  203 | `	if( pHash->apBucket ){` |
|     519909 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     260057 |  205 | `	}` |
|     519909 |  206 | `	pHash->apBucket = 0;` |
|     519909 |  207 | `	pHash->nBucketSize = 0;` |
|     519909 |  208 | `	pHash->pAllocator = 0;` |
|     519909 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   79235552 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   79235557 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   79235557 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   71265667 |  218 | `	for(;;){` |
|  142180142 |  219 | `		if( pEntry == 0 ){` |
|   31490121 |  220 | `			break;` |
|          - |  221 | `		}` |
|  134562152 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   47749424 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   47745441 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   62944590 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   31490121 |  229 | `	return 0;` |
|   39624013 |  230 | `}` |
|   89113316 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   89113321 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    9878231 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   79235095 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   79235095 |  244 | `	if( pEntry == 0 ){` |
|   31490103 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   47744997 |  247 | `	return (SyHashEntry *)pEntry;` |
|   44563000 |  248 | `}` |
|     516494 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     516499 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     420553 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     210804 |  254 | `	}else{` |
|      95951 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     516499 |  257 | `	if( pEntry->pNextCollide ){` |
|       4393 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2200 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     516499 |  261 | `	if( pHash->pLast == pEntry ){` |
|     506943 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     254099 |  263 | `	}` |
|     516499 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     516499 |  265 | `	pHash->nEntry--;` |
|     516499 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     516499 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     516499 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        462 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        467 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        467 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        449 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        449 |  288 | `	return rc;` |
|        236 |  289 | `}` |
|     516050 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     516055 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     516055 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     516055 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3408036 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3408041 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3408041 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   26251160 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   26251165 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3407775 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3407775 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   22843395 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   22843395 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   22843395 |  328 | `	return (SyHashEntry *)pEntry;` |
|   13125585 |  329 | `}` |
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
|       4857 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4843 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4843 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4843 |  348 | `		pEntry = pEntry->pNext;` |
|       2422 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|     100088 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100093 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100093 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100093 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100093 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18520285 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18420197 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18420197 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18420197 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18420197 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8890696 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4445070 |  375 | `		}` |
|   18420197 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18420197 |  378 | `		pEntry = pEntry->pNext;` |
|    9210101 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100093 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100093 |  382 | `	pHash->apBucket = apNew;` |
|     100093 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100093 |  384 | `	return SXRET_OK;` |
|      50049 |  385 | `}` |
|   22467104 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   22467109 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   22467109 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   22467109 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   14103011 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    7051631 |  393 | `	}` |
|   22467109 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   22467109 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|     876993 |  399 | `		pHash->pLast->pNext = pEntry;` |
|     876993 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|     876993 |  401 | `		pHash->pLast = pEntry;` |
|     438499 |  402 | `	}else{` |
|   21590121 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   22467109 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1209213 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1209213 |  408 | `		pHash->pLast = pEntry;` |
|     604709 |  409 | `	}` |
|   22467109 |  410 | `	pHash->nEntry++;` |
|   22467109 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   22467104 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   22467109 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100093 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100093 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50044 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   22467109 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   22467109 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   22467109 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   22467109 |  435 | `	pEntry->pHash = pHash;` |
|   22467109 |  436 | `	pEntry->pKey = pKey;` |
|   22467109 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   22467109 |  438 | `	pEntry->pUserData = pUserData;` |
|   22467109 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   22467109 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   22467109 |  442 | `	return rc;` |
|   11234187 |  443 | `}` |
|   21329754 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   21329759 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|    1137350 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  455 | `{` |
|    1137355 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          5 |  457 | `}` |
|     556236 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     556241 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
