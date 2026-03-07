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
|  10271030 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  10271032 |   16 | `	pSet->nSize = 0 ;` |
|  10271032 |   17 | `	pSet->nUsed = 0;` |
|  10271032 |   18 | `	pSet->nCursor = 0;` |
|  10271032 |   19 | `	pSet->eSize = ElemSize;` |
|  10271032 |   20 | `	pSet->pAllocator = pAllocator;` |
|  10271032 |   21 | `	pSet->pBase =  0;` |
|  10271032 |   22 | `	pSet->pUserData = 0;` |
|  10271032 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  16209324 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  16209326 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3312640 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3312640 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3249238 |   34 | `			pSet->nSize = 4;` |
|   1624618 |   35 | `		}` |
|   3312640 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3312640 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3312640 |   40 | `		pSet->pBase = pNew;` |
|   3312640 |   41 | `		pSet->nSize <<= 1;` |
|   1656319 |   42 | `	}` |
|  16209326 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 121947306 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  16209326 |   45 | `	pSet->nUsed++;` |
|  16209326 |   46 | `	return SXRET_OK;` |
|   8104686 |   47 |  |
|    453812 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    453814 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    453814 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    453814 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    453814 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    453814 |   60 | `	pSet->nSize = nItem;` |
|    453814 |   61 | `	return SXRET_OK;` |
|    226908 |   62 |  |
|    889020 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|    889022 |   65 | `	pSet->nUsed   = 0;` |
|    889022 |   66 | `	pSet->nCursor = 0;` |
|    889022 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     35968 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     35970 |   71 | `	pSet->nCursor = 0;` |
|     35970 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     39468 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     39470 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     14454 |   79 | `		pSet->nCursor = 0;` |
|     14454 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     25018 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     25018 |   83 | `	if( ppEntry ){` |
|     25018 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     12508 |   85 | `	}` |
|     25018 |   86 | `	pSet->nCursor++;` |
|     25018 |   87 | `	return SXRET_OK;` |
|     19736 |   88 |  |
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
|     57524 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     57526 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     57526 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   6876176 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   6876178 |  109 | `	sxi32 rc = SXRET_OK;` |
|   6876178 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3533084 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1766541 |  112 | `	}` |
|   6876178 |  113 | `	pSet->pBase = 0;` |
|   6876178 |  114 | `	pSet->nUsed = 0;` |
|   6876178 |  115 | `	pSet->nCursor = 0;` |
|   6876178 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3348310 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3348312 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3348222 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3348222 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1674157 |  126 |  |
|   2985460 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   2985462 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2123836 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    861628 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    861628 |  135 | `	pSet->nUsed--;` |
|    861628 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    861628 |  137 | `	return pData;` |
|   1492732 |  138 |  |
|   8758906 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   8758908 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   8758908 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   8758908 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4379705 |  148 |  |
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
|     81636 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     81638 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     81638 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     81638 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     81638 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     81638 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     81638 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     81638 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     81638 |  180 | `	pHash->nEntry = 0;` |
|     81638 |  181 | `	pHash->apBucket = apNew;` |
|     81638 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     81638 |  183 | `	return SXRET_OK;` |
|     40820 |  184 |  |
|     10286 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     10288 |  193 | `	pEntry = pHash->pList;` |
|      6079 |  194 | `	for(;;){` |
|     12160 |  195 | `		if( pHash->nEntry == 0 ){` |
|     10288 |  196 | `			break;` |
|         - |  197 | `		}` |
|      1874 |  198 | `		pNext = pEntry->pNext;` |
|      1874 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      1874 |  200 | `		pEntry = pNext;` |
|      1874 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     10288 |  203 | `	if( pHash->apBucket ){` |
|     10288 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      5143 |  205 | `	}` |
|     10288 |  206 | `	pHash->apBucket = 0;` |
|     10288 |  207 | `	pHash->nBucketSize = 0;` |
|     10288 |  208 | `	pHash->pAllocator = 0;` |
|     10288 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   8250012 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   8250014 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   8250014 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   7121186 |  218 | `	for(;;){` |
|  14267742 |  219 | `		if( pEntry == 0 ){` |
|   4473138 |  220 | `			break;` |
|         - |  221 | `		}` |
|  11682914 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   3776880 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   3776878 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   6017730 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   4473138 |  229 | `	return 0;` |
|   4125272 |  230 |  |
|   8296058 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   8296060 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     46054 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   8250008 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   8250008 |  244 | `	if( pEntry == 0 ){` |
|   4473138 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   3776872 |  247 | `	return (SyHashEntry *)pEntry;` |
|   4148295 |  248 |  |
|     65850 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     65852 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     49314 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     24658 |  254 | `	}else{` |
|     16540 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     65852 |  257 | `	if( pEntry->pNextCollide ){` |
|      3949 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      1974 |  259 | `	}` |
|     65852 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     65852 |  261 | `	pHash->nEntry--;` |
|     65852 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     65852 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     65852 |  268 | `	return rc;` |
|         2 |  269 |  |
|         6 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         1 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|         7 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|         7 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|         7 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|         7 |  284 | `	return rc;` |
|         4 |  285 |  |
|     65844 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     65846 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     65846 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     65846 |  296 | `	return rc;` |
|         2 |  297 |  |
|    118600 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    118602 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    118602 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    825218 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    825220 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    118168 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    118168 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    707054 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    707054 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    707054 |  324 | `	return (SyHashEntry *)pEntry;` |
|    412611 |  325 |  |
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
|      1579 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1569 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1569 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1569 |  344 | `		pEntry = pEntry->pNext;` |
|       785 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     11628 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     11630 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     11630 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     11630 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     11630 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1593614 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1581986 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1581986 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1581986 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1581986 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    759676 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    379838 |  371 | `		}` |
|   1581986 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1581986 |  374 | `		pEntry = pEntry->pNext;` |
|    790994 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     11630 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     11630 |  378 | `	pHash->apBucket = apNew;` |
|     11630 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     11630 |  380 | `	return SXRET_OK;` |
|      5816 |  381 |  |
|   1432778 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1432780 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1432780 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1432780 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    956281 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    478127 |  389 | `	}` |
|   1432780 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1432780 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1432780 |  393 | `	if( pHash->nEntry == 0 ){` |
|     58550 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     29274 |  395 | `	}` |
|   1432780 |  396 | `	pHash->nEntry++;` |
|   1432780 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1432778 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1432780 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     11630 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     11630 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      5814 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1432780 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1432780 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1432780 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1432780 |  421 | `	pEntry->pHash = pHash;` |
|   1432780 |  422 | `	pEntry->pKey = pKey;` |
|   1432780 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1432780 |  424 | `	pEntry->pUserData = pUserData;` |
|   1432780 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1432780 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1432780 |  428 | `	return rc;` |
|    716391 |  429 |  |
|     80498 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     80500 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
