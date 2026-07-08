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
|  20688322 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 | `{` |
|  20688327 |   16 | `	pSet->nSize = 0 ;` |
|  20688327 |   17 | `	pSet->nUsed = 0;` |
|  20688327 |   18 | `	pSet->nCursor = 0;` |
|  20688327 |   19 | `	pSet->eSize = ElemSize;` |
|  20688327 |   20 | `	pSet->pAllocator = pAllocator;` |
|  20688327 |   21 | `	pSet->pBase =  0;` |
|  20688327 |   22 | `	pSet->pUserData = 0;` |
|  20688327 |   23 | `	return SXRET_OK;` |
|         5 |   24 | `}` |
|  34285819 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 | `{` |
|         - |   27 | `	unsigned char *zbase;` |
|  34285824 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4845505 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4845505 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4675243 |   34 | `			pSet->nSize = 4;` |
|   2337619 |   35 | `		}` |
|   4845505 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4845505 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4845505 |   40 | `		pSet->pBase = pNew;` |
|   4845505 |   41 | `		pSet->nSize <<= 1;` |
|   2422750 |   42 | `	}` |
|  34285824 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 256321900 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  34285824 |   45 | `	pSet->nUsed++;` |
|  34285824 |   46 | `	return SXRET_OK;` |
|  17142957 |   47 | `}` |
|   1427312 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 | `{` |
|   1427317 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1427317 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1427317 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1427317 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1427317 |   60 | `	pSet->nSize = nItem;` |
|   1427317 |   61 | `	return SXRET_OK;` |
|    713661 |   62 | `}` |
|   2305411 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 | `{` |
|   2305416 |   65 | `	pSet->nUsed   = 0;` |
|   2305416 |   66 | `	pSet->nCursor = 0;` |
|   2305416 |   67 | `	return SXRET_OK;` |
|         5 |   68 | `}` |
|     66824 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 | `{` |
|     66829 |   71 | `	pSet->nCursor = 0;` |
|     66829 |   72 | `	return SXRET_OK;` |
|         5 |   73 | `}` |
|     71034 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 | `{` |
|         - |   76 | `	register unsigned char *zSrc;` |
|     71039 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     28461 |   79 | `		pSet->nCursor = 0;` |
|     28461 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     42583 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     42583 |   83 | `	if( ppEntry ){` |
|     42583 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     21289 |   85 | `	}` |
|     42583 |   86 | `	pSet->nCursor++;` |
|     42583 |   87 | `	return SXRET_OK;` |
|     35522 |   88 | `}` |
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
|    239982 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 | `{` |
|    239987 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       649 |  103 | `		pSet->nUsed = nNewSize;` |
|       322 |  104 | `	}` |
|    239987 |  105 | `	return SXRET_OK;` |
|         5 |  106 | `}` |
|  10559992 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 | `{` |
|  10559997 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10559997 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5307029 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2653512 |  112 | `	}` |
|  10559997 |  113 | `	pSet->pBase = 0;` |
|  10559997 |  114 | `	pSet->nUsed = 0;` |
|  10559997 |  115 | `	pSet->nCursor = 0;` |
|  10559997 |  116 | `	return rc;` |
|         5 |  117 | `}` |
|   6136908 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 | `{` |
|         - |  120 | `	const char *zBase;` |
|   6136913 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       133 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   6136785 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   6136785 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   3068459 |  126 | `}` |
|   3716672 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 | `{` |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3716677 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2192671 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1524011 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1524011 |  135 | `	pSet->nUsed--;` |
|   1524011 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1524011 |  137 | `	return pData;` |
|   1858341 |  138 | `}` |
|  14124052 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	const char *zBase;` |
|  14124057 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  14124057 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  14124057 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   7062377 |  148 | `}` |
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
|    679628 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 | `{` |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    679633 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    679633 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    679633 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    679633 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    679633 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    679633 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    679633 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    679633 |  180 | `	pHash->nEntry = 0;` |
|    679633 |  181 | `	pHash->apBucket = apNew;` |
|    679633 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    679633 |  183 | `	return SXRET_OK;` |
|    339819 |  184 | `}` |
|    153956 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 | `{` |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    153961 |  193 | `	pEntry = pHash->pList;` |
|     81246 |  194 | `	for(;;){` |
|    162497 |  195 | `		if( pHash->nEntry == 0 ){` |
|    153961 |  196 | `			break;` |
|         - |  197 | `		}` |
|      8541 |  198 | `		pNext = pEntry->pNext;` |
|      8541 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      8541 |  200 | `		pEntry = pNext;` |
|      8541 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    153961 |  203 | `	if( pHash->apBucket ){` |
|    153961 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     76978 |  205 | `	}` |
|    153961 |  206 | `	pHash->apBucket = 0;` |
|    153961 |  207 | `	pHash->nBucketSize = 0;` |
|    153961 |  208 | `	pHash->pAllocator = 0;` |
|    153961 |  209 | `	return SXRET_OK;` |
|         5 |  210 | `}` |
|  19030438 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 | `{` |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  19030443 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  19030443 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  17114804 |  218 | `	for(;;){` |
|  34221868 |  219 | `		if( pEntry == 0 ){` |
|  10151045 |  220 | `			break;` |
|         - |  221 | `		}` |
|  28510277 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   8879408 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   8879403 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  15191430 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|  10151045 |  229 | `	return 0;` |
|   9515734 |  230 | `}` |
|  19996040 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 | `{` |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  19996045 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    965831 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  19030219 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  19030219 |  244 | `	if( pEntry == 0 ){` |
|  10151045 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   8879179 |  247 | `	return (SyHashEntry *)pEntry;` |
|   9998535 |  248 | `}` |
|    175202 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 | `{` |
|         - |  251 | `	sxi32 rc;` |
|    175207 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    136747 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     68376 |  254 | `	}else{` |
|     38465 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    175207 |  257 | `	if( pEntry->pNextCollide ){` |
|      5224 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2611 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    175207 |  261 | `	if( pHash->pLast == pEntry ){` |
|    168705 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     84350 |  263 | `	}` |
|    175207 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    175207 |  265 | `	pHash->nEntry--;` |
|    175207 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    175207 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    175207 |  272 | `	return rc;` |
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
|    174978 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 | `{` |
|    174983 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    174983 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    174983 |  300 | `	return rc;` |
|         5 |  301 | `}` |
|   1333538 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 | `{` |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1333543 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1333543 |  310 | `	return SXRET_OK;` |
|         5 |  311 | `}` |
|   8374228 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 | `{` |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   8374233 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1333281 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1333281 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   7040957 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   7040957 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   7040957 |  328 | `	return (SyHashEntry *)pEntry;` |
|   4187119 |  329 | `}` |
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
|      2075 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|      2065 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      2065 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|      2065 |  348 | `		pEntry = pEntry->pNext;` |
|      1033 |  349 | `	}` |
|        11 |  350 | `	return SXRET_OK;` |
|         6 |  351 | `}` |
|     32934 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  353 | `{` |
|     32939 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  355 | `	SyHashEntry_Pr *pEntry;` |
|         - |  356 | `	SyHashEntry_Pr **apNew;` |
|         - |  357 | `	sxu32 n,iBucket;` |
|         - |  358 |  |
|         - |  359 | `	/* Allocate a new larger table */` |
|     32939 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     32939 |  361 | `	if( apNew == 0 ){` |
|         - |  362 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  363 | `		return SXRET_OK;` |
|         - |  364 | `	}` |
|         - |  365 | `	/* Zero the new table */` |
|     32939 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  367 | `	/* Rehash all entries */` |
|   4151819 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   4118885 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  370 | `		/* Install in the new bucket */` |
|   4118885 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   4118885 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   4118885 |  373 | `		if( apNew[iBucket] != 0 ){` |
|   1976993 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    988453 |  375 | `		}` |
|   4118885 |  376 | `		apNew[iBucket] = pEntry;` |
|         - |  377 | `		/* Point to the next entry */` |
|   4118885 |  378 | `		pEntry = pEntry->pNext;` |
|   2059445 |  379 | `	}` |
|         - |  380 | `	/* Release the old table and reflect the change */` |
|     32939 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     32939 |  382 | `	pHash->apBucket = apNew;` |
|     32939 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     32939 |  384 | `	return SXRET_OK;` |
|     16472 |  385 | `}` |
|   5563446 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 | `{` |
|   5563451 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   5563451 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   5563451 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   3112707 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1556314 |  393 | `	}` |
|   5563451 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   5563451 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        51 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        51 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        51 |  401 | `		pHash->pLast = pEntry;` |
|        26 |  402 | `	}else{` |
|   5563401 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   5563451 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    358569 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    358569 |  408 | `		pHash->pLast = pEntry;` |
|    179282 |  409 | `	}` |
|   5563451 |  410 | `	pHash->nEntry++;` |
|   5563451 |  411 | `	return SXRET_OK;` |
|         5 |  412 | `}` |
|   5563446 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 | `{` |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   5563451 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     32939 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     32939 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     16467 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   5563451 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   5563451 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   5563451 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   5563451 |  435 | `	pEntry->pHash = pHash;` |
|   5563451 |  436 | `	pEntry->pKey = pKey;` |
|   5563451 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   5563451 |  438 | `	pEntry->pUserData = pUserData;` |
|   5563451 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   5563451 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   5563451 |  442 | `	return rc;` |
|   2781728 |  443 | `}` |
|   5563330 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 | `{` |
|   5563335 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|    214932 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 | `{` |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    214937 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 | `}` |
|         - |  468 |  |
