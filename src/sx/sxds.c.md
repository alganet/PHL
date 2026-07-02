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
|  19978714 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 | `{` |
|  19978719 |   16 | `	pSet->nSize = 0 ;` |
|  19978719 |   17 | `	pSet->nUsed = 0;` |
|  19978719 |   18 | `	pSet->nCursor = 0;` |
|  19978719 |   19 | `	pSet->eSize = ElemSize;` |
|  19978719 |   20 | `	pSet->pAllocator = pAllocator;` |
|  19978719 |   21 | `	pSet->pBase =  0;` |
|  19978719 |   22 | `	pSet->pUserData = 0;` |
|  19978719 |   23 | `	return SXRET_OK;` |
|         5 |   24 | `}` |
|  33043367 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 | `{` |
|         - |   27 | `	unsigned char *zbase;` |
|  33043372 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4691897 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4691897 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4527985 |   34 | `			pSet->nSize = 4;` |
|   2263990 |   35 | `		}` |
|   4691897 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4691897 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4691897 |   40 | `		pSet->pBase = pNew;` |
|   4691897 |   41 | `		pSet->nSize <<= 1;` |
|   2345946 |   42 | `	}` |
|  33043372 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 247597860 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  33043372 |   45 | `	pSet->nUsed++;` |
|  33043372 |   46 | `	return SXRET_OK;` |
|  16521731 |   47 | `}` |
|   1366334 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 | `{` |
|   1366339 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1366339 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1366339 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1366339 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1366339 |   60 | `	pSet->nSize = nItem;` |
|   1366339 |   61 | `	return SXRET_OK;` |
|    683172 |   62 | `}` |
|   1877505 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 | `{` |
|   1877510 |   65 | `	pSet->nUsed   = 0;` |
|   1877510 |   66 | `	pSet->nCursor = 0;` |
|   1877510 |   67 | `	return SXRET_OK;` |
|         5 |   68 | `}` |
|     58856 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 | `{` |
|     58861 |   71 | `	pSet->nCursor = 0;` |
|     58861 |   72 | `	return SXRET_OK;` |
|         5 |   73 | `}` |
|     63062 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 | `{` |
|         - |   76 | `	register unsigned char *zSrc;` |
|     63067 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     24421 |   79 | `		pSet->nCursor = 0;` |
|     24421 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     38651 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     38651 |   83 | `	if( ppEntry ){` |
|     38651 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     19323 |   85 | `	}` |
|     38651 |   86 | `	pSet->nCursor++;` |
|     38651 |   87 | `	return SXRET_OK;` |
|     31536 |   88 | `}` |
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
|    230164 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 | `{` |
|    230169 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       295 |  103 | `		pSet->nUsed = nNewSize;` |
|       145 |  104 | `	}` |
|    230169 |  105 | `	return SXRET_OK;` |
|         5 |  106 | `}` |
|  10243472 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 | `{` |
|  10243477 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10243477 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5138083 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2569039 |  112 | `	}` |
|  10243477 |  113 | `	pSet->pBase = 0;` |
|  10243477 |  114 | `	pSet->nUsed = 0;` |
|  10243477 |  115 | `	pSet->nCursor = 0;` |
|  10243477 |  116 | `	return rc;` |
|         5 |  117 | `}` |
|   5975280 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 | `{` |
|         - |  120 | `	const char *zBase;` |
|   5975285 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       133 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5975157 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5975157 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2987645 |  126 | `}` |
|   3635998 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 | `{` |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3636003 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2185771 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1450237 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1450237 |  135 | `	pSet->nUsed--;` |
|   1450237 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1450237 |  137 | `	return pData;` |
|   1818004 |  138 | `}` |
|  13724500 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	const char *zBase;` |
|  13724505 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13724505 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13724505 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6862619 |  148 | `}` |
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
|    601748 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 | `{` |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    601753 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    601753 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    601753 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    601753 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    601753 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    601753 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    601753 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    601753 |  180 | `	pHash->nEntry = 0;` |
|    601753 |  181 | `	pHash->apBucket = apNew;` |
|    601753 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    601753 |  183 | `	return SXRET_OK;` |
|    300879 |  184 | `}` |
|    107830 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 | `{` |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    107835 |  193 | `	pEntry = pHash->pList;` |
|     57875 |  194 | `	for(;;){` |
|    115755 |  195 | `		if( pHash->nEntry == 0 ){` |
|    107835 |  196 | `			break;` |
|         - |  197 | `		}` |
|      7925 |  198 | `		pNext = pEntry->pNext;` |
|      7925 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      7925 |  200 | `		pEntry = pNext;` |
|      7925 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    107835 |  203 | `	if( pHash->apBucket ){` |
|    107835 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     53915 |  205 | `	}` |
|    107835 |  206 | `	pHash->apBucket = 0;` |
|    107835 |  207 | `	pHash->nBucketSize = 0;` |
|    107835 |  208 | `	pHash->pAllocator = 0;` |
|    107835 |  209 | `	return SXRET_OK;` |
|         5 |  210 | `}` |
|  18151114 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 | `{` |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  18151119 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  18151119 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  16379248 |  218 | `	for(;;){` |
|  32878644 |  219 | `		if( pEntry == 0 ){` |
|   9671227 |  220 | `			break;` |
|         - |  221 | `		}` |
|  27447118 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   8479902 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   8479897 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  14727530 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   9671227 |  229 | `	return 0;` |
|   9076072 |  230 | `}` |
|  19059940 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 | `{` |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  19059945 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    909041 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  18150909 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  18150909 |  244 | `	if( pEntry == 0 ){` |
|   9671227 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   8479687 |  247 | `	return (SyHashEntry *)pEntry;` |
|   9530485 |  248 | `}` |
|    133826 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 | `{` |
|         - |  251 | `	sxi32 rc;` |
|    133831 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    103801 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     51903 |  254 | `	}else{` |
|     30035 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    133831 |  257 | `	if( pEntry->pNextCollide ){` |
|      5229 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2614 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    133831 |  261 | `	if( pHash->pLast == pEntry ){` |
|    127523 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     63759 |  263 | `	}` |
|    133831 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    133831 |  265 | `	pHash->nEntry--;` |
|    133831 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    133831 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    133831 |  272 | `	return rc;` |
|         5 |  273 | `}` |
|       210 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 | `{` |
|         - |  276 | `	SyHashEntry_Pr *pEntry;` |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( INVALID_HASH(pHash) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|       215 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       215 |  284 | `	if( pEntry == 0 ){` |
|       ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|         - |  286 | `	}` |
|       215 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       215 |  288 | `	return rc;` |
|       110 |  289 | `}` |
|    133616 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 | `{` |
|    133621 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    133621 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    133621 |  300 | `	return rc;` |
|         5 |  301 | `}` |
|   1208484 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 | `{` |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1208489 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1208489 |  310 | `	return SXRET_OK;` |
|         5 |  311 | `}` |
|   7663176 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 | `{` |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   7663181 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1208227 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1208227 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   6454959 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   6454959 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   6454959 |  328 | `	return (SyHashEntry *)pEntry;` |
|   3831593 |  329 | `}` |
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
|      2021 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|      2011 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      2011 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|      2011 |  348 | `		pEntry = pEntry->pNext;` |
|      1006 |  349 | `	}` |
|        11 |  350 | `	return SXRET_OK;` |
|         6 |  351 | `}` |
|     31574 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  353 | `{` |
|     31579 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  355 | `	SyHashEntry_Pr *pEntry;` |
|         - |  356 | `	SyHashEntry_Pr **apNew;` |
|         - |  357 | `	sxu32 n,iBucket;` |
|         - |  358 |  |
|         - |  359 | `	/* Allocate a new larger table */` |
|     31579 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     31579 |  361 | `	if( apNew == 0 ){` |
|         - |  362 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  363 | `		return SXRET_OK;` |
|         - |  364 | `	}` |
|         - |  365 | `	/* Zero the new table */` |
|     31579 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  367 | `	/* Rehash all entries */` |
|   3980635 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3949061 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  370 | `		/* Install in the new bucket */` |
|   3949061 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3949061 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3949061 |  373 | `		if( apNew[iBucket] != 0 ){` |
|   1892212 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    946175 |  375 | `		}` |
|   3949061 |  376 | `		apNew[iBucket] = pEntry;` |
|         - |  377 | `		/* Point to the next entry */` |
|   3949061 |  378 | `		pEntry = pEntry->pNext;` |
|   1974533 |  379 | `	}` |
|         - |  380 | `	/* Release the old table and reflect the change */` |
|     31579 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     31579 |  382 | `	pHash->apBucket = apNew;` |
|     31579 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     31579 |  384 | `	return SXRET_OK;` |
|     15792 |  385 | `}` |
|   5219364 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 | `{` |
|   5219369 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   5219369 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   5219369 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2958511 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1479273 |  393 | `	}` |
|   5219369 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   5219369 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        51 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        51 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        51 |  401 | `		pHash->pLast = pEntry;` |
|        26 |  402 | `	}else{` |
|   5219319 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   5219369 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    324743 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    324743 |  408 | `		pHash->pLast = pEntry;` |
|    162369 |  409 | `	}` |
|   5219369 |  410 | `	pHash->nEntry++;` |
|   5219369 |  411 | `	return SXRET_OK;` |
|         5 |  412 | `}` |
|   5219364 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 | `{` |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   5219369 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     31579 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     31579 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     15787 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   5219369 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   5219369 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   5219369 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   5219369 |  435 | `	pEntry->pHash = pHash;` |
|   5219369 |  436 | `	pEntry->pKey = pKey;` |
|   5219369 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   5219369 |  438 | `	pEntry->pUserData = pUserData;` |
|   5219369 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   5219369 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   5219369 |  442 | `	return rc;` |
|   2609687 |  443 | `}` |
|   5219248 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 | `{` |
|   5219253 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|    171748 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 | `{` |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    171753 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 | `}` |
|         - |  468 |  |
