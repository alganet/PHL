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
|  156541280 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  156541285 |   16 | `	pSet->nSize = 0 ;` |
|  156541285 |   17 | `	pSet->nUsed = 0;` |
|  156541285 |   18 | `	pSet->nCursor = 0;` |
|  156541285 |   19 | `	pSet->eSize = ElemSize;` |
|  156541285 |   20 | `	pSet->pAllocator = pAllocator;` |
|  156541285 |   21 | `	pSet->pBase =  0;` |
|  156541285 |   22 | `	pSet->pUserData = 0;` |
|  156541285 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  356152349 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  356152354 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   20616527 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   20616527 |   33 | `		if( pSet->nSize <= 0 ){` |
|   17567939 |   34 | `			pSet->nSize = 4;` |
|    8783967 |   35 | `		}` |
|   20616527 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   20616527 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   20616527 |   40 | `		pSet->pBase = pNew;` |
|   20616527 |   41 | `		pSet->nSize <<= 1;` |
|   10308261 |   42 | `	}` |
|  356152354 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2809317434 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  356152354 |   45 | `	pSet->nUsed++;` |
|  356152354 |   46 | `	return SXRET_OK;` |
|  178076203 |   47 | `}` |
|   17505754 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   17505759 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   17505759 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   17505759 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   17505759 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   17505759 |   60 | `	pSet->nSize = nItem;` |
|   17505759 |   61 | `	return SXRET_OK;` |
|    8752882 |   62 | `}` |
|   26039137 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   26039142 |   65 | `	pSet->nUsed   = 0;` |
|   26039142 |   66 | `	pSet->nCursor = 0;` |
|   26039142 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69486 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69491 |   71 | `	pSet->nCursor = 0;` |
|      69491 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73630 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73635 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30073 |   79 | `		pSet->nCursor = 0;` |
|      30073 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43567 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43567 |   83 | `	if( ppEntry ){` |
|      43567 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21781 |   85 | `	}` |
|      43567 |   86 | `	pSet->nCursor++;` |
|      43567 |   87 | `	return SXRET_OK;` |
|      36820 |   88 | `}` |
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
|    2699938 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2699943 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1181 |  103 | `		pSet->nUsed = nNewSize;` |
|        588 |  104 | `	}` |
|    2699943 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   54021054 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   54021059 |  109 | `	sxi32 rc = SXRET_OK;` |
|   54021059 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   29266143 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   14633069 |  112 | `	}` |
|   54021059 |  113 | `	pSet->pBase = 0;` |
|   54021059 |  114 | `	pSet->nUsed = 0;` |
|   54021059 |  115 | `	pSet->nCursor = 0;` |
|   54021059 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   63442096 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   63442101 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15365 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   63426741 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   63426741 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   31721053 |  126 | `}` |
|    8845342 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    8845347 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2213653 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    6631699 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    6631699 |  135 | `	pSet->nUsed--;` |
|    6631699 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    6631699 |  137 | `	return pData;` |
|    4422676 |  138 | `}` |
|   32131253 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   32131258 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   32131236 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   32131236 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   16065746 |  148 | `}` |
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
|    1763132 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1763137 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1763137 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1763137 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1763137 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1763137 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1763137 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1763137 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1763137 |  180 | `	pHash->nEntry = 0;` |
|    1763137 |  181 | `	pHash->apBucket = apNew;` |
|    1763137 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1763137 |  183 | `	return SXRET_OK;` |
|     881571 |  184 | `}` |
|     408696 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     408701 |  193 | `	pEntry = pHash->pList;` |
|     217582 |  194 | `	for(;;){` |
|     435169 |  195 | `		if( pHash->nEntry == 0 ){` |
|     408701 |  196 | `			break;` |
|          - |  197 | `		}` |
|      26473 |  198 | `		pNext = pEntry->pNext;` |
|      26473 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      26473 |  200 | `		pEntry = pNext;` |
|      26473 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     408701 |  203 | `	if( pHash->apBucket ){` |
|     408701 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     204348 |  205 | `	}` |
|     408701 |  206 | `	pHash->apBucket = 0;` |
|     408701 |  207 | `	pHash->nBucketSize = 0;` |
|     408701 |  208 | `	pHash->pAllocator = 0;` |
|     408701 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   65612127 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   65612132 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   65612132 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   58995555 |  218 | `	for(;;){` |
|  117973986 |  219 | `		if( pEntry == 0 ){` |
|   23823712 |  220 | `			break;` |
|          - |  221 | `		}` |
|  115044423 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   41788570 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   41788425 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   52361859 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   23823712 |  229 | `	return 0;` |
|   32806352 |  230 | `}` |
|   72437521 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   72437526 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    6825751 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   65611780 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   65611780 |  244 | `	if( pEntry == 0 ){` |
|   23823694 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   41788091 |  247 | `	return (SyHashEntry *)pEntry;` |
|   36219049 |  248 | `}` |
|     424124 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     424129 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     348578 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     174291 |  254 | `	}else{` |
|      75556 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     424129 |  257 | `	if( pEntry->pNextCollide ){` |
|       4208 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2103 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     424129 |  261 | `	if( pHash->pLast == pEntry ){` |
|     416417 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     208206 |  263 | `	}` |
|     424129 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     424129 |  265 | `	pHash->nEntry--;` |
|     424129 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     424129 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     424129 |  272 | `	return rc;` |
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
|     423790 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     423795 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     423795 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     423795 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2822942 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2822947 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2822947 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   20976532 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   20976537 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2822681 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2822681 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18153861 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18153861 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18153861 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10488271 |  329 | `}` |
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
|       4009 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3999 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3999 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3999 |  348 | `		pEntry = pEntry->pNext;` |
|       2000 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      91674 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      91679 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      91679 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      91679 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      91679 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14391071 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14299397 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14299397 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14299397 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14299397 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    6896047 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3447937 |  375 | `		}` |
|   14299397 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14299397 |  378 | `		pEntry = pEntry->pNext;` |
|    7149701 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      91679 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      91679 |  382 | `	pHash->apBucket = apNew;` |
|      91679 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      91679 |  384 | `	return SXRET_OK;` |
|      45842 |  385 | `}` |
|   18080904 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   18080909 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   18080909 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   18080909 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11370882 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5685274 |  393 | `	}` |
|   18080909 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   18080909 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   18080857 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   18080909 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     971617 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     971617 |  408 | `		pHash->pLast = pEntry;` |
|     485806 |  409 | `	}` |
|   18080909 |  410 | `	pHash->nEntry++;` |
|   18080909 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   18080904 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   18080909 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      91679 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      91679 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45837 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   18080909 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   18080909 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   18080909 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   18080909 |  435 | `	pEntry->pHash = pHash;` |
|   18080909 |  436 | `	pEntry->pKey = pKey;` |
|   18080909 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   18080909 |  438 | `	pEntry->pUserData = pUserData;` |
|   18080909 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   18080909 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   18080909 |  442 | `	return rc;` |
|    9040457 |  443 | `}` |
|   18080772 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   18080777 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     463404 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     463409 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
