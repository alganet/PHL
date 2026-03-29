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
|  12499826 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  12499828 |   16 | `	pSet->nSize = 0 ;` |
|  12499828 |   17 | `	pSet->nUsed = 0;` |
|  12499828 |   18 | `	pSet->nCursor = 0;` |
|  12499828 |   19 | `	pSet->eSize = ElemSize;` |
|  12499828 |   20 | `	pSet->pAllocator = pAllocator;` |
|  12499828 |   21 | `	pSet->pBase =  0;` |
|  12499828 |   22 | `	pSet->pUserData = 0;` |
|  12499828 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  20504322 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  20504324 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3659126 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3659126 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3561124 |   34 | `			pSet->nSize = 4;` |
|   1780561 |   35 | `		}` |
|   3659126 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3659126 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3659126 |   40 | `		pSet->pBase = pNew;` |
|   3659126 |   41 | `		pSet->nSize <<= 1;` |
|   1829562 |   42 | `	}` |
|  20504324 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 152444536 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  20504324 |   45 | `	pSet->nUsed++;` |
|  20504324 |   46 | `	return SXRET_OK;` |
|  10252185 |   47 |  |
|    669042 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    669044 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    669044 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    669044 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    669044 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    669044 |   60 | `	pSet->nSize = nItem;` |
|    669044 |   61 | `	return SXRET_OK;` |
|    334523 |   62 |  |
|   1131990 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1131992 |   65 | `	pSet->nUsed   = 0;` |
|   1131992 |   66 | `	pSet->nCursor = 0;` |
|   1131992 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     41078 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     41080 |   71 | `	pSet->nCursor = 0;` |
|     41080 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     44960 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     44962 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     16798 |   79 | `		pSet->nCursor = 0;` |
|     16798 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     28166 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     28166 |   83 | `	if( ppEntry ){` |
|     28166 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     14082 |   85 | `	}` |
|     28166 |   86 | `	pSet->nCursor++;` |
|     28166 |   87 | `	return SXRET_OK;` |
|     22482 |   88 |  |
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
|     82398 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     82400 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     82400 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   7714478 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   7714480 |  109 | `	sxi32 rc = SXRET_OK;` |
|   7714480 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3986362 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1993180 |  112 | `	}` |
|   7714480 |  113 | `	pSet->pBase = 0;` |
|   7714480 |  114 | `	pSet->nUsed = 0;` |
|   7714480 |  115 | `	pSet->nCursor = 0;` |
|   7714480 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4119322 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4119324 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4119234 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4119234 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2059663 |  126 |  |
|   3177782 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3177784 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2138774 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1039012 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1039012 |  135 | `	pSet->nUsed--;` |
|   1039012 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1039012 |  137 | `	return pData;` |
|   1588893 |  138 |  |
|   9748030 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   9748032 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   9748032 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   9748032 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4874243 |  148 |  |
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
|    141726 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    141728 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    141728 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    141728 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    141728 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    141728 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    141728 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    141728 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    141728 |  180 | `	pHash->nEntry = 0;` |
|    141728 |  181 | `	pHash->apBucket = apNew;` |
|    141728 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    141728 |  183 | `	return SXRET_OK;` |
|     70865 |  184 |  |
|     27500 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     27502 |  193 | `	pEntry = pHash->pList;` |
|     15422 |  194 | `	for(;;){` |
|     30846 |  195 | `		if( pHash->nEntry == 0 ){` |
|     27502 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3346 |  198 | `		pNext = pEntry->pNext;` |
|      3346 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3346 |  200 | `		pEntry = pNext;` |
|      3346 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     27502 |  203 | `	if( pHash->apBucket ){` |
|     27502 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     13750 |  205 | `	}` |
|     27502 |  206 | `	pHash->apBucket = 0;` |
|     27502 |  207 | `	pHash->nBucketSize = 0;` |
|     27502 |  208 | `	pHash->pAllocator = 0;` |
|     27502 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  10213818 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  10213820 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  10213820 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   9021993 |  218 | `	for(;;){` |
|  17976076 |  219 | `		if( pEntry == 0 ){` |
|   5608176 |  220 | `			break;` |
|         - |  221 | `		}` |
|  14670594 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   4605648 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   4605646 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   7762258 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   5608176 |  229 | `	return 0;` |
|   5107175 |  230 |  |
|  10297298 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  10297300 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     83490 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  10213812 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  10213812 |  244 | `	if( pEntry == 0 ){` |
|   5608176 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   4605638 |  247 | `	return (SyHashEntry *)pEntry;` |
|   5148915 |  248 |  |
|     79162 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     79164 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     59958 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     29980 |  254 | `	}else{` |
|     19208 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     79164 |  257 | `	if( pEntry->pNextCollide ){` |
|      4133 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2066 |  259 | `	}` |
|     79164 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     79164 |  261 | `	pHash->nEntry--;` |
|     79164 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     79164 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     79164 |  268 | `	return rc;` |
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
|     79154 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     79156 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     79156 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     79156 |  296 | `	return rc;` |
|         2 |  297 |  |
|    171918 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    171920 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    171920 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   1230562 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   1230564 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    171486 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    171486 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   1059080 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   1059080 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   1059080 |  324 | `	return (SyHashEntry *)pEntry;` |
|    615283 |  325 |  |
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
|     17618 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     17620 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     17620 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     17620 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     17620 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2428564 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2410946 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2410946 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2410946 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2410946 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1157615 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    578804 |  371 | `		}` |
|   2410946 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2410946 |  374 | `		pEntry = pEntry->pNext;` |
|   1205474 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     17620 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     17620 |  378 | `	pHash->apBucket = apNew;` |
|     17620 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     17620 |  380 | `	return SXRET_OK;` |
|      8811 |  381 |  |
|   2196312 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2196314 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2196314 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2196314 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1468722 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    734345 |  389 | `	}` |
|   2196314 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2196314 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2196314 |  393 | `	if( pHash->nEntry == 0 ){` |
|     87402 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     43700 |  395 | `	}` |
|   2196314 |  396 | `	pHash->nEntry++;` |
|   2196314 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2196312 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2196314 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     17620 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     17620 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      8809 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2196314 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2196314 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2196314 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2196314 |  421 | `	pEntry->pHash = pHash;` |
|   2196314 |  422 | `	pEntry->pKey = pKey;` |
|   2196314 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2196314 |  424 | `	pEntry->pUserData = pUserData;` |
|   2196314 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2196314 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2196314 |  428 | `	return rc;` |
|   1098158 |  429 |  |
|    101620 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    101622 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
