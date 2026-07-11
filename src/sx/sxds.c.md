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
|  20837890 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 | `{` |
|  20837895 |   16 | `	pSet->nSize = 0 ;` |
|  20837895 |   17 | `	pSet->nUsed = 0;` |
|  20837895 |   18 | `	pSet->nCursor = 0;` |
|  20837895 |   19 | `	pSet->eSize = ElemSize;` |
|  20837895 |   20 | `	pSet->pAllocator = pAllocator;` |
|  20837895 |   21 | `	pSet->pBase =  0;` |
|  20837895 |   22 | `	pSet->pUserData = 0;` |
|  20837895 |   23 | `	return SXRET_OK;` |
|         5 |   24 | `}` |
|  34516481 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 | `{` |
|         - |   27 | `	unsigned char *zbase;` |
|  34516486 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4880213 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4880213 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4708633 |   34 | `			pSet->nSize = 4;` |
|   2354314 |   35 | `		}` |
|   4880213 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4880213 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4880213 |   40 | `		pSet->pBase = pNew;` |
|   4880213 |   41 | `		pSet->nSize <<= 1;` |
|   2440104 |   42 | `	}` |
|  34516486 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 257891538 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  34516486 |   45 | `	pSet->nUsed++;` |
|  34516486 |   46 | `	return SXRET_OK;` |
|  17258288 |   47 | `}` |
|   1436044 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 | `{` |
|   1436049 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1436049 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1436049 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1436049 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1436049 |   60 | `	pSet->nSize = nItem;` |
|   1436049 |   61 | `	return SXRET_OK;` |
|    718027 |   62 | `}` |
|   2313263 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 | `{` |
|   2313268 |   65 | `	pSet->nUsed   = 0;` |
|   2313268 |   66 | `	pSet->nCursor = 0;` |
|   2313268 |   67 | `	return SXRET_OK;` |
|         5 |   68 | `}` |
|     66820 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 | `{` |
|     66825 |   71 | `	pSet->nCursor = 0;` |
|     66825 |   72 | `	return SXRET_OK;` |
|         5 |   73 | `}` |
|     70948 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 | `{` |
|         - |   76 | `	register unsigned char *zSrc;` |
|     70953 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     28743 |   79 | `		pSet->nCursor = 0;` |
|     28743 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     42215 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     42215 |   83 | `	if( ppEntry ){` |
|     42215 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     21105 |   85 | `	}` |
|     42215 |   86 | `	pSet->nCursor++;` |
|     42215 |   87 | `	return SXRET_OK;` |
|     35479 |   88 | `}` |
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
|    241744 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 | `{` |
|    241749 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       679 |  103 | `		pSet->nUsed = nNewSize;` |
|       337 |  104 | `	}` |
|    241749 |  105 | `	return SXRET_OK;` |
|         5 |  106 | `}` |
|  10638066 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 | `{` |
|  10638071 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10638071 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5342131 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2671063 |  112 | `	}` |
|  10638071 |  113 | `	pSet->pBase = 0;` |
|  10638071 |  114 | `	pSet->nUsed = 0;` |
|  10638071 |  115 | `	pSet->nCursor = 0;` |
|  10638071 |  116 | `	return rc;` |
|         5 |  117 | `}` |
|   6170494 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 | `{` |
|         - |  120 | `	const char *zBase;` |
|   6170499 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       133 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   6170371 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   6170371 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   3085252 |  126 | `}` |
|   3739622 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 | `{` |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3739627 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2193801 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1545831 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1545831 |  135 | `	pSet->nUsed--;` |
|   1545831 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1545831 |  137 | `	return pData;` |
|   1869816 |  138 | `}` |
|  14214506 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	const char *zBase;` |
|  14214511 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  14214511 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  14214511 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   7107584 |  148 | `}` |
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
|    685348 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 | `{` |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    685353 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    685353 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    685353 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    685353 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    685353 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    685353 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    685353 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    685353 |  180 | `	pHash->nEntry = 0;` |
|    685353 |  181 | `	pHash->apBucket = apNew;` |
|    685353 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    685353 |  183 | `	return SXRET_OK;` |
|    342679 |  184 | `}` |
|    156240 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 | `{` |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    156245 |  193 | `	pEntry = pHash->pList;` |
|     83817 |  194 | `	for(;;){` |
|    167639 |  195 | `		if( pHash->nEntry == 0 ){` |
|    156245 |  196 | `			break;` |
|         - |  197 | `		}` |
|     11399 |  198 | `		pNext = pEntry->pNext;` |
|     11399 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     11399 |  200 | `		pEntry = pNext;` |
|     11399 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    156245 |  203 | `	if( pHash->apBucket ){` |
|    156245 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     78120 |  205 | `	}` |
|    156245 |  206 | `	pHash->apBucket = 0;` |
|    156245 |  207 | `	pHash->nBucketSize = 0;` |
|    156245 |  208 | `	pHash->pAllocator = 0;` |
|    156245 |  209 | `	return SXRET_OK;` |
|         5 |  210 | `}` |
|  20101354 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 | `{` |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  20101359 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  20101359 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  18239064 |  218 | `	for(;;){` |
|  36367600 |  219 | `		if( pEntry == 0 ){` |
|  10245323 |  220 | `			break;` |
|         - |  221 | `		}` |
|  31050051 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   9856048 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   9856041 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  16266246 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|  10245323 |  229 | `	return 0;` |
|  10051192 |  230 | `}` |
|  21077966 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 | `{` |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  21077971 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    976881 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  20101095 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  20101095 |  244 | `	if( pEntry == 0 ){` |
|  10245323 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   9855777 |  247 | `	return (SyHashEntry *)pEntry;` |
|  10539498 |  248 | `}` |
|    183526 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 | `{` |
|         - |  251 | `	sxi32 rc;` |
|    183531 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    143999 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     72002 |  254 | `	}else{` |
|     39537 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    183531 |  257 | `	if( pEntry->pNextCollide ){` |
|      3846 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      1922 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    183531 |  261 | `	if( pHash->pLast == pEntry ){` |
|    176941 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     88468 |  263 | `	}` |
|    183531 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    183531 |  265 | `	pHash->nEntry--;` |
|    183531 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    183531 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    183531 |  272 | `	return rc;` |
|         5 |  273 | `}` |
|       264 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 | `{` |
|         - |  276 | `	SyHashEntry_Pr *pEntry;` |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( INVALID_HASH(pHash) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|       269 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       269 |  284 | `	if( pEntry == 0 ){` |
|       ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|         - |  286 | `	}` |
|       269 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       269 |  288 | `	return rc;` |
|       137 |  289 | `}` |
|    183262 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 | `{` |
|    183267 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    183267 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    183267 |  300 | `	return rc;` |
|         5 |  301 | `}` |
|   1336052 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 | `{` |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1336057 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1336057 |  310 | `	return SXRET_OK;` |
|         5 |  311 | `}` |
|   8395814 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 | `{` |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   8395819 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1335795 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1335795 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   7060029 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   7060029 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   7060029 |  328 | `	return (SyHashEntry *)pEntry;` |
|   4197912 |  329 | `}` |
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
|      2101 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|      2091 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      2091 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|      2091 |  348 | `		pEntry = pEntry->pNext;` |
|      1046 |  349 | `	}` |
|        11 |  350 | `	return SXRET_OK;` |
|         6 |  351 | `}` |
|     33174 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  353 | `{` |
|     33179 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  355 | `	SyHashEntry_Pr *pEntry;` |
|         - |  356 | `	SyHashEntry_Pr **apNew;` |
|         - |  357 | `	sxu32 n,iBucket;` |
|         - |  358 |  |
|         - |  359 | `	/* Allocate a new larger table */` |
|     33179 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     33179 |  361 | `	if( apNew == 0 ){` |
|         - |  362 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  363 | `		return SXRET_OK;` |
|         - |  364 | `	}` |
|         - |  365 | `	/* Zero the new table */` |
|     33179 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  367 | `	/* Rehash all entries */` |
|   4183835 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   4150661 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  370 | `		/* Install in the new bucket */` |
|   4150661 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   4150661 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   4150661 |  373 | `		if( apNew[iBucket] != 0 ){` |
|   2006010 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|   1003101 |  375 | `		}` |
|   4150661 |  376 | `		apNew[iBucket] = pEntry;` |
|         - |  377 | `		/* Point to the next entry */` |
|   4150661 |  378 | `		pEntry = pEntry->pNext;` |
|   2075333 |  379 | `	}` |
|         - |  380 | `	/* Release the old table and reflect the change */` |
|     33179 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     33179 |  382 | `	pHash->apBucket = apNew;` |
|     33179 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     33179 |  384 | `	return SXRET_OK;` |
|     16592 |  385 | `}` |
|   5637762 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 | `{` |
|   5637767 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   5637767 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   5637767 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   3145468 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1572725 |  393 | `	}` |
|   5637767 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   5637767 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        51 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        51 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        51 |  401 | `		pHash->pLast = pEntry;` |
|        26 |  402 | `	}else{` |
|   5637717 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   5637767 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    363511 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    363511 |  408 | `		pHash->pLast = pEntry;` |
|    181753 |  409 | `	}` |
|   5637767 |  410 | `	pHash->nEntry++;` |
|   5637767 |  411 | `	return SXRET_OK;` |
|         5 |  412 | `}` |
|   5637762 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 | `{` |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   5637767 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     33179 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     33179 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     16587 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   5637767 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   5637767 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   5637767 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   5637767 |  435 | `	pEntry->pHash = pHash;` |
|   5637767 |  436 | `	pEntry->pKey = pKey;` |
|   5637767 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   5637767 |  438 | `	pEntry->pUserData = pUserData;` |
|   5637767 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   5637767 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   5637767 |  442 | `	return rc;` |
|   2818886 |  443 | `}` |
|   5637646 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 | `{` |
|   5637651 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|    223480 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 | `{` |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    223485 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 | `}` |
|         - |  468 |  |
