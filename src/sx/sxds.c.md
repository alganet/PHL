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
|  18404544 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 |  |
|  18404549 |   16 | `	pSet->nSize = 0 ;` |
|  18404549 |   17 | `	pSet->nUsed = 0;` |
|  18404549 |   18 | `	pSet->nCursor = 0;` |
|  18404549 |   19 | `	pSet->eSize = ElemSize;` |
|  18404549 |   20 | `	pSet->pAllocator = pAllocator;` |
|  18404549 |   21 | `	pSet->pBase =  0;` |
|  18404549 |   22 | `	pSet->pUserData = 0;` |
|  18404549 |   23 | `	return SXRET_OK;` |
|         5 |   24 |  |
|  30167665 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  30167670 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4458253 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4458253 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4309971 |   34 | `			pSet->nSize = 4;` |
|   2154983 |   35 | `		}` |
|   4458253 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4458253 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4458253 |   40 | `		pSet->pBase = pNew;` |
|   4458253 |   41 | `		pSet->nSize <<= 1;` |
|   2229124 |   42 | `	}` |
|  30167670 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 225612246 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  30167670 |   45 | `	pSet->nUsed++;` |
|  30167670 |   46 | `	return SXRET_OK;` |
|  15083880 |   47 |  |
|   1223442 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 |  |
|   1223447 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1223447 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1223447 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1223447 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1223447 |   60 | `	pSet->nSize = nItem;` |
|   1223447 |   61 | `	return SXRET_OK;` |
|    611726 |   62 |  |
|   1716905 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 |  |
|   1716910 |   65 | `	pSet->nUsed   = 0;` |
|   1716910 |   66 | `	pSet->nCursor = 0;` |
|   1716910 |   67 | `	return SXRET_OK;` |
|         5 |   68 |  |
|     55878 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 |  |
|     55883 |   71 | `	pSet->nCursor = 0;` |
|     55883 |   72 | `	return SXRET_OK;` |
|         5 |   73 |  |
|     60084 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     60089 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     23077 |   79 | `		pSet->nCursor = 0;` |
|     23077 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     37017 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     37017 |   83 | `	if( ppEntry ){` |
|     37017 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     18506 |   85 | `	}` |
|     37017 |   86 | `	pSet->nCursor++;` |
|     37017 |   87 | `	return SXRET_OK;` |
|     30047 |   88 |  |
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
|    207008 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 |  |
|    207013 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       118 |  103 | `		pSet->nUsed = nNewSize;` |
|        57 |  104 | `	}` |
|    207013 |  105 | `	return SXRET_OK;` |
|         5 |  106 |  |
|   9724294 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 |  |
|   9724299 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9724299 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4867703 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2433849 |  112 | `	}` |
|   9724299 |  113 | `	pSet->pBase = 0;` |
|   9724299 |  114 | `	pSet->nUsed = 0;` |
|   9724299 |  115 | `	pSet->nCursor = 0;` |
|   9724299 |  116 | `	return rc;` |
|         5 |  117 |  |
|   5527762 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5527767 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       121 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5527651 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5527651 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2763886 |  126 |  |
|   3540438 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3540443 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2175373 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1365075 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1365075 |  135 | `	pSet->nUsed--;` |
|   1365075 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1365075 |  137 | `	return pData;` |
|   1770224 |  138 |  |
|  13055026 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  13055031 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13055031 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13055031 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6527804 |  148 |  |
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
|    531752 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    531757 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    531757 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    531757 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    531757 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    531757 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    531757 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    531757 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    531757 |  180 | `	pHash->nEntry = 0;` |
|    531757 |  181 | `	pHash->apBucket = apNew;` |
|    531757 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    531757 |  183 | `	return SXRET_OK;` |
|    265881 |  184 |  |
|     97848 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     97853 |  193 | `	pEntry = pHash->pList;` |
|     52250 |  194 | `	for(;;){` |
|    104505 |  195 | `		if( pHash->nEntry == 0 ){` |
|     97853 |  196 | `			break;` |
|         - |  197 | `		}` |
|      6657 |  198 | `		pNext = pEntry->pNext;` |
|      6657 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      6657 |  200 | `		pEntry = pNext;` |
|      6657 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|     97853 |  203 | `	if( pHash->apBucket ){` |
|     97853 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     48924 |  205 | `	}` |
|     97853 |  206 | `	pHash->apBucket = 0;` |
|     97853 |  207 | `	pHash->nBucketSize = 0;` |
|     97853 |  208 | `	pHash->pAllocator = 0;` |
|     97853 |  209 | `	return SXRET_OK;` |
|         5 |  210 |  |
|  16719770 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  16719775 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  16719775 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  14862294 |  218 | `	for(;;){` |
|  29747149 |  219 | `		if( pEntry == 0 ){` |
|   8858013 |  220 | `			break;` |
|         - |  221 | `		}` |
|  24819769 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   7861766 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   7861767 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  13027379 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   8858013 |  229 | `	return 0;` |
|   8360400 |  230 |  |
|  17503600 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  17503605 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    784029 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  16719581 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  16719581 |  244 | `	if( pEntry == 0 ){` |
|   8858013 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   7861573 |  247 | `	return (SyHashEntry *)pEntry;` |
|   8752315 |  248 |  |
|    118072 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    118077 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     90733 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     45369 |  254 | `	}else{` |
|     27349 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    118077 |  257 | `	if( pEntry->pNextCollide ){` |
|      4983 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2491 |  259 | `	}` |
|    118077 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    118077 |  261 | `	pHash->nEntry--;` |
|    118077 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    118077 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    118077 |  268 | `	return rc;` |
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
|    117878 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  287 |  |
|    117883 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    117883 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    117883 |  296 | `	return rc;` |
|         5 |  297 |  |
|   1080132 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|   1080137 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   1080137 |  306 | `	return SXRET_OK;` |
|         5 |  307 |  |
|   6761852 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   6761857 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1079687 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   1079687 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   5682175 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   5682175 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   5682175 |  324 | `	return (SyHashEntry *)pEntry;` |
|   3380931 |  325 |  |
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
|      1991 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1981 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1981 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1981 |  344 | `		pEntry = pEntry->pNext;` |
|       991 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     27524 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  349 |  |
|     27529 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     27529 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     27529 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     27529 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3495913 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3468389 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3468389 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3468389 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3468389 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1663704 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    831923 |  371 | `		}` |
|   3468389 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3468389 |  374 | `		pEntry = pEntry->pNext;` |
|   1734197 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     27529 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     27529 |  378 | `	pHash->apBucket = apNew;` |
|     27529 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     27529 |  380 | `	return SXRET_OK;` |
|     13767 |  381 |  |
|   4618802 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         5 |  383 |  |
|   4618807 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   4618807 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4618807 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2604947 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1302420 |  389 | `	}` |
|   4618807 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   4618807 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   4618807 |  393 | `	if( pHash->nEntry == 0 ){` |
|    290285 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    145140 |  395 | `	}` |
|   4618807 |  396 | `	pHash->nEntry++;` |
|   4618807 |  397 | `	return SXRET_OK;` |
|         5 |  398 |  |
|   4618802 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   4618807 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     27529 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     27529 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     13762 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   4618807 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4618807 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   4618807 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4618807 |  421 | `	pEntry->pHash = pHash;` |
|   4618807 |  422 | `	pEntry->pKey = pKey;` |
|   4618807 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   4618807 |  424 | `	pEntry->pUserData = pUserData;` |
|   4618807 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4618807 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   4618807 |  428 | `	return rc;` |
|   2309406 |  429 |  |
|    151938 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    151943 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  439 |  |
|         - |  440 |  |
