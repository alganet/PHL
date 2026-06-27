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
|  19251722 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 |  |
|  19251727 |   16 | `	pSet->nSize = 0 ;` |
|  19251727 |   17 | `	pSet->nUsed = 0;` |
|  19251727 |   18 | `	pSet->nCursor = 0;` |
|  19251727 |   19 | `	pSet->eSize = ElemSize;` |
|  19251727 |   20 | `	pSet->pAllocator = pAllocator;` |
|  19251727 |   21 | `	pSet->pBase =  0;` |
|  19251727 |   22 | `	pSet->pUserData = 0;` |
|  19251727 |   23 | `	return SXRET_OK;` |
|         5 |   24 |  |
|  31670013 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  31670018 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4567403 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4567403 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4409233 |   34 | `			pSet->nSize = 4;` |
|   2204614 |   35 | `		}` |
|   4567403 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4567403 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4567403 |   40 | `		pSet->pBase = pNew;` |
|   4567403 |   41 | `		pSet->nSize <<= 1;` |
|   2283699 |   42 | `	}` |
|  31670018 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 236804386 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  31670018 |   45 | `	pSet->nUsed++;` |
|  31670018 |   46 | `	return SXRET_OK;` |
|  15835055 |   47 |  |
|   1307490 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 |  |
|   1307495 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1307495 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1307495 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1307495 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1307495 |   60 | `	pSet->nSize = nItem;` |
|   1307495 |   61 | `	return SXRET_OK;` |
|    653750 |   62 |  |
|   1804209 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 |  |
|   1804214 |   65 | `	pSet->nUsed   = 0;` |
|   1804214 |   66 | `	pSet->nCursor = 0;` |
|   1804214 |   67 | `	return SXRET_OK;` |
|         5 |   68 |  |
|     57456 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 |  |
|     57461 |   71 | `	pSet->nCursor = 0;` |
|     57461 |   72 | `	return SXRET_OK;` |
|         5 |   73 |  |
|     61662 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     61667 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     23797 |   79 | `		pSet->nCursor = 0;` |
|     23797 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     37875 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     37875 |   83 | `	if( ppEntry ){` |
|     37875 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     18935 |   85 | `	}` |
|     37875 |   86 | `	pSet->nCursor++;` |
|     37875 |   87 | `	return SXRET_OK;` |
|     30836 |   88 |  |
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
|    221886 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 |  |
|    221891 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       141 |  103 | `		pSet->nUsed = nNewSize;` |
|        68 |  104 | `	}` |
|    221891 |  105 | `	return SXRET_OK;` |
|         5 |  106 |  |
|  10002864 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 |  |
|  10002869 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10002869 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5002249 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2501122 |  112 | `	}` |
|  10002869 |  113 | `	pSet->pBase = 0;` |
|  10002869 |  114 | `	pSet->nUsed = 0;` |
|  10002869 |  115 | `	pSet->nCursor = 0;` |
|  10002869 |  116 | `	return rc;` |
|         5 |  117 |  |
|   5766776 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5766781 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       131 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5766655 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5766655 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2883393 |  126 |  |
|   3586888 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3586893 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2181845 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1405053 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1405053 |  135 | `	pSet->nUsed--;` |
|   1405053 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1405053 |  137 | `	return pData;` |
|   1793449 |  138 |  |
|  13343171 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  13343176 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13343176 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13343176 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6671926 |  148 |  |
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
|    568122 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    568127 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    568127 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    568127 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    568127 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    568127 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    568127 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    568127 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    568127 |  180 | `	pHash->nEntry = 0;` |
|    568127 |  181 | `	pHash->apBucket = apNew;` |
|    568127 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    568127 |  183 | `	return SXRET_OK;` |
|    284066 |  184 |  |
|    103000 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    103005 |  193 | `	pEntry = pHash->pList;` |
|     55145 |  194 | `	for(;;){` |
|    110295 |  195 | `		if( pHash->nEntry == 0 ){` |
|    103005 |  196 | `			break;` |
|         - |  197 | `		}` |
|      7295 |  198 | `		pNext = pEntry->pNext;` |
|      7295 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      7295 |  200 | `		pEntry = pNext;` |
|      7295 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    103005 |  203 | `	if( pHash->apBucket ){` |
|    103005 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     51500 |  205 | `	}` |
|    103005 |  206 | `	pHash->apBucket = 0;` |
|    103005 |  207 | `	pHash->nBucketSize = 0;` |
|    103005 |  208 | `	pHash->pAllocator = 0;` |
|    103005 |  209 | `	return SXRET_OK;` |
|         5 |  210 |  |
|  17480878 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  17480883 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  17480883 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  15592848 |  218 | `	for(;;){` |
|  31231862 |  219 | `		if( pEntry == 0 ){` |
|   9278331 |  220 | `			break;` |
|         - |  221 | `		}` |
|  26054553 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   8202556 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   8202557 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  13750984 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   9278331 |  229 | `	return 0;` |
|   8740966 |  230 |  |
|  18320090 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  18320095 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    839419 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  17480681 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  17480681 |  244 | `	if( pEntry == 0 ){` |
|   9278331 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   8202355 |  247 | `	return (SyHashEntry *)pEntry;` |
|   9160572 |  248 |  |
|    123198 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    123203 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     94985 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     47495 |  254 | `	}else{` |
|     28223 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    123203 |  257 | `	if( pEntry->pNextCollide ){` |
|      5039 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2519 |  259 | `	}` |
|    123203 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    123203 |  261 | `	pHash->nEntry--;` |
|    123203 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    123203 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    123203 |  268 | `	return rc;` |
|         5 |  269 |  |
|       202 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       207 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       207 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       207 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       207 |  284 | `	return rc;` |
|       106 |  285 |  |
|    122996 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  287 |  |
|    123001 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    123001 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    123001 |  296 | `	return rc;` |
|         5 |  297 |  |
|   1152822 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|   1152827 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   1152827 |  306 | `	return SXRET_OK;` |
|         5 |  307 |  |
|   7285436 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   7285441 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1152375 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   1152375 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   6133071 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   6133071 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   6133071 |  324 | `	return (SyHashEntry *)pEntry;` |
|   3642723 |  325 |  |
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
|      1995 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1985 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1985 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1985 |  344 | `		pEntry = pEntry->pNext;` |
|       993 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     29604 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  349 |  |
|     29609 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     29609 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     29609 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     29609 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3762761 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3733157 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3733157 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3733157 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3733157 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1790907 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    895460 |  371 | `		}` |
|   3733157 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3733157 |  374 | `		pEntry = pEntry->pNext;` |
|   1866581 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     29609 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     29609 |  378 | `	pHash->apBucket = apNew;` |
|     29609 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     29609 |  380 | `	return SXRET_OK;` |
|     14807 |  381 |  |
|   4963500 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         5 |  383 |  |
|   4963505 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   4963505 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4963505 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2806056 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1403089 |  389 | `	}` |
|   4963505 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   4963505 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   4963505 |  393 | `	if( pHash->nEntry == 0 ){` |
|    311143 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    155569 |  395 | `	}` |
|   4963505 |  396 | `	pHash->nEntry++;` |
|   4963505 |  397 | `	return SXRET_OK;` |
|         5 |  398 |  |
|   4963500 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   4963505 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     29609 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     29609 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     14802 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   4963505 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4963505 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   4963505 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4963505 |  421 | `	pEntry->pHash = pHash;` |
|   4963505 |  422 | `	pEntry->pKey = pKey;` |
|   4963505 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   4963505 |  424 | `	pEntry->pUserData = pUserData;` |
|   4963505 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4963505 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   4963505 |  428 | `	return rc;` |
|   2481755 |  429 |  |
|    159734 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    159739 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  439 |  |
|         - |  440 |  |
