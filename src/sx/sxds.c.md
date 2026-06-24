# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 272/287 lines (94.77%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits | Line | Source |
| --------: | ---: | :--- |
|         - |    1 | `/**` |
|         - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|         - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |    5 | ` */` |
|         - |    6 | `#include "sxtypes.h"` |
|         - |    7 | `#include "sxmacros.h"` |
|         - |    8 | `#include "sxset.h"` |
|         - |    9 | `#include "sxmem.h"` |
|         - |   10 | `#include "sxhashtable.h"` |
|         - |   11 | `#include "sxhash.h"` |
|         - |   12 | `#include "sxstr.h"` |
|         - |   13 |  |
|  18717906 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 |  |
|  18717911 |   16 | `	pSet->nSize = 0 ;` |
|  18717911 |   17 | `	pSet->nUsed = 0;` |
|  18717911 |   18 | `	pSet->nCursor = 0;` |
|  18717911 |   19 | `	pSet->eSize = ElemSize;` |
|  18717911 |   20 | `	pSet->pAllocator = pAllocator;` |
|  18717911 |   21 | `	pSet->pBase =  0;` |
|  18717911 |   22 | `	pSet->pUserData = 0;` |
|  18717911 |   23 | `	return SXRET_OK;` |
|         5 |   24 |  |
|  30717645 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  30717650 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4500385 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4500385 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4348499 |   34 | `			pSet->nSize = 4;` |
|   2174247 |   35 | `		}` |
|   4500385 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4500385 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4500385 |   40 | `		pSet->pBase = pNew;` |
|   4500385 |   41 | `		pSet->nSize <<= 1;` |
|   2250190 |   42 | `	}` |
|  30717650 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 229672398 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  30717650 |   45 | `	pSet->nUsed++;` |
|  30717650 |   46 | `	return SXRET_OK;` |
|  15358870 |   47 |  |
|   1254268 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 |  |
|   1254273 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1254273 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1254273 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1254273 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1254273 |   60 | `	pSet->nSize = nItem;` |
|   1254273 |   61 | `	return SXRET_OK;` |
|    627139 |   62 |  |
|   1751405 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 |  |
|   1751410 |   65 | `	pSet->nUsed   = 0;` |
|   1751410 |   66 | `	pSet->nCursor = 0;` |
|   1751410 |   67 | `	return SXRET_OK;` |
|         5 |   68 |  |
|     56696 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 |  |
|     56701 |   71 | `	pSet->nCursor = 0;` |
|     56701 |   72 | `	return SXRET_OK;` |
|         5 |   73 |  |
|     60902 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     60907 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     23437 |   79 | `		pSet->nCursor = 0;` |
|     23437 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     37475 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     37475 |   83 | `	if( ppEntry ){` |
|     37475 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     18735 |   85 | `	}` |
|     37475 |   86 | `	pSet->nCursor++;` |
|     37475 |   87 | `	return SXRET_OK;` |
|     30456 |   88 |  |
|         - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         8 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|         1 |   91 |  |
|         - |   92 | `	register unsigned char *zSrc;` |
|         9 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         3 |   94 | `		return 0;` |
|         - |   95 | `	}` |
|         7 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|         7 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|         5 |   98 |  |
|         - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    212342 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 |  |
|    212347 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       118 |  103 | `		pSet->nUsed = nNewSize;` |
|        57 |  104 | `	}` |
|    212347 |  105 | `	return SXRET_OK;` |
|         5 |  106 |  |
|   9830004 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 |  |
|   9830009 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9830009 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4919753 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2459874 |  112 | `	}` |
|   9830009 |  113 | `	pSet->pBase = 0;` |
|   9830009 |  114 | `	pSet->nUsed = 0;` |
|   9830009 |  115 | `	pSet->nCursor = 0;` |
|   9830009 |  116 | `	return rc;` |
|         5 |  117 |  |
|   5614424 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5614429 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       131 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5614303 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5614303 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2807217 |  126 |  |
|   3559512 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3559517 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2177429 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1382093 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1382093 |  135 | `	pSet->nUsed--;` |
|   1382093 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1382093 |  137 | `	return pData;` |
|   1779761 |  138 |  |
|  13174824 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  13174829 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13174829 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13174829 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6587743 |  148 |  |
|         - |  149 | `/* Private hash entry */` |
|         - |  150 | `struct SyHashEntry_Pr` |
|         - |  151 |  |
|         - |  152 | `	const void *pKey; /* Hash key */` |
|         - |  153 | `	sxu32 nKeyLen;    /* Key length */` |
|         - |  154 | `	void *pUserData;  /* User private data */` |
|         - |  155 | `	/* Private fields */` |
|         - |  156 | `	sxu32 nHash;` |
|         - |  157 | `	SyHash *pHash;` |
|         - |  158 | `	SyHashEntry_Pr *pNext,*pPrev; /* Next and previous entry in the list */` |
|         - |  159 | `	SyHashEntry_Pr *pNextCollide,*pPrevCollide; /* Collision list */` |
|         - |  160 | `};` |
|         - |  161 | `#define INVALID_HASH(H) ((H)->apBucket == 0)` |
|    545142 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    545147 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    545147 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    545147 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    545147 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    545147 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    545147 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    545147 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    545147 |  180 | `	pHash->nEntry = 0;` |
|    545147 |  181 | `	pHash->apBucket = apNew;` |
|    545147 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    545147 |  183 | `	return SXRET_OK;` |
|    272576 |  184 |  |
|     99988 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     99993 |  193 | `	pEntry = pHash->pList;` |
|     53393 |  194 | `	for(;;){` |
|    106791 |  195 | `		if( pHash->nEntry == 0 ){` |
|     99993 |  196 | `			break;` |
|         - |  197 | `		}` |
|      6803 |  198 | `		pNext = pEntry->pNext;` |
|      6803 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      6803 |  200 | `		pEntry = pNext;` |
|      6803 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|     99993 |  203 | `	if( pHash->apBucket ){` |
|     99993 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     49994 |  205 | `	}` |
|     99993 |  206 | `	pHash->apBucket = 0;` |
|     99993 |  207 | `	pHash->nBucketSize = 0;` |
|     99993 |  208 | `	pHash->pAllocator = 0;` |
|     99993 |  209 | `	return SXRET_OK;` |
|         5 |  210 |  |
|  17003214 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  17003219 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  17003219 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  15191160 |  218 | `	for(;;){` |
|  30302485 |  219 | `		if( pEntry == 0 ){` |
|   9012897 |  220 | `			break;` |
|         - |  221 | `		}` |
|  25284501 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   7990326 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   7990327 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  13299271 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   9012897 |  229 | `	return 0;` |
|   8502122 |  230 |  |
|  17806988 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  17806993 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    803975 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  17003023 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  17003023 |  244 | `	if( pEntry == 0 ){` |
|   9012897 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   7990131 |  247 | `	return (SyHashEntry *)pEntry;` |
|   8904009 |  248 |  |
|    119996 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    120001 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     92297 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     46151 |  254 | `	}else{` |
|     27709 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    120001 |  257 | `	if( pEntry->pNextCollide ){` |
|      5041 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2520 |  259 | `	}` |
|    120001 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    120001 |  261 | `	pHash->nEntry--;` |
|    120001 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    120001 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    120001 |  268 | `	return rc;` |
|         5 |  269 |  |
|       196 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       201 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       201 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       201 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       201 |  284 | `	return rc;` |
|       103 |  285 |  |
|    119800 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  287 |  |
|    119805 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    119805 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    119805 |  296 | `	return rc;` |
|         5 |  297 |  |
|   1108614 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|   1108619 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   1108619 |  306 | `	return SXRET_OK;` |
|         5 |  307 |  |
|   7006782 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   7006787 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1108169 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   1108169 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   5898623 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   5898623 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   5898623 |  324 | `	return (SyHashEntry *)pEntry;` |
|   3503396 |  325 |  |
|        10 |  326 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         1 |  327 |  |
|         - |  328 | `	SyHashEntry_Pr *pEntry;` |
|         - |  329 | `	sxi32 rc;` |
|         - |  330 | `	sxu32 n;` |
|         - |  331 | `#if defined(UNTRUST)` |
|         - |  332 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  333 | `		return 0;` |
|         - |  334 | `	}` |
|         - |  335 | `#endif` |
|        11 |  336 | `	pEntry = pHash->pList;` |
|      1991 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1981 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1981 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1981 |  344 | `		pEntry = pEntry->pNext;` |
|       991 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     28280 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  349 |  |
|     28285 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     28285 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     28285 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     28285 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3593437 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3565157 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3565157 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3565157 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3565157 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1710120 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    855040 |  371 | `		}` |
|   3565157 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3565157 |  374 | `		pEntry = pEntry->pNext;` |
|   1782581 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     28285 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     28285 |  378 | `	pHash->apBucket = apNew;` |
|     28285 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     28285 |  380 | `	return SXRET_OK;` |
|     14145 |  381 |  |
|   4741870 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         5 |  383 |  |
|   4741875 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   4741875 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4741875 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2676433 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1338242 |  389 | `	}` |
|   4741875 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   4741875 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   4741875 |  393 | `	if( pHash->nEntry == 0 ){` |
|    298027 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    149011 |  395 | `	}` |
|   4741875 |  396 | `	pHash->nEntry++;` |
|   4741875 |  397 | `	return SXRET_OK;` |
|         5 |  398 |  |
|   4741870 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   4741875 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     28285 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     28285 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     14140 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   4741875 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4741875 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   4741875 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4741875 |  421 | `	pEntry->pHash = pHash;` |
|   4741875 |  422 | `	pEntry->pKey = pKey;` |
|   4741875 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   4741875 |  424 | `	pEntry->pUserData = pUserData;` |
|   4741875 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4741875 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   4741875 |  428 | `	return rc;` |
|   2370940 |  429 |  |
|    154824 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    154829 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  439 |  |
|         - |  440 |  |
