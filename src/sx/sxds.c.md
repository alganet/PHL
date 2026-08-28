# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 290/304 lines (95.39%)

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
|  132038302 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  132038307 |   16 | `	pSet->nSize = 0 ;` |
|  132038307 |   17 | `	pSet->nUsed = 0;` |
|  132038307 |   18 | `	pSet->nCursor = 0;` |
|  132038307 |   19 | `	pSet->eSize = ElemSize;` |
|  132038307 |   20 | `	pSet->pAllocator = pAllocator;` |
|  132038307 |   21 | `	pSet->pBase =  0;` |
|  132038307 |   22 | `	pSet->pUserData = 0;` |
|  132038307 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  293850287 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  293850292 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   17373815 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   17373815 |   33 | `		if( pSet->nSize <= 0 ){` |
|   14904957 |   34 | `			pSet->nSize = 4;` |
|    7452476 |   35 | `		}` |
|   17373815 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   17373815 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   17373815 |   40 | `		pSet->pBase = pNew;` |
|   17373815 |   41 | `		pSet->nSize <<= 1;` |
|    8686905 |   42 | `	}` |
|  293850292 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2181205188 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  293850292 |   45 | `	pSet->nUsed++;` |
|  293850292 |   46 | `	return SXRET_OK;` |
|  146925191 |   47 | `}` |
|   14274300 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   14274305 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   14274305 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   14274305 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   14274305 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   14274305 |   60 | `	pSet->nSize = nItem;` |
|   14274305 |   61 | `	return SXRET_OK;` |
|    7137155 |   62 | `}` |
|   20869863 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   20869868 |   65 | `	pSet->nUsed   = 0;` |
|   20869868 |   66 | `	pSet->nCursor = 0;` |
|   20869868 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69334 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69339 |   71 | `	pSet->nCursor = 0;` |
|      69339 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73576 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73581 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29845 |   79 | `		pSet->nCursor = 0;` |
|      29845 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43741 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43741 |   83 | `	if( ppEntry ){` |
|      43741 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21868 |   85 | `	}` |
|      43741 |   86 | `	pSet->nCursor++;` |
|      43741 |   87 | `	return SXRET_OK;` |
|      36793 |   88 | `}` |
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
|    2424424 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2424429 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1179 |  103 | `		pSet->nUsed = nNewSize;` |
|        587 |  104 | `	}` |
|    2424429 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   45368996 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   45369001 |  109 | `	sxi32 rc = SXRET_OK;` |
|   45369001 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   24019155 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   12009575 |  112 | `	}` |
|   45369001 |  113 | `	pSet->pBase = 0;` |
|   45369001 |  114 | `	pSet->nUsed = 0;` |
|   45369001 |  115 | `	pSet->nCursor = 0;` |
|   45369001 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   52788026 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   52788031 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15653 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   52772383 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   52772383 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   26394018 |  126 | `}` |
|    7557060 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    7557065 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2216747 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    5340323 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    5340323 |  135 | `	pSet->nUsed--;` |
|    5340323 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    5340323 |  137 | `	return pData;` |
|    3778535 |  138 | `}` |
|   27118042 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   27118047 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   27118025 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   27118025 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   13559371 |  148 | `}` |
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
|    1670180 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1670185 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1670185 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1670185 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1670185 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1670185 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1670185 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1670185 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1670185 |  180 | `	pHash->nEntry = 0;` |
|    1670185 |  181 | `	pHash->apBucket = apNew;` |
|    1670185 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1670185 |  183 | `	return SXRET_OK;` |
|     835095 |  184 | `}` |
|     364496 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     364501 |  193 | `	pEntry = pHash->pList;` |
|     194682 |  194 | `	for(;;){` |
|     389369 |  195 | `		if( pHash->nEntry == 0 ){` |
|     364501 |  196 | `			break;` |
|          - |  197 | `		}` |
|      24873 |  198 | `		pNext = pEntry->pNext;` |
|      24873 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      24873 |  200 | `		pEntry = pNext;` |
|      24873 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     364501 |  203 | `	if( pHash->apBucket ){` |
|     364501 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     182248 |  205 | `	}` |
|     364501 |  206 | `	pHash->apBucket = 0;` |
|     364501 |  207 | `	pHash->nBucketSize = 0;` |
|     364501 |  208 | `	pHash->pAllocator = 0;` |
|     364501 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   56856037 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   56856042 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   56856042 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   52942077 |  218 | `	for(;;){` |
|  105770571 |  219 | `		if( pEntry == 0 ){` |
|   21223122 |  220 | `			break;` |
|          - |  221 | `		}` |
|  102363707 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   35633016 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   35632925 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   48914534 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   21223122 |  229 | `	return 0;` |
|   28428535 |  230 | `}` |
|   62558451 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   62558456 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    5702741 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   56855720 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   56855720 |  244 | `	if( pEntry == 0 ){` |
|   21223122 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   35632603 |  247 | `	return (SyHashEntry *)pEntry;` |
|   31279742 |  248 | `}` |
|     229010 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     229015 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     184609 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      92307 |  254 | `	}else{` |
|      44411 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     229015 |  257 | `	if( pEntry->pNextCollide ){` |
|       4326 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2162 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     229015 |  261 | `	if( pHash->pLast == pEntry ){` |
|     222029 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     111012 |  263 | `	}` |
|     229015 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     229015 |  265 | `	pHash->nEntry--;` |
|     229015 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|        ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        ! 0 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     229015 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     229015 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        322 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        327 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        327 |  284 | `	if( pEntry == 0 ){` |
|        ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        327 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        327 |  288 | `	return rc;` |
|        166 |  289 | `}` |
|     228688 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     228693 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     228693 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     228693 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2748672 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2748677 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2748677 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   20516604 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   20516609 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2748411 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2748411 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   17768203 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   17768203 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   17768203 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10258307 |  329 | `}` |
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
|       3681 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3671 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3671 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3671 |  348 | `		pEntry = pEntry->pNext;` |
|       1836 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      93628 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      93633 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      93633 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      93633 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      93633 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14746689 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14653061 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14653061 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14653061 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14653061 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    7019875 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3509813 |  375 | `		}` |
|   14653061 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14653061 |  378 | `		pEntry = pEntry->pNext;` |
|    7326533 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      93633 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      93633 |  382 | `	pHash->apBucket = apNew;` |
|      93633 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      93633 |  384 | `	return SXRET_OK;` |
|      46819 |  385 | `}` |
|   16715130 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   16715135 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   16715135 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   16715135 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   10431308 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5215687 |  393 | `	}` |
|   16715135 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   16715135 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   16715083 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   16715135 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     910727 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     910727 |  408 | `		pHash->pLast = pEntry;` |
|     455361 |  409 | `	}` |
|   16715135 |  410 | `	pHash->nEntry++;` |
|   16715135 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   16715130 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   16715135 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      93633 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      93633 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      46814 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   16715135 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   16715135 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   16715135 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   16715135 |  435 | `	pEntry->pHash = pHash;` |
|   16715135 |  436 | `	pEntry->pKey = pKey;` |
|   16715135 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   16715135 |  438 | `	pEntry->pUserData = pUserData;` |
|   16715135 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   16715135 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   16715135 |  442 | `	return rc;` |
|    8357570 |  443 | `}` |
|   16715002 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   16715007 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     269766 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     269771 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
