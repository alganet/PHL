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
|  172196434 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  172196439 |   16 | `	pSet->nSize = 0 ;` |
|  172196439 |   17 | `	pSet->nUsed = 0;` |
|  172196439 |   18 | `	pSet->nCursor = 0;` |
|  172196439 |   19 | `	pSet->eSize = ElemSize;` |
|  172196439 |   20 | `	pSet->pAllocator = pAllocator;` |
|  172196439 |   21 | `	pSet->pBase =  0;` |
|  172196439 |   22 | `	pSet->pUserData = 0;` |
|  172196439 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  390018916 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  390018921 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   22480143 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   22480143 |   33 | `		if( pSet->nSize <= 0 ){` |
|   19146579 |   34 | `			pSet->nSize = 4;` |
|    9573962 |   35 | `		}` |
|   22480143 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   22480143 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   22480143 |   40 | `		pSet->pBase = pNew;` |
|   22480143 |   41 | `		pSet->nSize <<= 1;` |
|   11240744 |   42 | `	}` |
|  390018921 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3076742995 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  390018921 |   45 | `	pSet->nUsed++;` |
|  390018921 |   46 | `	return SXRET_OK;` |
|  195011925 |   47 | `}` |
|   19405038 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   19405043 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   19405043 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   19405043 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   19405043 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   19405043 |   60 | `	pSet->nSize = nItem;` |
|   19405043 |   61 | `	return SXRET_OK;` |
|    9702524 |   62 | `}` |
|   28888932 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   28888937 |   65 | `	pSet->nUsed   = 0;` |
|   28888937 |   66 | `	pSet->nCursor = 0;` |
|   28888937 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      70992 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      70997 |   71 | `	pSet->nCursor = 0;` |
|      70997 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      75180 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      75185 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30733 |   79 | `		pSet->nCursor = 0;` |
|      30733 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      44457 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      44457 |   83 | `	if( ppEntry ){` |
|      44457 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      22226 |   85 | `	}` |
|      44457 |   86 | `	pSet->nCursor++;` |
|      44457 |   87 | `	return SXRET_OK;` |
|      37595 |   88 | `}` |
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
|    3001316 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3001321 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1199 |  103 | `		pSet->nUsed = nNewSize;` |
|        597 |  104 | `	}` |
|    3001321 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   59308548 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   59308553 |  109 | `	sxi32 rc = SXRET_OK;` |
|   59308553 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   32076891 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   16039118 |  112 | `	}` |
|   59308553 |  113 | `	pSet->pBase = 0;` |
|   59308553 |  114 | `	pSet->nUsed = 0;` |
|   59308553 |  115 | `	pSet->nCursor = 0;` |
|   59308553 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   69396540 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   69396545 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19497 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   69377053 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   69377053 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   34698275 |  126 | `}` |
|    9688734 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9688739 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2235025 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7453719 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7453719 |  135 | `	pSet->nUsed--;` |
|    7453719 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7453719 |  137 | `	return pData;` |
|    4844822 |  138 | `}` |
|   35821724 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   35821729 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   35821677 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   35821677 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   17915040 |  148 | `}` |
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
|    1900446 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1900451 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1900451 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1900451 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1900451 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1900451 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1900451 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1900451 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1900451 |  180 | `	pHash->nEntry = 0;` |
|    1900451 |  181 | `	pHash->apBucket = apNew;` |
|    1900451 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1900451 |  183 | `	return SXRET_OK;` |
|     950303 |  184 | `}` |
|     441564 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     441569 |  193 | `	pEntry = pHash->pList;` |
|     235519 |  194 | `	for(;;){` |
|     470893 |  195 | `		if( pHash->nEntry == 0 ){` |
|     441569 |  196 | `			break;` |
|          - |  197 | `		}` |
|      29329 |  198 | `		pNext = pEntry->pNext;` |
|      29329 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      29329 |  200 | `		pEntry = pNext;` |
|      29329 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     441569 |  203 | `	if( pHash->apBucket ){` |
|     441569 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     220857 |  205 | `	}` |
|     441569 |  206 | `	pHash->apBucket = 0;` |
|     441569 |  207 | `	pHash->nBucketSize = 0;` |
|     441569 |  208 | `	pHash->pAllocator = 0;` |
|     441569 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   72513516 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   72513521 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   72513521 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   65554739 |  218 | `	for(;;){` |
|  130925206 |  219 | `		if( pEntry == 0 ){` |
|   26687949 |  220 | `			break;` |
|          - |  221 | `		}` |
|  127149153 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   45829614 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   45825577 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   58411690 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   26687949 |  229 | `	return 0;` |
|   36263408 |  230 | `}` |
|   79925168 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   79925173 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    7412089 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   72513089 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   72513089 |  244 | `	if( pEntry == 0 ){` |
|   26687931 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   45825163 |  247 | `	return (SyHashEntry *)pEntry;` |
|   39969309 |  248 | `}` |
|     487974 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     487979 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     399065 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     199910 |  254 | `	}else{` |
|      88919 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     487979 |  257 | `	if( pEntry->pNextCollide ){` |
|       4631 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2314 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     487979 |  261 | `	if( pHash->pLast == pEntry ){` |
|     477939 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     239417 |  263 | `	}` |
|     487979 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     487979 |  265 | `	pHash->nEntry--;` |
|     487979 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     487979 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     487979 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        432 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        437 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        437 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        419 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        419 |  288 | `	return rc;` |
|        221 |  289 | `}` |
|     487560 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     487565 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     487565 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     487565 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3101126 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3101131 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3101131 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   23847080 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   23847085 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3100865 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3100865 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   20746225 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   20746225 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   20746225 |  328 | `	return (SyHashEntry *)pEntry;` |
|   11923545 |  329 | `}` |
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
|       4557 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4543 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4543 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4543 |  348 | `		pEntry = pEntry->pNext;` |
|       2272 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|     100116 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100121 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100121 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100121 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100121 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18590681 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18490565 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18490565 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18490565 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18490565 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8860031 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4430495 |  375 | `		}` |
|   18490565 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18490565 |  378 | `		pEntry = pEntry->pNext;` |
|    9245285 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100121 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100121 |  382 | `	pHash->apBucket = apNew;` |
|     100121 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100121 |  384 | `	return SXRET_OK;` |
|      50063 |  385 | `}` |
|   20514958 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   20514963 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   20514963 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   20514963 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   13083388 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    6541604 |  393 | `	}` |
|   20514963 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   20514963 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   20514911 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   20514963 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1064107 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1064107 |  408 | `		pHash->pLast = pEntry;` |
|     532126 |  409 | `	}` |
|   20514963 |  410 | `	pHash->nEntry++;` |
|   20514963 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   20514958 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   20514963 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100121 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100121 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50058 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   20514963 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   20514963 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   20514963 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   20514963 |  435 | `	pEntry->pHash = pHash;` |
|   20514963 |  436 | `	pEntry->pKey = pKey;` |
|   20514963 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   20514963 |  438 | `	pEntry->pUserData = pUserData;` |
|   20514963 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   20514963 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   20514963 |  442 | `	return rc;` |
|   10257934 |  443 | `}` |
|   20514824 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   20514829 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     527932 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     527937 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
