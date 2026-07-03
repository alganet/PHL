# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 289/304 lines (95.07%)

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
|  20403586 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 | `{` |
|  20403591 |   16 | `	pSet->nSize = 0 ;` |
|  20403591 |   17 | `	pSet->nUsed = 0;` |
|  20403591 |   18 | `	pSet->nCursor = 0;` |
|  20403591 |   19 | `	pSet->eSize = ElemSize;` |
|  20403591 |   20 | `	pSet->pAllocator = pAllocator;` |
|  20403591 |   21 | `	pSet->pBase =  0;` |
|  20403591 |   22 | `	pSet->pUserData = 0;` |
|  20403591 |   23 | `	return SXRET_OK;` |
|         5 |   24 | `}` |
|  33824485 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 | `{` |
|         - |   27 | `	unsigned char *zbase;` |
|  33824490 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4763501 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4763501 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4595131 |   34 | `			pSet->nSize = 4;` |
|   2297563 |   35 | `		}` |
|   4763501 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4763501 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4763501 |   40 | `		pSet->pBase = pNew;` |
|   4763501 |   41 | `		pSet->nSize <<= 1;` |
|   2381748 |   42 | `	}` |
|  33824490 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 253386410 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  33824490 |   45 | `	pSet->nUsed++;` |
|  33824490 |   46 | `	return SXRET_OK;` |
|  16912291 |   47 | `}` |
|   1411116 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 | `{` |
|   1411121 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1411121 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1411121 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1411121 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1411121 |   60 | `	pSet->nSize = nItem;` |
|   1411121 |   61 | `	return SXRET_OK;` |
|    705563 |   62 | `}` |
|   2272811 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 | `{` |
|   2272816 |   65 | `	pSet->nUsed   = 0;` |
|   2272816 |   66 | `	pSet->nCursor = 0;` |
|   2272816 |   67 | `	return SXRET_OK;` |
|         5 |   68 | `}` |
|     59012 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 | `{` |
|     59017 |   71 | `	pSet->nCursor = 0;` |
|     59017 |   72 | `	return SXRET_OK;` |
|         5 |   73 | `}` |
|     63210 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 | `{` |
|         - |   76 | `	register unsigned char *zSrc;` |
|     63215 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     24561 |   79 | `		pSet->nCursor = 0;` |
|     24561 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     38659 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     38659 |   83 | `	if( ppEntry ){` |
|     38659 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     19327 |   85 | `	}` |
|     38659 |   86 | `	pSet->nCursor++;` |
|     38659 |   87 | `	return SXRET_OK;` |
|     31610 |   88 | `}` |
|         - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         8 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|         1 |   91 | `{` |
|         - |   92 | `	register unsigned char *zSrc;` |
|         9 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         3 |   94 | `		return 0;` |
|         - |   95 | `	}` |
|         7 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|         7 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|         5 |   98 | `}` |
|         - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    236844 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 | `{` |
|    236849 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       295 |  103 | `		pSet->nUsed = nNewSize;` |
|       145 |  104 | `	}` |
|    236849 |  105 | `	return SXRET_OK;` |
|         5 |  106 | `}` |
|  10414258 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 | `{` |
|  10414263 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10414263 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5223925 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2611960 |  112 | `	}` |
|  10414263 |  113 | `	pSet->pBase = 0;` |
|  10414263 |  114 | `	pSet->nUsed = 0;` |
|  10414263 |  115 | `	pSet->nCursor = 0;` |
|  10414263 |  116 | `	return rc;` |
|         5 |  117 | `}` |
|   6084412 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 | `{` |
|         - |  120 | `	const char *zBase;` |
|   6084417 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       133 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   6084289 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   6084289 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   3042211 |  126 | `}` |
|   3665576 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 | `{` |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3665581 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2189029 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1476557 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1476557 |  135 | `	pSet->nUsed--;` |
|   1476557 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1476557 |  137 | `	return pData;` |
|   1832793 |  138 | `}` |
|  13896875 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	const char *zBase;` |
|  13896880 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13896880 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13896880 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6948811 |  148 | `}` |
|         - |  149 | `/* Private hash entry */` |
|         - |  150 | `struct SyHashEntry_Pr` |
|         - |  151 | `{` |
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
|    666726 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 | `{` |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    666731 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    666731 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    666731 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    666731 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    666731 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    666731 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    666731 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    666731 |  180 | `	pHash->nEntry = 0;` |
|    666731 |  181 | `	pHash->apBucket = apNew;` |
|    666731 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    666731 |  183 | `	return SXRET_OK;` |
|    333368 |  184 | `}` |
|    147100 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 | `{` |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    147105 |  193 | `	pEntry = pHash->pList;` |
|     77566 |  194 | `	for(;;){` |
|    155137 |  195 | `		if( pHash->nEntry == 0 ){` |
|    147105 |  196 | `			break;` |
|         - |  197 | `		}` |
|      8037 |  198 | `		pNext = pEntry->pNext;` |
|      8037 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      8037 |  200 | `		pEntry = pNext;` |
|      8037 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    147105 |  203 | `	if( pHash->apBucket ){` |
|    147105 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     73550 |  205 | `	}` |
|    147105 |  206 | `	pHash->apBucket = 0;` |
|    147105 |  207 | `	pHash->nBucketSize = 0;` |
|    147105 |  208 | `	pHash->pAllocator = 0;` |
|    147105 |  209 | `	return SXRET_OK;` |
|         5 |  210 | `}` |
|  18706514 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 | `{` |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  18706519 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  18706519 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  16977725 |  218 | `	for(;;){` |
|  34049045 |  219 | `		if( pEntry == 0 ){` |
|   9960695 |  220 | `			break;` |
|         - |  221 | `		}` |
|  28461011 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   8745834 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   8745829 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  15342531 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   9960695 |  229 | `	return 0;` |
|   9353784 |  230 | `}` |
|  19655954 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 | `{` |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  19655959 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    949669 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  18706295 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  18706295 |  244 | `	if( pEntry == 0 ){` |
|   9960695 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   8745605 |  247 | `	return (SyHashEntry *)pEntry;` |
|   9828504 |  248 | `}` |
|    145972 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 | `{` |
|         - |  251 | `	sxi32 rc;` |
|    145977 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    115187 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     57596 |  254 | `	}else{` |
|     30795 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    145977 |  257 | `	if( pEntry->pNextCollide ){` |
|      5183 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2591 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    145977 |  261 | `	if( pHash->pLast == pEntry ){` |
|    139675 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     69835 |  263 | `	}` |
|    145977 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    145977 |  265 | `	pHash->nEntry--;` |
|    145977 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    145977 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    145977 |  272 | `	return rc;` |
|         5 |  273 | `}` |
|       224 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 | `{` |
|         - |  276 | `	SyHashEntry_Pr *pEntry;` |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( INVALID_HASH(pHash) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|       229 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       229 |  284 | `	if( pEntry == 0 ){` |
|       ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|         - |  286 | `	}` |
|       229 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       229 |  288 | `	return rc;` |
|       117 |  289 | `}` |
|    145748 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 | `{` |
|    145753 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    145753 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    145753 |  300 | `	return rc;` |
|         5 |  301 | `}` |
|   1318814 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 | `{` |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1318819 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1318819 |  310 | `	return SXRET_OK;` |
|         5 |  311 | `}` |
|   8279948 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 | `{` |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   8279953 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1318557 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1318557 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   6961401 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   6961401 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   6961401 |  328 | `	return (SyHashEntry *)pEntry;` |
|   4139979 |  329 | `}` |
|        10 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         1 |  331 | `{` |
|         - |  332 | `	SyHashEntry_Pr *pEntry;` |
|         - |  333 | `	sxi32 rc;` |
|         - |  334 | `	sxu32 n;` |
|         - |  335 | `#if defined(UNTRUST)` |
|         - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  337 | `		return 0;` |
|         - |  338 | `	}` |
|         - |  339 | `#endif` |
|        11 |  340 | `	pEntry = pHash->pList;` |
|      2043 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|      2033 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      2033 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|      2033 |  348 | `		pEntry = pEntry->pNext;` |
|      1017 |  349 | `	}` |
|        11 |  350 | `	return SXRET_OK;` |
|         6 |  351 | `}` |
|     32510 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  353 | `{` |
|     32515 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  355 | `	SyHashEntry_Pr *pEntry;` |
|         - |  356 | `	SyHashEntry_Pr **apNew;` |
|         - |  357 | `	sxu32 n,iBucket;` |
|         - |  358 |  |
|         - |  359 | `	/* Allocate a new larger table */` |
|     32515 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     32515 |  361 | `	if( apNew == 0 ){` |
|         - |  362 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  363 | `		return SXRET_OK;` |
|         - |  364 | `	}` |
|         - |  365 | `	/* Zero the new table */` |
|     32515 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  367 | `	/* Rehash all entries */` |
|   4098403 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   4065893 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  370 | `		/* Install in the new bucket */` |
|   4065893 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   4065893 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   4065893 |  373 | `		if( apNew[iBucket] != 0 ){` |
|   1951457 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    975610 |  375 | `		}` |
|   4065893 |  376 | `		apNew[iBucket] = pEntry;` |
|         - |  377 | `		/* Point to the next entry */` |
|   4065893 |  378 | `		pEntry = pEntry->pNext;` |
|   2032949 |  379 | `	}` |
|         - |  380 | `	/* Release the old table and reflect the change */` |
|     32515 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     32515 |  382 | `	pHash->apBucket = apNew;` |
|     32515 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     32515 |  384 | `	return SXRET_OK;` |
|     16260 |  385 | `}` |
|   5468244 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 | `{` |
|   5468249 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   5468249 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   5468249 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   3066702 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1533258 |  393 | `	}` |
|   5468249 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   5468249 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        51 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        51 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        51 |  401 | `		pHash->pLast = pEntry;` |
|        26 |  402 | `	}else{` |
|   5468199 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   5468249 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    349169 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    349169 |  408 | `		pHash->pLast = pEntry;` |
|    174582 |  409 | `	}` |
|   5468249 |  410 | `	pHash->nEntry++;` |
|   5468249 |  411 | `	return SXRET_OK;` |
|         5 |  412 | `}` |
|   5468244 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 | `{` |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   5468249 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     32515 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     32515 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     16255 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   5468249 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   5468249 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   5468249 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   5468249 |  435 | `	pEntry->pHash = pHash;` |
|   5468249 |  436 | `	pEntry->pKey = pKey;` |
|   5468249 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   5468249 |  438 | `	pEntry->pUserData = pUserData;` |
|   5468249 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   5468249 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   5468249 |  442 | `	return rc;` |
|   2734127 |  443 | `}` |
|   5468128 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 | `{` |
|   5468133 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|         5 |  447 | `}` |
|         - |  448 | `/*` |
|         - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|         - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|         - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|         - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|         - |  453 | ` */` |
|       116 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  455 | `{` |
|       118 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|         2 |  457 | `}` |
|    185166 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 | `{` |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    185171 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 | `}` |
|         - |  468 |  |
