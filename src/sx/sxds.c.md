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
|  14557988 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  14557990 |   16 | `	pSet->nSize = 0 ;` |
|  14557990 |   17 | `	pSet->nUsed = 0;` |
|  14557990 |   18 | `	pSet->nCursor = 0;` |
|  14557990 |   19 | `	pSet->eSize = ElemSize;` |
|  14557990 |   20 | `	pSet->pAllocator = pAllocator;` |
|  14557990 |   21 | `	pSet->pBase =  0;` |
|  14557990 |   22 | `	pSet->pUserData = 0;` |
|  14557990 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  24130636 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  24130638 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3938200 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3938200 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3826048 |   34 | `			pSet->nSize = 4;` |
|   1913023 |   35 | `		}` |
|   3938200 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3938200 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3938200 |   40 | `		pSet->pBase = pNew;` |
|   3938200 |   41 | `		pSet->nSize <<= 1;` |
|   1969099 |   42 | `	}` |
|  24130638 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 179002962 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  24130638 |   45 | `	pSet->nUsed++;` |
|  24130638 |   46 | `	return SXRET_OK;` |
|  12065342 |   47 |  |
|    883694 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    883696 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    883696 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    883696 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    883696 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    883696 |   60 | `	pSet->nSize = nItem;` |
|    883696 |   61 | `	return SXRET_OK;` |
|    441849 |   62 |  |
|   1355860 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1355862 |   65 | `	pSet->nUsed   = 0;` |
|   1355862 |   66 | `	pSet->nCursor = 0;` |
|   1355862 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     47104 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     47106 |   71 | `	pSet->nCursor = 0;` |
|     47106 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     51186 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     51188 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     19338 |   79 | `		pSet->nCursor = 0;` |
|     19338 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     31852 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     31852 |   83 | `	if( ppEntry ){` |
|     31852 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     15925 |   85 | `	}` |
|     31852 |   86 | `	pSet->nCursor++;` |
|     31852 |   87 | `	return SXRET_OK;` |
|     25595 |   88 |  |
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
|    146798 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    146800 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|    146800 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8525474 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8525476 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8525476 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4351644 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2175821 |  112 | `	}` |
|   8525476 |  113 | `	pSet->pBase = 0;` |
|   8525476 |  114 | `	pSet->nUsed = 0;` |
|   8525476 |  115 | `	pSet->nCursor = 0;` |
|   8525476 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4654462 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4654464 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       108 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4654358 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4654358 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2327233 |  126 |  |
|   3307836 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3307838 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2145072 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1162768 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1162768 |  135 | `	pSet->nUsed--;` |
|   1162768 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1162768 |  137 | `	return pData;` |
|   1653920 |  138 |  |
|  11120051 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  11120053 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  11120053 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  11120053 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5560229 |  148 |  |
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
|    265858 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    265860 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    265860 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    265860 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    265860 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    265860 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    265860 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    265860 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    265860 |  180 | `	pHash->nEntry = 0;` |
|    265860 |  181 | `	pHash->apBucket = apNew;` |
|    265860 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    265860 |  183 | `	return SXRET_OK;` |
|    132931 |  184 |  |
|     79290 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     79292 |  193 | `	pEntry = pHash->pList;` |
|     41677 |  194 | `	for(;;){` |
|     83356 |  195 | `		if( pHash->nEntry == 0 ){` |
|     79292 |  196 | `			break;` |
|         - |  197 | `		}` |
|      4066 |  198 | `		pNext = pEntry->pNext;` |
|      4066 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      4066 |  200 | `		pEntry = pNext;` |
|      4066 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     79292 |  203 | `	if( pHash->apBucket ){` |
|     79292 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     39645 |  205 | `	}` |
|     79292 |  206 | `	pHash->apBucket = 0;` |
|     79292 |  207 | `	pHash->nBucketSize = 0;` |
|     79292 |  208 | `	pHash->pAllocator = 0;` |
|     79292 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  12171248 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  12171250 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  12171250 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  11002191 |  218 | `	for(;;){` |
|  21997955 |  219 | `		if( pEntry == 0 ){` |
|   6712986 |  220 | `			break;` |
|         - |  221 | `		}` |
|  18013973 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5458268 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5458266 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   9826707 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   6712986 |  229 | `	return 0;` |
|   6085890 |  230 |  |
|  12658752 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  12658754 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    487604 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  12171152 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  12171152 |  244 | `	if( pEntry == 0 ){` |
|   6712986 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5458168 |  247 | `	return (SyHashEntry *)pEntry;` |
|   6329642 |  248 |  |
|     91502 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     91504 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     69504 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     34753 |  254 | `	}else{` |
|     22002 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     91504 |  257 | `	if( pEntry->pNextCollide ){` |
|      4711 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2355 |  259 | `	}` |
|     91504 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     91504 |  261 | `	pHash->nEntry--;` |
|     91504 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     91504 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     91504 |  268 | `	return rc;` |
|         2 |  269 |  |
|        98 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       100 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       100 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       100 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       100 |  284 | `	return rc;` |
|        51 |  285 |  |
|     91404 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     91406 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     91406 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     91406 |  296 | `	return rc;` |
|         2 |  297 |  |
|    317928 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    317930 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    317930 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2511632 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2511634 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    317496 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    317496 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2194140 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2194140 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2194140 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1255818 |  325 |  |
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
|      1773 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1763 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1763 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1763 |  344 | `		pEntry = pEntry->pNext;` |
|       882 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     22988 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     22990 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     22990 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     22990 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     22990 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2919790 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2896802 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2896802 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2896802 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2896802 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1384145 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    692011 |  371 | `		}` |
|   2896802 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2896802 |  374 | `		pEntry = pEntry->pNext;` |
|   1448402 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     22990 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     22990 |  378 | `	pHash->apBucket = apNew;` |
|     22990 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     22990 |  380 | `	return SXRET_OK;` |
|     11496 |  381 |  |
|   2968110 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2968112 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2968112 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2968112 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1922640 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    961281 |  389 | `	}` |
|   2968112 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2968112 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2968112 |  393 | `	if( pHash->nEntry == 0 ){` |
|    132452 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     66225 |  395 | `	}` |
|   2968112 |  396 | `	pHash->nEntry++;` |
|   2968112 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2968110 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2968112 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     22990 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     22990 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     11494 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2968112 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2968112 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2968112 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2968112 |  421 | `	pEntry->pHash = pHash;` |
|   2968112 |  422 | `	pEntry->pKey = pKey;` |
|   2968112 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2968112 |  424 | `	pEntry->pUserData = pUserData;` |
|   2968112 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2968112 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2968112 |  428 | `	return rc;` |
|   1484057 |  429 |  |
|    117208 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    117210 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
