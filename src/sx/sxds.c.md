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
|  184109603 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  184109608 |   16 | `	pSet->nSize = 0 ;` |
|  184109608 |   17 | `	pSet->nUsed = 0;` |
|  184109608 |   18 | `	pSet->nCursor = 0;` |
|  184109608 |   19 | `	pSet->eSize = ElemSize;` |
|  184109608 |   20 | `	pSet->pAllocator = pAllocator;` |
|  184109608 |   21 | `	pSet->pBase =  0;` |
|  184109608 |   22 | `	pSet->pUserData = 0;` |
|  184109608 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  417192463 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  417192468 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   24107487 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   24107487 |   33 | `		if( pSet->nSize <= 0 ){` |
|   20549329 |   34 | `			pSet->nSize = 4;` |
|   10275584 |   35 | `		}` |
|   24107487 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   24107487 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   24107487 |   40 | `		pSet->pBase = pNew;` |
|   24107487 |   41 | `		pSet->nSize <<= 1;` |
|   12054663 |   42 | `	}` |
|  417192468 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3293760744 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  417192468 |   45 | `	pSet->nUsed++;` |
|  417192468 |   46 | `	return SXRET_OK;` |
|  208599366 |   47 | `}` |
|   20735458 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   20735463 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   20735463 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   20735463 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   20735463 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   20735463 |   60 | `	pSet->nSize = nItem;` |
|   20735463 |   61 | `	return SXRET_OK;` |
|   10367734 |   62 | `}` |
|   30704999 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   30705004 |   65 | `	pSet->nUsed   = 0;` |
|   30705004 |   66 | `	pSet->nCursor = 0;` |
|   30705004 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69082 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69087 |   71 | `	pSet->nCursor = 0;` |
|      69087 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      69340 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      69345 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30045 |   79 | `		pSet->nCursor = 0;` |
|      30045 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      39305 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      39305 |   83 | `	if( ppEntry ){` |
|      39305 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      19650 |   85 | `	}` |
|      39305 |   86 | `	pSet->nCursor++;` |
|      39305 |   87 | `	return SXRET_OK;` |
|      34675 |   88 | `}` |
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
|    3276402 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3276407 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1213 |  103 | `		pSet->nUsed = nNewSize;` |
|        604 |  104 | `	}` |
|    3276407 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   62975737 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   62975742 |  109 | `	sxi32 rc = SXRET_OK;` |
|   62975742 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   34091273 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   17046556 |  112 | `	}` |
|   62975742 |  113 | `	pSet->pBase = 0;` |
|   62975742 |  114 | `	pSet->nUsed = 0;` |
|   62975742 |  115 | `	pSet->nCursor = 0;` |
|   62975742 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   74149686 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   74149691 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       3999 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   74145697 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   74145697 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   37074848 |  126 | `}` |
|    9834979 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9834984 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2237577 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7597412 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7597412 |  135 | `	pSet->nUsed--;` |
|    7597412 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7597412 |  137 | `	return pData;` |
|    4918109 |  138 | `}` |
|   36992263 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   36992268 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   36992216 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   36992216 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   18500234 |  148 | `}` |
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
|    2176761 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    2176766 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2176766 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    2176766 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2176766 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    2176766 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    2176766 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    2176766 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    2176766 |  180 | `	pHash->nEntry = 0;` |
|    2176766 |  181 | `	pHash->apBucket = apNew;` |
|    2176766 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    2176766 |  183 | `	return SXRET_OK;` |
|    1088488 |  184 | `}` |
|     519757 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     519762 |  193 | `	pEntry = pHash->pList;` |
|     275332 |  194 | `	for(;;){` |
|     550464 |  195 | `		if( pHash->nEntry == 0 ){` |
|     519762 |  196 | `			break;` |
|          - |  197 | `		}` |
|      30707 |  198 | `		pNext = pEntry->pNext;` |
|      30707 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      30707 |  200 | `		pEntry = pNext;` |
|      30707 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     519762 |  203 | `	if( pHash->apBucket ){` |
|     519762 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     259981 |  205 | `	}` |
|     519762 |  206 | `	pHash->apBucket = 0;` |
|     519762 |  207 | `	pHash->nBucketSize = 0;` |
|     519762 |  208 | `	pHash->pAllocator = 0;` |
|     519762 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   74935635 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   74935640 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   74935640 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   67798076 |  218 | `	for(;;){` |
|  135530852 |  219 | `		if( pEntry == 0 ){` |
|   27217369 |  220 | `			break;` |
|          - |  221 | `		}` |
|  132172097 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   47722269 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   47718276 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   60595217 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   27217369 |  229 | `	return 0;` |
|   37473909 |  230 | `}` |
|   83914758 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   83914763 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    8979586 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   74935182 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   74935182 |  244 | `	if( pEntry == 0 ){` |
|   27217351 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   47717836 |  247 | `	return (SyHashEntry *)pEntry;` |
|   41963573 |  248 | `}` |
|     496824 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     496829 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     407897 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     204463 |  254 | `	}else{` |
|      88937 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     496829 |  257 | `	if( pEntry->pNextCollide ){` |
|       4288 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2142 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     496829 |  261 | `	if( pHash->pLast == pEntry ){` |
|     487325 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     244275 |  263 | `	}` |
|     496829 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     496829 |  265 | `	pHash->nEntry--;` |
|     496829 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     496829 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     496829 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        458 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        463 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        463 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        445 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        445 |  288 | `	return rc;` |
|        234 |  289 | `}` |
|     496384 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     496389 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     496389 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     496389 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3422710 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3422715 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3422715 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   26378690 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   26378695 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3422449 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3422449 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   22956251 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   22956251 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   22956251 |  328 | `	return (SyHashEntry *)pEntry;` |
|   13189350 |  329 | `}` |
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
|       4835 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4821 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4821 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4821 |  348 | `		pEntry = pEntry->pNext;` |
|       2411 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|     100464 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100469 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100469 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100469 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100469 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18603125 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18502661 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18502661 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18502661 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18502661 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8929494 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4464658 |  375 | `		}` |
|   18502661 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18502661 |  378 | `		pEntry = pEntry->pNext;` |
|    9251333 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100469 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100469 |  382 | `	pHash->apBucket = apNew;` |
|     100469 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100469 |  384 | `	return SXRET_OK;` |
|      50237 |  385 | `}` |
|   22525472 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   22525477 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   22525477 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   22525477 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   14150919 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    7075585 |  393 | `	}` |
|   22525477 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   22525477 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|     878735 |  399 | `		pHash->pLast->pNext = pEntry;` |
|     878735 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|     878735 |  401 | `		pHash->pLast = pEntry;` |
|     439370 |  402 | `	}else{` |
|   21646747 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   22525477 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1210494 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1210494 |  408 | `		pHash->pLast = pEntry;` |
|     605347 |  409 | `	}` |
|   22525477 |  410 | `	pHash->nEntry++;` |
|   22525477 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   22525472 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   22525477 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100469 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100469 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50232 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   22525477 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   22525477 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   22525477 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   22525477 |  435 | `	pEntry->pHash = pHash;` |
|   22525477 |  436 | `	pEntry->pKey = pKey;` |
|   22525477 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   22525477 |  438 | `	pEntry->pUserData = pUserData;` |
|   22525477 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   22525477 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   22525477 |  442 | `	return rc;` |
|   11263356 |  443 | `}` |
|   21385852 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   21385857 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|    1139620 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  455 | `{` |
|    1139625 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          5 |  457 | `}` |
|     536756 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     536761 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
