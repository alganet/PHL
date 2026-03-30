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
|  12702740 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  12702742 |   16 | `	pSet->nSize = 0 ;` |
|  12702742 |   17 | `	pSet->nUsed = 0;` |
|  12702742 |   18 | `	pSet->nCursor = 0;` |
|  12702742 |   19 | `	pSet->eSize = ElemSize;` |
|  12702742 |   20 | `	pSet->pAllocator = pAllocator;` |
|  12702742 |   21 | `	pSet->pBase =  0;` |
|  12702742 |   22 | `	pSet->pUserData = 0;` |
|  12702742 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  20911730 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  20911732 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3684800 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3684800 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3583592 |   34 | `			pSet->nSize = 4;` |
|   1791795 |   35 | `		}` |
|   3684800 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3684800 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3684800 |   40 | `		pSet->pBase = pNew;` |
|   3684800 |   41 | `		pSet->nSize <<= 1;` |
|   1842399 |   42 | `	}` |
|  20911732 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 155421876 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  20911732 |   45 | `	pSet->nUsed++;` |
|  20911732 |   46 | `	return SXRET_OK;` |
|  10455889 |   47 |  |
|    691352 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    691354 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    691354 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    691354 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    691354 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    691354 |   60 | `	pSet->nSize = nItem;` |
|    691354 |   61 | `	return SXRET_OK;` |
|    345678 |   62 |  |
|   1156072 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1156074 |   65 | `	pSet->nUsed   = 0;` |
|   1156074 |   66 | `	pSet->nCursor = 0;` |
|   1156074 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     41430 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     41432 |   71 | `	pSet->nCursor = 0;` |
|     41432 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     45312 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     45314 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     16974 |   79 | `		pSet->nCursor = 0;` |
|     16974 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     28342 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     28342 |   83 | `	if( ppEntry ){` |
|     28342 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     14170 |   85 | `	}` |
|     28342 |   86 | `	pSet->nCursor++;` |
|     28342 |   87 | `	return SXRET_OK;` |
|     22658 |   88 |  |
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
|     85374 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     85376 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     85376 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   7782110 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   7782112 |  109 | `	sxi32 rc = SXRET_OK;` |
|   7782112 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4022542 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2011270 |  112 | `	}` |
|   7782112 |  113 | `	pSet->pBase = 0;` |
|   7782112 |  114 | `	pSet->nUsed = 0;` |
|   7782112 |  115 | `	pSet->nCursor = 0;` |
|   7782112 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4187748 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4187750 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4187660 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4187660 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2093876 |  126 |  |
|   3188184 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3188186 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2139952 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1048236 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1048236 |  135 | `	pSet->nUsed--;` |
|   1048236 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1048236 |  137 | `	return pData;` |
|   1594094 |  138 |  |
|   9828027 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   9828029 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   9828029 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   9828029 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4914258 |  148 |  |
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
|    146530 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    146532 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    146532 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    146532 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    146532 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    146532 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    146532 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    146532 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    146532 |  180 | `	pHash->nEntry = 0;` |
|    146532 |  181 | `	pHash->apBucket = apNew;` |
|    146532 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    146532 |  183 | `	return SXRET_OK;` |
|     73267 |  184 |  |
|     28068 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     28070 |  193 | `	pEntry = pHash->pList;` |
|     15727 |  194 | `	for(;;){` |
|     31456 |  195 | `		if( pHash->nEntry == 0 ){` |
|     28070 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3388 |  198 | `		pNext = pEntry->pNext;` |
|      3388 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3388 |  200 | `		pEntry = pNext;` |
|      3388 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     28070 |  203 | `	if( pHash->apBucket ){` |
|     28070 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     14034 |  205 | `	}` |
|     28070 |  206 | `	pHash->apBucket = 0;` |
|     28070 |  207 | `	pHash->nBucketSize = 0;` |
|     28070 |  208 | `	pHash->pAllocator = 0;` |
|     28070 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  10409310 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  10409312 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  10409312 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   9120473 |  218 | `	for(;;){` |
|  18312419 |  219 | `		if( pEntry == 0 ){` |
|   5720262 |  220 | `			break;` |
|         - |  221 | `		}` |
|  14936554 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   4689054 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   4689052 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   7903109 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   5720262 |  229 | `	return 0;` |
|   5204921 |  230 |  |
|  10495646 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  10495648 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     86346 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  10409304 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  10409304 |  244 | `	if( pEntry == 0 ){` |
|   5720262 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   4689044 |  247 | `	return (SyHashEntry *)pEntry;` |
|   5248089 |  248 |  |
|     80318 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     80320 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     60922 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     30462 |  254 | `	}else{` |
|     19400 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     80320 |  257 | `	if( pEntry->pNextCollide ){` |
|      4133 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2066 |  259 | `	}` |
|     80320 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     80320 |  261 | `	pHash->nEntry--;` |
|     80320 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     80320 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     80320 |  268 | `	return rc;` |
|         2 |  269 |  |
|         8 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|        10 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        10 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|        10 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        10 |  284 | `	return rc;` |
|         6 |  285 |  |
|     80310 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     80312 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     80312 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     80312 |  296 | `	return rc;` |
|         2 |  297 |  |
|    199204 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    199206 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    199206 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   1470600 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   1470602 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    198772 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    198772 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   1271832 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   1271832 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   1271832 |  324 | `	return (SyHashEntry *)pEntry;` |
|    735302 |  325 |  |
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
|     18180 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     18182 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     18182 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     18182 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     18182 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2505062 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2486882 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2486882 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2486882 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2486882 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1194080 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    597039 |  371 | `		}` |
|   2486882 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2486882 |  374 | `		pEntry = pEntry->pNext;` |
|   1243442 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     18182 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     18182 |  378 | `	pHash->apBucket = apNew;` |
|     18182 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     18182 |  380 | `	return SXRET_OK;` |
|      9092 |  381 |  |
|   2267010 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2267012 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2267012 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2267012 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1515170 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    757603 |  389 | `	}` |
|   2267012 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2267012 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2267012 |  393 | `	if( pHash->nEntry == 0 ){` |
|     90420 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     45209 |  395 | `	}` |
|   2267012 |  396 | `	pHash->nEntry++;` |
|   2267012 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2267010 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2267012 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     18182 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     18182 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      9090 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2267012 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2267012 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2267012 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2267012 |  421 | `	pEntry->pHash = pHash;` |
|   2267012 |  422 | `	pEntry->pKey = pKey;` |
|   2267012 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2267012 |  424 | `	pEntry->pUserData = pUserData;` |
|   2267012 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2267012 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2267012 |  428 | `	return rc;` |
|   1133507 |  429 |  |
|    103494 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    103496 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
