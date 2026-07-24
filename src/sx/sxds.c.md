# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 289/304 lines (95.07%)

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
|   81110162 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|   81110167 |   16 | `	pSet->nSize = 0 ;` |
|   81110167 |   17 | `	pSet->nUsed = 0;` |
|   81110167 |   18 | `	pSet->nCursor = 0;` |
|   81110167 |   19 | `	pSet->eSize = ElemSize;` |
|   81110167 |   20 | `	pSet->pAllocator = pAllocator;` |
|   81110167 |   21 | `	pSet->pBase =  0;` |
|   81110167 |   22 | `	pSet->pUserData = 0;` |
|   81110167 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  175154077 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  175154082 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   11778123 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   11778123 |   33 | `		if( pSet->nSize <= 0 ){` |
|   10404377 |   34 | `			pSet->nSize = 4;` |
|    5202186 |   35 | `		}` |
|   11778123 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   11778123 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   11778123 |   40 | `		pSet->pBase = pNew;` |
|   11778123 |   41 | `		pSet->nSize <<= 1;` |
|    5889059 |   42 | `	}` |
|  175154082 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 1290460450 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  175154082 |   45 | `	pSet->nUsed++;` |
|  175154082 |   46 | `	return SXRET_OK;` |
|   87577086 |   47 | `}` |
|    8677446 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|    8677451 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|    8677451 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|    8677451 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    8677451 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|    8677451 |   60 | `	pSet->nSize = nItem;` |
|    8677451 |   61 | `	return SXRET_OK;` |
|    4338728 |   62 | `}` |
|   13593709 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   13593714 |   65 | `	pSet->nUsed   = 0;` |
|   13593714 |   66 | `	pSet->nCursor = 0;` |
|   13593714 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      68484 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      68489 |   71 | `	pSet->nCursor = 0;` |
|      68489 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      72660 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      72665 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29463 |   79 | `		pSet->nCursor = 0;` |
|      29463 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43207 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43207 |   83 | `	if( ppEntry ){` |
|      43207 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21601 |   85 | `	}` |
|      43207 |   86 | `	pSet->nCursor++;` |
|      43207 |   87 | `	return SXRET_OK;` |
|      36335 |   88 | `}` |
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
|    1400076 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    1400081 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        683 |  103 | `		pSet->nUsed = nNewSize;` |
|        339 |  104 | `	}` |
|    1400081 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   30717992 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   30717997 |  109 | `	sxi32 rc = SXRET_OK;` |
|   30717997 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   16247769 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|    8123882 |  112 | `	}` |
|   30717997 |  113 | `	pSet->pBase = 0;` |
|   30717997 |  114 | `	pSet->nUsed = 0;` |
|   30717997 |  115 | `	pSet->nCursor = 0;` |
|   30717997 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   30603378 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   30603383 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        133 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   30603255 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   30603255 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   15301694 |  126 | `}` |
|    6268454 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    6268459 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2394175 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    3874289 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    3874289 |  135 | `	pSet->nUsed--;` |
|    3874289 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    3874289 |  137 | `	return pData;` |
|    3134232 |  138 | `}` |
|   21216273 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   21216278 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|        ! 0 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   21216278 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   21216278 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   10608392 |  148 | `}` |
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
|    1152876 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1152881 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1152881 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1152881 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1152881 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1152881 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1152881 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1152881 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1152881 |  180 | `	pHash->nEntry = 0;` |
|    1152881 |  181 | `	pHash->apBucket = apNew;` |
|    1152881 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1152881 |  183 | `	return SXRET_OK;` |
|     576443 |  184 | `}` |
|     306312 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     306317 |  193 | `	pEntry = pHash->pList;` |
|     161662 |  194 | `	for(;;){` |
|     323329 |  195 | `		if( pHash->nEntry == 0 ){` |
|     306317 |  196 | `			break;` |
|          - |  197 | `		}` |
|      17017 |  198 | `		pNext = pEntry->pNext;` |
|      17017 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      17017 |  200 | `		pEntry = pNext;` |
|      17017 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     306317 |  203 | `	if( pHash->apBucket ){` |
|     306317 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     153156 |  205 | `	}` |
|     306317 |  206 | `	pHash->apBucket = 0;` |
|     306317 |  207 | `	pHash->nBucketSize = 0;` |
|     306317 |  208 | `	pHash->pAllocator = 0;` |
|     306317 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   39747754 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   39747759 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   39747759 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   37242972 |  218 | `	for(;;){` |
|   74577550 |  219 | `		if( pEntry == 0 ){` |
|   15923451 |  220 | `			break;` |
|          - |  221 | `		}` |
|   70566022 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   23824346 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   23824313 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   34829796 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   15923451 |  229 | `	return 0;` |
|   19874392 |  230 | `}` |
|   43303976 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   43303981 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    3556509 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   39747477 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   39747477 |  244 | `	if( pEntry == 0 ){` |
|   15923451 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   23824031 |  247 | `	return (SyHashEntry *)pEntry;` |
|   21652503 |  248 | `}` |
|     205808 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     205813 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     163859 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      81932 |  254 | `	}else{` |
|      41959 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     205813 |  257 | `	if( pEntry->pNextCollide ){` |
|       3724 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       1861 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     205813 |  261 | `	if( pHash->pLast == pEntry ){` |
|     199131 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|      99563 |  263 | `	}` |
|     205813 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     205813 |  265 | `	pHash->nEntry--;` |
|     205813 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|        ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        ! 0 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     205813 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     205813 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        282 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        287 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        287 |  284 | `	if( pEntry == 0 ){` |
|        ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        287 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        287 |  288 | `	return rc;` |
|        146 |  289 | `}` |
|     205526 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     205531 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     205531 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     205531 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    1744592 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    1744597 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    1744597 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   13060330 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   13060335 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    1744335 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    1744335 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   11316005 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   11316005 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   11316005 |  328 | `	return (SyHashEntry *)pEntry;` |
|    6530170 |  329 | `}` |
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
|       2845 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       2835 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       2835 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       2835 |  348 | `		pEntry = pEntry->pNext;` |
|       1418 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      77532 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      77537 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      77537 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      77537 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      77537 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|    9231713 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|    9154181 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|    9154181 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|    9154181 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|    9154181 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    4405391 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    2202664 |  375 | `		}` |
|    9154181 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|    9154181 |  378 | `		pEntry = pEntry->pNext;` |
|    4577093 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      77537 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      77537 |  382 | `	pHash->apBucket = apNew;` |
|      77537 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      77537 |  384 | `	return SXRET_OK;` |
|      38771 |  385 | `}` |
|   11355918 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   11355923 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   11355923 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   11355923 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    7134156 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    3567224 |  393 | `	}` |
|   11355923 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   11355923 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   11355871 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   11355923 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     598713 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     598713 |  408 | `		pHash->pLast = pEntry;` |
|     299354 |  409 | `	}` |
|   11355923 |  410 | `	pHash->nEntry++;` |
|   11355923 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   11355918 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   11355923 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      77537 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      77537 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      38766 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   11355923 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   11355923 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   11355923 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   11355923 |  435 | `	pEntry->pHash = pHash;` |
|   11355923 |  436 | `	pEntry->pKey = pKey;` |
|   11355923 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   11355923 |  438 | `	pEntry->pUserData = pUserData;` |
|   11355923 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   11355923 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   11355923 |  442 | `	return rc;` |
|    5677964 |  443 | `}` |
|   11355790 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   11355795 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     246218 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     246223 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
