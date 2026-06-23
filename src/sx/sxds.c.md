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
|  18367164 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 |  |
|  18367169 |   16 | `	pSet->nSize = 0 ;` |
|  18367169 |   17 | `	pSet->nUsed = 0;` |
|  18367169 |   18 | `	pSet->nCursor = 0;` |
|  18367169 |   19 | `	pSet->eSize = ElemSize;` |
|  18367169 |   20 | `	pSet->pAllocator = pAllocator;` |
|  18367169 |   21 | `	pSet->pBase =  0;` |
|  18367169 |   22 | `	pSet->pUserData = 0;` |
|  18367169 |   23 | `	return SXRET_OK;` |
|         5 |   24 |  |
|  30121805 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  30121810 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4446719 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4446719 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4298665 |   34 | `			pSet->nSize = 4;` |
|   2149330 |   35 | `		}` |
|   4446719 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4446719 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4446719 |   40 | `		pSet->pBase = pNew;` |
|   4446719 |   41 | `		pSet->nSize <<= 1;` |
|   2223357 |   42 | `	}` |
|  30121810 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 225354114 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  30121810 |   45 | `	pSet->nUsed++;` |
|  30121810 |   46 | `	return SXRET_OK;` |
|  15060950 |   47 |  |
|   1221938 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 |  |
|   1221943 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1221943 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1221943 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1221943 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1221943 |   60 | `	pSet->nSize = nItem;` |
|   1221943 |   61 | `	return SXRET_OK;` |
|    610974 |   62 |  |
|   1712865 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 |  |
|   1712870 |   65 | `	pSet->nUsed   = 0;` |
|   1712870 |   66 | `	pSet->nCursor = 0;` |
|   1712870 |   67 | `	return SXRET_OK;` |
|         5 |   68 |  |
|     55664 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 |  |
|     55669 |   71 | `	pSet->nCursor = 0;` |
|     55669 |   72 | `	return SXRET_OK;` |
|         5 |   73 |  |
|     59848 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     59853 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     22981 |   79 | `		pSet->nCursor = 0;` |
|     22981 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     36877 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     36877 |   83 | `	if( ppEntry ){` |
|     36877 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     18436 |   85 | `	}` |
|     36877 |   86 | `	pSet->nCursor++;` |
|     36877 |   87 | `	return SXRET_OK;` |
|     29929 |   88 |  |
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
|    206862 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 |  |
|    206867 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       118 |  103 | `		pSet->nUsed = nNewSize;` |
|        57 |  104 | `	}` |
|    206867 |  105 | `	return SXRET_OK;` |
|         5 |  106 |  |
|   9701542 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 |  |
|   9701547 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9701547 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4855409 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2427702 |  112 | `	}` |
|   9701547 |  113 | `	pSet->pBase = 0;` |
|   9701547 |  114 | `	pSet->nUsed = 0;` |
|   9701547 |  115 | `	pSet->nCursor = 0;` |
|   9701547 |  116 | `	return rc;` |
|         5 |  117 |  |
|   5523492 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5523497 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       121 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5523381 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5523381 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2761751 |  126 |  |
|   3530936 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3530941 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2175273 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1355673 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1355673 |  135 | `	pSet->nUsed--;` |
|   1355673 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1355673 |  137 | `	return pData;` |
|   1765473 |  138 |  |
|  13007955 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  13007960 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13007960 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13007960 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6504245 |  148 |  |
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
|    531110 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    531115 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    531115 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    531115 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    531115 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    531115 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    531115 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    531115 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    531115 |  180 | `	pHash->nEntry = 0;` |
|    531115 |  181 | `	pHash->apBucket = apNew;` |
|    531115 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    531115 |  183 | `	return SXRET_OK;` |
|    265560 |  184 |  |
|     97468 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     97473 |  193 | `	pEntry = pHash->pList;` |
|     52060 |  194 | `	for(;;){` |
|    104125 |  195 | `		if( pHash->nEntry == 0 ){` |
|     97473 |  196 | `			break;` |
|         - |  197 | `		}` |
|      6657 |  198 | `		pNext = pEntry->pNext;` |
|      6657 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      6657 |  200 | `		pEntry = pNext;` |
|      6657 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|     97473 |  203 | `	if( pHash->apBucket ){` |
|     97473 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     48734 |  205 | `	}` |
|     97473 |  206 | `	pHash->apBucket = 0;` |
|     97473 |  207 | `	pHash->nBucketSize = 0;` |
|     97473 |  208 | `	pHash->pAllocator = 0;` |
|     97473 |  209 | `	return SXRET_OK;` |
|         5 |  210 |  |
|  16611778 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  16611783 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  16611783 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  14748258 |  218 | `	for(;;){` |
|  29494354 |  219 | `		if( pEntry == 0 ){` |
|   8763871 |  220 | `			break;` |
|         - |  221 | `		}` |
|  24654191 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   7847916 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   7847917 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  12882576 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   8763871 |  229 | `	return 0;` |
|   8306404 |  230 |  |
|  17394322 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  17394327 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    782743 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  16611589 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  16611589 |  244 | `	if( pEntry == 0 ){` |
|   8763871 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   7847723 |  247 | `	return (SyHashEntry *)pEntry;` |
|   8697676 |  248 |  |
|    117554 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    117559 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     90317 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     45161 |  254 | `	}else{` |
|     27247 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    117559 |  257 | `	if( pEntry->pNextCollide ){` |
|      4981 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2490 |  259 | `	}` |
|    117559 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    117559 |  261 | `	pHash->nEntry--;` |
|    117559 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    117559 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    117559 |  268 | `	return rc;` |
|         5 |  269 |  |
|       194 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       199 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       199 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       199 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       199 |  284 | `	return rc;` |
|       102 |  285 |  |
|    117360 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  287 |  |
|    117365 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    117365 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    117365 |  296 | `	return rc;` |
|         5 |  297 |  |
|   1079290 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|   1079295 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   1079295 |  306 | `	return SXRET_OK;` |
|         5 |  307 |  |
|   6757020 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   6757025 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1078845 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   1078845 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   5678185 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   5678185 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   5678185 |  324 | `	return (SyHashEntry *)pEntry;` |
|   3378515 |  325 |  |
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
|      1933 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1923 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1923 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1923 |  344 | `		pEntry = pEntry->pNext;` |
|       962 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     27506 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  349 |  |
|     27511 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     27511 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     27511 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     27511 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3493591 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3466085 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3466085 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3466085 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3466085 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1665824 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    832995 |  371 | `		}` |
|   3466085 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3466085 |  374 | `		pEntry = pEntry->pNext;` |
|   1733045 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     27511 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     27511 |  378 | `	pHash->apBucket = apNew;` |
|     27511 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     27511 |  380 | `	return SXRET_OK;` |
|     13758 |  381 |  |
|   4535318 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         5 |  383 |  |
|   4535323 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   4535323 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4535323 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2528793 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1264377 |  389 | `	}` |
|   4535323 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   4535323 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   4535323 |  393 | `	if( pHash->nEntry == 0 ){` |
|    289855 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    144925 |  395 | `	}` |
|   4535323 |  396 | `	pHash->nEntry++;` |
|   4535323 |  397 | `	return SXRET_OK;` |
|         5 |  398 |  |
|   4535318 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   4535323 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     27511 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     27511 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     13753 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   4535323 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4535323 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   4535323 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4535323 |  421 | `	pEntry->pHash = pHash;` |
|   4535323 |  422 | `	pEntry->pKey = pKey;` |
|   4535323 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   4535323 |  424 | `	pEntry->pUserData = pUserData;` |
|   4535323 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4535323 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   4535323 |  428 | `	return rc;` |
|   2267664 |  429 |  |
|    151396 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    151401 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  439 |  |
|         - |  440 |  |
