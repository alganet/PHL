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
|   9826502 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|   9826504 |   16 | `	pSet->nSize = 0 ;` |
|   9826504 |   17 | `	pSet->nUsed = 0;` |
|   9826504 |   18 | `	pSet->nCursor = 0;` |
|   9826504 |   19 | `	pSet->eSize = ElemSize;` |
|   9826504 |   20 | `	pSet->pAllocator = pAllocator;` |
|   9826504 |   21 | `	pSet->pBase =  0;` |
|   9826504 |   22 | `	pSet->pUserData = 0;` |
|   9826504 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  15495006 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  15495008 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3220590 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3220590 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3161880 |   34 | `			pSet->nSize = 4;` |
|   1580939 |   35 | `		}` |
|   3220590 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3220590 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3220590 |   40 | `		pSet->pBase = pNew;` |
|   3220590 |   41 | `		pSet->nSize <<= 1;` |
|   1610294 |   42 | `	}` |
|  15495008 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 117178448 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  15495008 |   45 | `	pSet->nUsed++;` |
|  15495008 |   46 | `	return SXRET_OK;` |
|   7747527 |   47 |  |
|    420506 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    420508 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    420508 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    420508 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    420508 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    420508 |   60 | `	pSet->nSize = nItem;` |
|    420508 |   61 | `	return SXRET_OK;` |
|    210255 |   62 |  |
|    833784 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|    833786 |   65 | `	pSet->nUsed   = 0;` |
|    833786 |   66 | `	pSet->nCursor = 0;` |
|    833786 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     34034 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     34036 |   71 | `	pSet->nCursor = 0;` |
|     34036 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     37282 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     37284 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     13622 |   79 | `		pSet->nCursor = 0;` |
|     13622 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     23664 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     23664 |   83 | `	if( ppEntry ){` |
|     23664 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     11831 |   85 | `	}` |
|     23664 |   86 | `	pSet->nCursor++;` |
|     23664 |   87 | `	return SXRET_OK;` |
|     18643 |   88 |  |
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
|     53172 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     53174 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     53174 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   6673088 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   6673090 |  109 | `	sxi32 rc = SXRET_OK;` |
|   6673090 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3425738 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1712868 |  112 | `	}` |
|   6673090 |  113 | `	pSet->pBase = 0;` |
|   6673090 |  114 | `	pSet->nUsed = 0;` |
|   6673090 |  115 | `	pSet->nCursor = 0;` |
|   6673090 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3255816 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3255818 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3255728 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3255728 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1627910 |  126 |  |
|   2917066 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   2917068 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2120816 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    796254 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    796254 |  135 | `	pSet->nUsed--;` |
|    796254 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    796254 |  137 | `	return pData;` |
|   1458535 |  138 |  |
|   8317959 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   8317961 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   8317961 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   8317961 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4159181 |  148 |  |
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
|     75624 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     75626 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     75626 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     75626 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     75626 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     75626 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     75626 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     75626 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     75626 |  180 | `	pHash->nEntry = 0;` |
|     75626 |  181 | `	pHash->apBucket = apNew;` |
|     75626 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     75626 |  183 | `	return SXRET_OK;` |
|     37814 |  184 |  |
|      9650 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|      9652 |  193 | `	pEntry = pHash->pList;` |
|      5569 |  194 | `	for(;;){` |
|     11140 |  195 | `		if( pHash->nEntry == 0 ){` |
|      9652 |  196 | `			break;` |
|         - |  197 | `		}` |
|      1490 |  198 | `		pNext = pEntry->pNext;` |
|      1490 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      1490 |  200 | `		pEntry = pNext;` |
|      1490 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|      9652 |  203 | `	if( pHash->apBucket ){` |
|      9652 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      4825 |  205 | `	}` |
|      9652 |  206 | `	pHash->apBucket = 0;` |
|      9652 |  207 | `	pHash->nBucketSize = 0;` |
|      9652 |  208 | `	pHash->pAllocator = 0;` |
|      9652 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   7688122 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   7688124 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   7688124 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   6658685 |  218 | `	for(;;){` |
|  13296304 |  219 | `		if( pEntry == 0 ){` |
|   4160734 |  220 | `			break;` |
|         - |  221 | `		}` |
|  10899137 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   3527394 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   3527392 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   5608182 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   4160734 |  229 | `	return 0;` |
|   3844327 |  230 |  |
|   7730904 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   7730906 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     42790 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   7688118 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   7688118 |  244 | `	if( pEntry == 0 ){` |
|   4160734 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   3527386 |  247 | `	return (SyHashEntry *)pEntry;` |
|   3865718 |  248 |  |
|     61988 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     61990 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     46394 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     23198 |  254 | `	}else{` |
|     15598 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     61990 |  257 | `	if( pEntry->pNextCollide ){` |
|      3653 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      1826 |  259 | `	}` |
|     61990 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     61990 |  261 | `	pHash->nEntry--;` |
|     61990 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     61990 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     61990 |  268 | `	return rc;` |
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
|     61982 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     61984 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     61984 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     61984 |  296 | `	return rc;` |
|         2 |  297 |  |
|    110956 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    110958 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    110958 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    773664 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    773666 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    110524 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    110524 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    663144 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    663144 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    663144 |  324 | `	return (SyHashEntry *)pEntry;` |
|    386834 |  325 |  |
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
|     10604 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     10606 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     10606 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     10606 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     10606 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1451278 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1440674 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1440674 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1440674 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1440674 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    691846 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    345923 |  371 | `		}` |
|   1440674 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1440674 |  374 | `		pEntry = pEntry->pNext;` |
|    720338 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     10606 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     10606 |  378 | `	pHash->apBucket = apNew;` |
|     10606 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     10606 |  380 | `	return SXRET_OK;` |
|      5304 |  381 |  |
|   1311952 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1311954 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1311954 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1311954 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    873293 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    436655 |  389 | `	}` |
|   1311954 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1311954 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1311954 |  393 | `	if( pHash->nEntry == 0 ){` |
|     54200 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     27099 |  395 | `	}` |
|   1311954 |  396 | `	pHash->nEntry++;` |
|   1311954 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1311952 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1311954 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     10606 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     10606 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      5302 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1311954 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1311954 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1311954 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1311954 |  421 | `	pEntry->pHash = pHash;` |
|   1311954 |  422 | `	pEntry->pKey = pKey;` |
|   1311954 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1311954 |  424 | `	pEntry->pUserData = pUserData;` |
|   1311954 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1311954 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1311954 |  428 | `	return rc;` |
|    655978 |  429 |  |
|     75354 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     75356 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
