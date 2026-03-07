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
|  10192114 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  10192116 |   16 | `	pSet->nSize = 0 ;` |
|  10192116 |   17 | `	pSet->nUsed = 0;` |
|  10192116 |   18 | `	pSet->nCursor = 0;` |
|  10192116 |   19 | `	pSet->eSize = ElemSize;` |
|  10192116 |   20 | `	pSet->pAllocator = pAllocator;` |
|  10192116 |   21 | `	pSet->pBase =  0;` |
|  10192116 |   22 | `	pSet->pUserData = 0;` |
|  10192116 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  16080444 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  16080446 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3297030 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3297030 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3234498 |   34 | `			pSet->nSize = 4;` |
|   1617248 |   35 | `		}` |
|   3297030 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3297030 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3297030 |   40 | `		pSet->pBase = pNew;` |
|   3297030 |   41 | `		pSet->nSize <<= 1;` |
|   1648514 |   42 | `	}` |
|  16080446 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 121075518 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  16080446 |   45 | `	pSet->nUsed++;` |
|  16080446 |   46 | `	return SXRET_OK;` |
|   8040246 |   47 |  |
|    447634 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    447636 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    447636 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    447636 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    447636 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    447636 |   60 | `	pSet->nSize = nItem;` |
|    447636 |   61 | `	return SXRET_OK;` |
|    223819 |   62 |  |
|    879278 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|    879280 |   65 | `	pSet->nUsed   = 0;` |
|    879280 |   66 | `	pSet->nCursor = 0;` |
|    879280 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     35654 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     35656 |   71 | `	pSet->nCursor = 0;` |
|     35656 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     39112 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     39114 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     14318 |   79 | `		pSet->nCursor = 0;` |
|     14318 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     24798 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     24798 |   83 | `	if( ppEntry ){` |
|     24798 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     12398 |   85 | `	}` |
|     24798 |   86 | `	pSet->nCursor++;` |
|     24798 |   87 | `	return SXRET_OK;` |
|     19558 |   88 |  |
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
|     56708 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     56710 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     56710 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   6841282 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   6841284 |  109 | `	sxi32 rc = SXRET_OK;` |
|   6841284 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3514664 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1757331 |  112 | `	}` |
|   6841284 |  113 | `	pSet->pBase = 0;` |
|   6841284 |  114 | `	pSet->nUsed = 0;` |
|   6841284 |  115 | `	pSet->nCursor = 0;` |
|   6841284 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3331176 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3331178 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3331088 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3331088 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1665590 |  126 |  |
|   2974202 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   2974204 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2123284 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    850922 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    850922 |  135 | `	pSet->nUsed--;` |
|    850922 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    850922 |  137 | `	return pData;` |
|   1487103 |  138 |  |
|   8683753 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   8683755 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   8683755 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   8683755 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4342126 |  148 |  |
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
|     80514 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     80516 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     80516 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     80516 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     80516 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     80516 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     80516 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     80516 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     80516 |  180 | `	pHash->nEntry = 0;` |
|     80516 |  181 | `	pHash->apBucket = apNew;` |
|     80516 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     80516 |  183 | `	return SXRET_OK;` |
|     40259 |  184 |  |
|     10172 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     10174 |  193 | `	pEntry = pHash->pList;` |
|      5986 |  194 | `	for(;;){` |
|     11974 |  195 | `		if( pHash->nEntry == 0 ){` |
|     10174 |  196 | `			break;` |
|         - |  197 | `		}` |
|      1802 |  198 | `		pNext = pEntry->pNext;` |
|      1802 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      1802 |  200 | `		pEntry = pNext;` |
|      1802 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     10174 |  203 | `	if( pHash->apBucket ){` |
|     10174 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      5086 |  205 | `	}` |
|     10174 |  206 | `	pHash->apBucket = 0;` |
|     10174 |  207 | `	pHash->nBucketSize = 0;` |
|     10174 |  208 | `	pHash->pAllocator = 0;` |
|     10174 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   8149368 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   8149370 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   8149370 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   6996446 |  218 | `	for(;;){` |
|  13999070 |  219 | `		if( pEntry == 0 ){` |
|   4416934 |  220 | `			break;` |
|         - |  221 | `		}` |
|  11448226 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   3732440 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   3732438 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   5849702 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   4416934 |  229 | `	return 0;` |
|   4074950 |  230 |  |
|   8194808 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   8194810 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     45448 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   8149364 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   8149364 |  244 | `	if( pEntry == 0 ){` |
|   4416934 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   3732432 |  247 | `	return (SyHashEntry *)pEntry;` |
|   4097670 |  248 |  |
|     65202 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     65204 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     48818 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     24410 |  254 | `	}else{` |
|     16388 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     65204 |  257 | `	if( pEntry->pNextCollide ){` |
|      3923 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      1961 |  259 | `	}` |
|     65204 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     65204 |  261 | `	pHash->nEntry--;` |
|     65204 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     65204 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     65204 |  268 | `	return rc;` |
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
|     65196 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     65198 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     65198 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     65198 |  296 | `	return rc;` |
|         2 |  297 |  |
|    117208 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    117210 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    117210 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    815768 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    815770 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    116776 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    116776 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    698996 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    698996 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    698996 |  324 | `	return (SyHashEntry *)pEntry;` |
|    407886 |  325 |  |
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
|     11436 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     11438 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     11438 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     11438 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     11438 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1566926 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1555490 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1555490 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1555490 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1555490 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    746956 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    373477 |  371 | `		}` |
|   1555490 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1555490 |  374 | `		pEntry = pEntry->pNext;` |
|    777746 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     11438 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     11438 |  378 | `	pHash->apBucket = apNew;` |
|     11438 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     11438 |  380 | `	return SXRET_OK;` |
|      5720 |  381 |  |
|   1410206 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1410208 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1410208 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1410208 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    940809 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    470400 |  389 | `	}` |
|   1410208 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1410208 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1410208 |  393 | `	if( pHash->nEntry == 0 ){` |
|     57740 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     28869 |  395 | `	}` |
|   1410208 |  396 | `	pHash->nEntry++;` |
|   1410208 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1410206 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1410208 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     11438 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     11438 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      5718 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1410208 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1410208 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1410208 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1410208 |  421 | `	pEntry->pHash = pHash;` |
|   1410208 |  422 | `	pEntry->pKey = pKey;` |
|   1410208 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1410208 |  424 | `	pEntry->pUserData = pUserData;` |
|   1410208 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1410208 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1410208 |  428 | `	return rc;` |
|    705105 |  429 |  |
|     79610 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     79612 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
