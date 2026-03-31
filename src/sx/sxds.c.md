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
|  12925864 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  12925866 |   16 | `	pSet->nSize = 0 ;` |
|  12925866 |   17 | `	pSet->nUsed = 0;` |
|  12925866 |   18 | `	pSet->nCursor = 0;` |
|  12925866 |   19 | `	pSet->eSize = ElemSize;` |
|  12925866 |   20 | `	pSet->pAllocator = pAllocator;` |
|  12925866 |   21 | `	pSet->pBase =  0;` |
|  12925866 |   22 | `	pSet->pUserData = 0;` |
|  12925866 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  21336638 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  21336640 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3714672 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3714672 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3610334 |   34 | `			pSet->nSize = 4;` |
|   1805166 |   35 | `		}` |
|   3714672 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3714672 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3714672 |   40 | `		pSet->pBase = pNew;` |
|   3714672 |   41 | `		pSet->nSize <<= 1;` |
|   1857335 |   42 | `	}` |
|  21336640 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 158523992 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  21336640 |   45 | `	pSet->nUsed++;` |
|  21336640 |   46 | `	return SXRET_OK;` |
|  10668343 |   47 |  |
|    717262 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    717264 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    717264 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    717264 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    717264 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    717264 |   60 | `	pSet->nSize = nItem;` |
|    717264 |   61 | `	return SXRET_OK;` |
|    358633 |   62 |  |
|   1191880 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1191882 |   65 | `	pSet->nUsed   = 0;` |
|   1191882 |   66 | `	pSet->nCursor = 0;` |
|   1191882 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     42254 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     42256 |   71 | `	pSet->nCursor = 0;` |
|     42256 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     46140 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     46142 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     17336 |   79 | `		pSet->nCursor = 0;` |
|     17336 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     28808 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     28808 |   83 | `	if( ppEntry ){` |
|     28808 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     14403 |   85 | `	}` |
|     28808 |   86 | `	pSet->nCursor++;` |
|     28808 |   87 | `	return SXRET_OK;` |
|     23072 |   88 |  |
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
|     88276 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     88278 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     88278 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   7860356 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   7860358 |  109 | `	sxi32 rc = SXRET_OK;` |
|   7860358 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4066228 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2033113 |  112 | `	}` |
|   7860358 |  113 | `	pSet->pBase = 0;` |
|   7860358 |  114 | `	pSet->nUsed = 0;` |
|   7860358 |  115 | `	pSet->nCursor = 0;` |
|   7860358 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4255128 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4255130 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4255040 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4255040 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2127566 |  126 |  |
|   3203918 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3203920 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2141504 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1062418 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1062418 |  135 | `	pSet->nUsed--;` |
|   1062418 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1062418 |  137 | `	return pData;` |
|   1601961 |  138 |  |
|  10098036 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  10098038 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  10098038 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  10098038 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5049238 |  148 |  |
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
|    151210 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    151212 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    151212 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    151212 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    151212 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    151212 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    151212 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    151212 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    151212 |  180 | `	pHash->nEntry = 0;` |
|    151212 |  181 | `	pHash->apBucket = apNew;` |
|    151212 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    151212 |  183 | `	return SXRET_OK;` |
|     75607 |  184 |  |
|     28682 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     28684 |  193 | `	pEntry = pHash->pList;` |
|     16044 |  194 | `	for(;;){` |
|     32090 |  195 | `		if( pHash->nEntry == 0 ){` |
|     28684 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3408 |  198 | `		pNext = pEntry->pNext;` |
|      3408 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3408 |  200 | `		pEntry = pNext;` |
|      3408 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     28684 |  203 | `	if( pHash->apBucket ){` |
|     28684 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     14341 |  205 | `	}` |
|     28684 |  206 | `	pHash->apBucket = 0;` |
|     28684 |  207 | `	pHash->nBucketSize = 0;` |
|     28684 |  208 | `	pHash->pAllocator = 0;` |
|     28684 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  10623216 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  10623218 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  10623218 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   9301568 |  218 | `	for(;;){` |
|  18667696 |  219 | `		if( pEntry == 0 ){` |
|   5843118 |  220 | `			break;` |
|         - |  221 | `		}` |
|  15214500 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   4780104 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   4780102 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   8044480 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   5843118 |  229 | `	return 0;` |
|   5311874 |  230 |  |
|  10712362 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  10712364 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     89158 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  10623208 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  10623208 |  244 | `	if( pEntry == 0 ){` |
|   5843118 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   4780092 |  247 | `	return (SyHashEntry *)pEntry;` |
|   5356447 |  248 |  |
|     82522 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     82524 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     62752 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     31377 |  254 | `	}else{` |
|     19774 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     82524 |  257 | `	if( pEntry->pNextCollide ){` |
|      4307 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2153 |  259 | `	}` |
|     82524 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     82524 |  261 | `	pHash->nEntry--;` |
|     82524 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     82524 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     82524 |  268 | `	return rc;` |
|         2 |  269 |  |
|        10 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|        12 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        12 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|        12 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        12 |  284 | `	return rc;` |
|         7 |  285 |  |
|     82512 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     82514 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     82514 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     82514 |  296 | `	return rc;` |
|         2 |  297 |  |
|    205170 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    205172 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    205172 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   1516184 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   1516186 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    204738 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    204738 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   1311450 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   1311450 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   1311450 |  324 | `	return (SyHashEntry *)pEntry;` |
|    758094 |  325 |  |
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
|      1619 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1609 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1609 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1609 |  344 | `		pEntry = pEntry->pNext;` |
|       805 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     18770 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     18772 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     18772 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     18772 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     18772 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2585908 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2567138 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2567138 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2567138 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2567138 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1232638 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    616316 |  371 | `		}` |
|   2567138 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2567138 |  374 | `		pEntry = pEntry->pNext;` |
|   1283570 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     18772 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     18772 |  378 | `	pHash->apBucket = apNew;` |
|     18772 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     18772 |  380 | `	return SXRET_OK;` |
|      9387 |  381 |  |
|   2340924 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2340926 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2340926 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2340926 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1564383 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    782226 |  389 | `	}` |
|   2340926 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2340926 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2340926 |  393 | `	if( pHash->nEntry == 0 ){` |
|     93892 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     46945 |  395 | `	}` |
|   2340926 |  396 | `	pHash->nEntry++;` |
|   2340926 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2340924 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2340926 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     18772 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     18772 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      9385 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2340926 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2340926 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2340926 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2340926 |  421 | `	pEntry->pHash = pHash;` |
|   2340926 |  422 | `	pEntry->pKey = pKey;` |
|   2340926 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2340926 |  424 | `	pEntry->pUserData = pUserData;` |
|   2340926 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2340926 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2340926 |  428 | `	return rc;` |
|   1170464 |  429 |  |
|    106506 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    106508 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
