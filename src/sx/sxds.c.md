# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 293/304 lines (96.38%)

[Root index](../../index.md) | [Directory index](index.md)

|       Hits | Line | Source |
| ---------: | ---: | :--- |
|          - |    1 | `/**` |
|          - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|          - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|          - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|          - |    5 | ` */` |
|          - |    6 | `#include "sxtypes.h"` |
|          - |    7 | `#include "sxmacros.h"` |
|          - |    8 | `#include "sxset.h"` |
|          - |    9 | `#include "sxmem.h"` |
|          - |   10 | `#include "sxhashtable.h"` |
|          - |   11 | `#include "sxhash.h"` |
|          - |   12 | `#include "sxstr.h"` |
|          - |   13 |  |
|  162178724 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  162178729 |   16 | `	pSet->nSize = 0 ;` |
|  162178729 |   17 | `	pSet->nUsed = 0;` |
|  162178729 |   18 | `	pSet->nCursor = 0;` |
|  162178729 |   19 | `	pSet->eSize = ElemSize;` |
|  162178729 |   20 | `	pSet->pAllocator = pAllocator;` |
|  162178729 |   21 | `	pSet->pBase =  0;` |
|  162178729 |   22 | `	pSet->pUserData = 0;` |
|  162178729 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  369234848 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  369234853 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   21540833 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   21540833 |   33 | `		if( pSet->nSize <= 0 ){` |
|   18378255 |   34 | `			pSet->nSize = 4;` |
|    9189125 |   35 | `		}` |
|   21540833 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   21540833 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   21540833 |   40 | `		pSet->pBase = pNew;` |
|   21540833 |   41 | `		pSet->nSize <<= 1;` |
|   10770414 |   42 | `	}` |
|  369234853 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2912595215 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  369234853 |   45 | `	pSet->nUsed++;` |
|  369234853 |   46 | `	return SXRET_OK;` |
|  184617451 |   47 | `}` |
|   18318254 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   18318259 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   18318259 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   18318259 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   18318259 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   18318259 |   60 | `	pSet->nSize = nItem;` |
|   18318259 |   61 | `	return SXRET_OK;` |
|    9159132 |   62 | `}` |
|   27247393 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   27247398 |   65 | `	pSet->nUsed   = 0;` |
|   27247398 |   66 | `	pSet->nCursor = 0;` |
|   27247398 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      70350 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      70355 |   71 | `	pSet->nCursor = 0;` |
|      70355 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      74532 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      74537 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30435 |   79 | `		pSet->nCursor = 0;` |
|      30435 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      44107 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      44107 |   83 | `	if( ppEntry ){` |
|      44107 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      22051 |   85 | `	}` |
|      44107 |   86 | `	pSet->nCursor++;` |
|      44107 |   87 | `	return SXRET_OK;` |
|      37271 |   88 | `}` |
|          - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|          8 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|          1 |   91 | `{` |
|          - |   92 | `	register unsigned char *zSrc;` |
|          9 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          3 |   94 | `		return 0;` |
|          - |   95 | `	}` |
|          7 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|          7 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|          5 |   98 | `}` |
|          - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    2754482 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2754487 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1199 |  103 | `		pSet->nUsed = nNewSize;` |
|        597 |  104 | `	}` |
|    2754487 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   56319398 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   56319403 |  109 | `	sxi32 rc = SXRET_OK;` |
|   56319403 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   30672787 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   15336391 |  112 | `	}` |
|   56319403 |  113 | `	pSet->pBase = 0;` |
|   56319403 |  114 | `	pSet->nUsed = 0;` |
|   56319403 |  115 | `	pSet->nCursor = 0;` |
|   56319403 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   65550634 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   65550639 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19237 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   65531407 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   65531407 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   32775322 |  126 | `}` |
|    9409444 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9409449 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2232931 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7176523 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7176523 |  135 | `	pSet->nUsed--;` |
|    7176523 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7176523 |  137 | `	return pData;` |
|    4704727 |  138 | `}` |
|   34417838 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   34417843 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   34417821 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   34417821 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   17209083 |  148 | `}` |
|          - |  149 | `/* Private hash entry */` |
|          - |  150 | `struct SyHashEntry_Pr` |
|          - |  151 | `{` |
|          - |  152 | `	const void *pKey; /* Hash key */` |
|          - |  153 | `	sxu32 nKeyLen;    /* Key length */` |
|          - |  154 | `	void *pUserData;  /* User private data */` |
|          - |  155 | `	/* Private fields */` |
|          - |  156 | `	sxu32 nHash;` |
|          - |  157 | `	SyHash *pHash;` |
|          - |  158 | `	SyHashEntry_Pr *pNext,*pPrev; /* Next and previous entry in the list */` |
|          - |  159 | `	SyHashEntry_Pr *pNextCollide,*pPrevCollide; /* Collision list */` |
|          - |  160 | `};` |
|          - |  161 | `#define INVALID_HASH(H) ((H)->apBucket == 0)` |
|    1771888 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1771893 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1771893 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1771893 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1771893 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1771893 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1771893 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1771893 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1771893 |  180 | `	pHash->nEntry = 0;` |
|    1771893 |  181 | `	pHash->apBucket = apNew;` |
|    1771893 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1771893 |  183 | `	return SXRET_OK;` |
|     885949 |  184 | `}` |
|     413072 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     413077 |  193 | `	pEntry = pHash->pList;` |
|     221044 |  194 | `	for(;;){` |
|     442093 |  195 | `		if( pHash->nEntry == 0 ){` |
|     413077 |  196 | `			break;` |
|          - |  197 | `		}` |
|      29021 |  198 | `		pNext = pEntry->pNext;` |
|      29021 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      29021 |  200 | `		pEntry = pNext;` |
|      29021 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     413077 |  203 | `	if( pHash->apBucket ){` |
|     413077 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     206536 |  205 | `	}` |
|     413077 |  206 | `	pHash->apBucket = 0;` |
|     413077 |  207 | `	pHash->nBucketSize = 0;` |
|     413077 |  208 | `	pHash->pAllocator = 0;` |
|     413077 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   67389847 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   67389852 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   67389852 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   61458206 |  218 | `	for(;;){` |
|  122617989 |  219 | `		if( pEntry == 0 ){` |
|   24393825 |  220 | `			break;` |
|          - |  221 | `		}` |
|  119724058 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   43000055 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   42996032 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   55228142 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   24393825 |  229 | `	return 0;` |
|   33695207 |  230 | `}` |
|   74403443 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   74403448 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    7014077 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   67389376 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   67389376 |  244 | `	if( pEntry == 0 ){` |
|   24393807 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   42995574 |  247 | `	return (SyHashEntry *)pEntry;` |
|   37202005 |  248 | `}` |
|     437628 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     437633 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     357183 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     178594 |  254 | `	}else{` |
|      80455 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     437633 |  257 | `	if( pEntry->pNextCollide ){` |
|       4380 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2189 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     437633 |  261 | `	if( pHash->pLast == pEntry ){` |
|     427645 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     213820 |  263 | `	}` |
|     437633 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     437633 |  265 | `	pHash->nEntry--;` |
|     437633 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     437633 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     437633 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        476 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        481 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        481 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        463 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        463 |  288 | `	return rc;` |
|        243 |  289 | `}` |
|     437170 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     437175 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     437175 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     437175 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2839712 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2839717 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2839717 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   21236476 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   21236481 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2839451 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2839451 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18397035 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18397035 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18397035 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10618243 |  329 | `}` |
|         14 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|          1 |  331 | `{` |
|          - |  332 | `	SyHashEntry_Pr *pEntry;` |
|          - |  333 | `	sxi32 rc;` |
|          - |  334 | `	sxu32 n;` |
|          - |  335 | `#if defined(UNTRUST)` |
|          - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|          - |  337 | `		return 0;` |
|          - |  338 | `	}` |
|          - |  339 | `#endif` |
|         15 |  340 | `	pEntry = pHash->pList;` |
|       4105 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4091 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4091 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4091 |  348 | `		pEntry = pEntry->pNext;` |
|       2046 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|      91978 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      91983 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      91983 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      91983 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      91983 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14440911 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14348933 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14348933 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14348933 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14348933 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    6878708 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3439475 |  375 | `		}` |
|   14348933 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14348933 |  378 | `		pEntry = pEntry->pNext;` |
|    7174469 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      91983 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      91983 |  382 | `	pHash->apBucket = apNew;` |
|      91983 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      91983 |  384 | `	return SXRET_OK;` |
|      45994 |  385 | `}` |
|   18509654 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   18509659 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   18509659 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   18509659 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11742469 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5871323 |  393 | `	}` |
|   18509659 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   18509659 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   18509607 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   18509659 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     977169 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     977169 |  408 | `		pHash->pLast = pEntry;` |
|     488582 |  409 | `	}` |
|   18509659 |  410 | `	pHash->nEntry++;` |
|   18509659 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   18509654 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   18509659 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      91983 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      91983 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45989 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   18509659 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   18509659 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   18509659 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   18509659 |  435 | `	pEntry->pHash = pHash;` |
|   18509659 |  436 | `	pEntry->pKey = pKey;` |
|   18509659 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   18509659 |  438 | `	pEntry->pUserData = pUserData;` |
|   18509659 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   18509659 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   18509659 |  442 | `	return rc;` |
|    9254832 |  443 | `}` |
|   18509520 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   18509525 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        134 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          2 |  455 | `{` |
|        136 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          2 |  457 | `}` |
|     476822 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     476827 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
