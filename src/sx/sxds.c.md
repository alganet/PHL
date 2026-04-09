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
|  14164748 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  14164750 |   16 | `	pSet->nSize = 0 ;` |
|  14164750 |   17 | `	pSet->nUsed = 0;` |
|  14164750 |   18 | `	pSet->nCursor = 0;` |
|  14164750 |   19 | `	pSet->eSize = ElemSize;` |
|  14164750 |   20 | `	pSet->pAllocator = pAllocator;` |
|  14164750 |   21 | `	pSet->pBase =  0;` |
|  14164750 |   22 | `	pSet->pUserData = 0;` |
|  14164750 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  23409020 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  23409022 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3884234 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3884234 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3776994 |   34 | `			pSet->nSize = 4;` |
|   1888496 |   35 | `		}` |
|   3884234 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3884234 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3884234 |   40 | `		pSet->pBase = pNew;` |
|   3884234 |   41 | `		pSet->nSize <<= 1;` |
|   1942116 |   42 | `	}` |
|  23409022 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 173865862 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  23409022 |   45 | `	pSet->nUsed++;` |
|  23409022 |   46 | `	return SXRET_OK;` |
|  11704534 |   47 |  |
|    845042 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    845044 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    845044 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    845044 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    845044 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    845044 |   60 | `	pSet->nSize = nItem;` |
|    845044 |   61 | `	return SXRET_OK;` |
|    422523 |   62 |  |
|   1301826 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1301828 |   65 | `	pSet->nUsed   = 0;` |
|   1301828 |   66 | `	pSet->nCursor = 0;` |
|   1301828 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     45198 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     45200 |   71 | `	pSet->nCursor = 0;` |
|     45200 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     49280 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     49282 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     18526 |   79 | `		pSet->nCursor = 0;` |
|     18526 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     30758 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     30758 |   83 | `	if( ppEntry ){` |
|     30758 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     15378 |   85 | `	}` |
|     30758 |   86 | `	pSet->nCursor++;` |
|     30758 |   87 | `	return SXRET_OK;` |
|     24642 |   88 |  |
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
|    140408 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|    140410 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|    140410 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   8388844 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   8388846 |  109 | `	sxi32 rc = SXRET_OK;` |
|   8388846 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4280032 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2140015 |  112 | `	}` |
|   8388846 |  113 | `	pSet->pBase = 0;` |
|   8388846 |  114 | `	pSet->nUsed = 0;` |
|   8388846 |  115 | `	pSet->nCursor = 0;` |
|   8388846 |  116 | `	return rc;` |
|         2 |  117 |  |
|   4541488 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   4541490 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       106 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   4541386 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   4541386 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2270746 |  126 |  |
|   3281348 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3281350 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2142904 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1138448 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1138448 |  135 | `	pSet->nUsed--;` |
|   1138448 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1138448 |  137 | `	return pData;` |
|   1640676 |  138 |  |
|  10867444 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  10867446 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  10867446 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  10867446 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   5433857 |  148 |  |
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
|    251588 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    251590 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    251590 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    251590 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    251590 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    251590 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    251590 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    251590 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    251590 |  180 | `	pHash->nEntry = 0;` |
|    251590 |  181 | `	pHash->apBucket = apNew;` |
|    251590 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    251590 |  183 | `	return SXRET_OK;` |
|    125796 |  184 |  |
|     75992 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     75994 |  193 | `	pEntry = pHash->pList;` |
|     39887 |  194 | `	for(;;){` |
|     79776 |  195 | `		if( pHash->nEntry == 0 ){` |
|     75994 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3784 |  198 | `		pNext = pEntry->pNext;` |
|      3784 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3784 |  200 | `		pEntry = pNext;` |
|      3784 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     75994 |  203 | `	if( pHash->apBucket ){` |
|     75994 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     37996 |  205 | `	}` |
|     75994 |  206 | `	pHash->apBucket = 0;` |
|     75994 |  207 | `	pHash->nBucketSize = 0;` |
|     75994 |  208 | `	pHash->pAllocator = 0;` |
|     75994 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  11729968 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  11729970 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  11729970 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  10589334 |  218 | `	for(;;){` |
|  21358143 |  219 | `		if( pEntry == 0 ){` |
|   6463784 |  220 | `			break;` |
|         - |  221 | `		}` |
|  17527324 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   5266190 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   5266188 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   9628175 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   6463784 |  229 | `	return 0;` |
|   5865250 |  230 |  |
|  12185356 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  12185358 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    455412 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  11729948 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  11729948 |  244 | `	if( pEntry == 0 ){` |
|   6463784 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   5266166 |  247 | `	return (SyHashEntry *)pEntry;` |
|   6092944 |  248 |  |
|     87282 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     87284 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     66270 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     33136 |  254 | `	}else{` |
|     21016 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     87284 |  257 | `	if( pEntry->pNextCollide ){` |
|      4535 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2267 |  259 | `	}` |
|     87284 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     87284 |  261 | `	pHash->nEntry--;` |
|     87284 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     87284 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     87284 |  268 | `	return rc;` |
|         2 |  269 |  |
|        22 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         2 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|        24 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        24 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|        24 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        24 |  284 | `	return rc;` |
|        13 |  285 |  |
|     87260 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     87262 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     87262 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     87262 |  296 | `	return rc;` |
|         2 |  297 |  |
|    303070 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    303072 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    303072 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   2376388 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   2376390 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    302638 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    302638 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   2073754 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   2073754 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   2073754 |  324 | `	return (SyHashEntry *)pEntry;` |
|   1188196 |  325 |  |
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
|     21962 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     21964 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     21964 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     21964 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     21964 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2788780 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2766818 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2766818 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2766818 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2766818 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1322210 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    661138 |  371 | `		}` |
|   2766818 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2766818 |  374 | `		pEntry = pEntry->pNext;` |
|   1383410 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     21964 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     21964 |  378 | `	pHash->apBucket = apNew;` |
|     21964 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     21964 |  380 | `	return SXRET_OK;` |
|     10983 |  381 |  |
|   2835332 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   2835334 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   2835334 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   2835334 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1836213 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    918096 |  389 | `	}` |
|   2835334 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   2835334 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   2835334 |  393 | `	if( pHash->nEntry == 0 ){` |
|    126398 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     63198 |  395 | `	}` |
|   2835334 |  396 | `	pHash->nEntry++;` |
|   2835334 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   2835332 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   2835334 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     21964 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     21964 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     10981 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   2835334 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   2835334 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   2835334 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   2835334 |  421 | `	pEntry->pHash = pHash;` |
|   2835334 |  422 | `	pEntry->pKey = pKey;` |
|   2835334 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   2835334 |  424 | `	pEntry->pUserData = pUserData;` |
|   2835334 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   2835334 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   2835334 |  428 | `	return rc;` |
|   1417668 |  429 |  |
|    111912 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    111914 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
