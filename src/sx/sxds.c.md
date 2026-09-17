# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 286/304 lines (94.08%)

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
|  185129610 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  185129615 |   16 | `	pSet->nSize = 0 ;` |
|  185129615 |   17 | `	pSet->nUsed = 0;` |
|  185129615 |   18 | `	pSet->nCursor = 0;` |
|  185129615 |   19 | `	pSet->eSize = ElemSize;` |
|  185129615 |   20 | `	pSet->pAllocator = pAllocator;` |
|  185129615 |   21 | `	pSet->pBase =  0;` |
|  185129615 |   22 | `	pSet->pUserData = 0;` |
|  185129615 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  419561848 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  419561853 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   24239466 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   24239466 |   33 | `		if( pSet->nSize <= 0 ){` |
|   20657196 |   34 | `			pSet->nSize = 4;` |
|   10329522 |   35 | `		}` |
|   24239466 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   24239466 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   24239466 |   40 | `		pSet->pBase = pNew;` |
|   24239466 |   41 | `		pSet->nSize <<= 1;` |
|   12120657 |   42 | `	}` |
|  419561853 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3312322615 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  419561853 |   45 | `	pSet->nUsed++;` |
|  419561853 |   46 | `	return SXRET_OK;` |
|  209784074 |   47 | `}` |
|   20853216 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   20853221 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   20853221 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   20853221 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   20853221 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   20853221 |   60 | `	pSet->nSize = nItem;` |
|   20853221 |   61 | `	return SXRET_OK;` |
|   10426613 |   62 | `}` |
|   30876271 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   30876276 |   65 | `	pSet->nUsed   = 0;` |
|   30876276 |   66 | `	pSet->nCursor = 0;` |
|   30876276 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69244 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69249 |   71 | `	pSet->nCursor = 0;` |
|      69249 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      69504 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      69509 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30153 |   79 | `		pSet->nCursor = 0;` |
|      30153 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      39361 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      39361 |   83 | `	if( ppEntry ){` |
|      39361 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      19678 |   85 | `	}` |
|      39361 |   86 | `	pSet->nCursor++;` |
|      39361 |   87 | `	return SXRET_OK;` |
|      34757 |   88 | `}` |
|          - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        ! 0 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|        ! 0 |   91 | `{` |
|          - |   92 | `	register unsigned char *zSrc;` |
|        ! 0 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|        ! 0 |   94 | `		return 0;` |
|          - |   95 | `	}` |
|        ! 0 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|        ! 0 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|        ! 0 |   98 | `}` |
|          - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    3295058 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3295063 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1253 |  103 | `		pSet->nUsed = nNewSize;` |
|        624 |  104 | `	}` |
|    3295063 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   63322116 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   63322121 |  109 | `	sxi32 rc = SXRET_OK;` |
|   63322121 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   34276492 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   17139170 |  112 | `	}` |
|   63322121 |  113 | `	pSet->pBase = 0;` |
|   63322121 |  114 | `	pSet->nUsed = 0;` |
|   63322121 |  115 | `	pSet->nCursor = 0;` |
|   63322121 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   74559714 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   74559719 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       4021 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   74555703 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   74555703 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   37279862 |  126 | `}` |
|    9882825 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9882830 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2238009 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7644826 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7644826 |  135 | `	pSet->nUsed--;` |
|    7644826 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7644826 |  137 | `	return pData;` |
|    4942035 |  138 | `}` |
|   37310854 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   37310859 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   37310807 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   37310807 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   18659545 |  148 | `}` |
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
|    2192438 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    2192443 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2192443 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    2192443 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2192443 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    2192443 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    2192443 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    2192443 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    2192443 |  180 | `	pHash->nEntry = 0;` |
|    2192443 |  181 | `	pHash->apBucket = apNew;` |
|    2192443 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    2192443 |  183 | `	return SXRET_OK;` |
|    1096327 |  184 | `}` |
|     522100 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     522105 |  193 | `	pEntry = pHash->pList;` |
|     276686 |  194 | `	for(;;){` |
|     553171 |  195 | `		if( pHash->nEntry == 0 ){` |
|     522105 |  196 | `			break;` |
|          - |  197 | `		}` |
|      31071 |  198 | `		pNext = pEntry->pNext;` |
|      31071 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      31071 |  200 | `		pEntry = pNext;` |
|      31071 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     522105 |  203 | `	if( pHash->apBucket ){` |
|     522105 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     261153 |  205 | `	}` |
|     522105 |  206 | `	pHash->apBucket = 0;` |
|     522105 |  207 | `	pHash->nBucketSize = 0;` |
|     522105 |  208 | `	pHash->pAllocator = 0;` |
|     522105 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   79646954 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   79646959 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   79646959 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   71257476 |  218 | `	for(;;){` |
|  142883489 |  219 | `		if( pEntry == 0 ){` |
|   31622116 |  220 | `			break;` |
|          - |  221 | `		}` |
|  135273269 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   48028863 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   48024848 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   63236535 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   31622116 |  229 | `	return 0;` |
|   39829603 |  230 | `}` |
|   89603674 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   89603679 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    9957183 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   79646501 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   79646501 |  244 | `	if( pEntry == 0 ){` |
|   31622098 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   48024408 |  247 | `	return (SyHashEntry *)pEntry;` |
|   44808066 |  248 | `}` |
|     510090 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     510095 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     415178 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     208106 |  254 | `	}else{` |
|      94922 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     510095 |  257 | `	if( pEntry->pNextCollide ){` |
|       4400 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2197 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     510095 |  261 | `	if( pHash->pLast == pEntry ){` |
|     500543 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     250887 |  263 | `	}` |
|     510095 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     510095 |  265 | `	pHash->nEntry--;` |
|     510095 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     510095 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     510095 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        458 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        463 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        463 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        445 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        445 |  288 | `	return rc;` |
|        234 |  289 | `}` |
|     509650 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     509655 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     509655 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     509655 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3435700 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3435705 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3435705 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   26463508 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   26463513 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3435439 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3435439 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   23028079 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   23028079 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   23028079 |  328 | `	return (SyHashEntry *)pEntry;` |
|   13231759 |  329 | `}` |
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
|       4857 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4843 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4843 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4843 |  348 | `		pEntry = pEntry->pNext;` |
|       2422 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|     100916 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100921 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100921 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100921 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100921 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18673369 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18572453 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18572453 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18572453 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18572453 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8960313 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4480552 |  375 | `		}` |
|   18572453 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18572453 |  378 | `		pEntry = pEntry->pNext;` |
|    9286229 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100921 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100921 |  382 | `	pHash->apBucket = apNew;` |
|     100921 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100921 |  384 | `	return SXRET_OK;` |
|      50463 |  385 | `}` |
|   22645120 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   22645125 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   22645125 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   22645125 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   14221545 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    7110940 |  393 | `	}` |
|   22645125 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   22645125 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|     883927 |  399 | `		pHash->pLast->pNext = pEntry;` |
|     883927 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|     883927 |  401 | `		pHash->pLast = pEntry;` |
|     441966 |  402 | `	}else{` |
|   21761203 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   22645125 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1217139 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1217139 |  408 | `		pHash->pLast = pEntry;` |
|     608670 |  409 | `	}` |
|   22645125 |  410 | `	pHash->nEntry++;` |
|   22645125 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   22645120 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   22645125 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100921 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100921 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50458 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   22645125 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   22645125 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   22645125 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   22645125 |  435 | `	pEntry->pHash = pHash;` |
|   22645125 |  436 | `	pEntry->pKey = pKey;` |
|   22645125 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   22645125 |  438 | `	pEntry->pUserData = pUserData;` |
|   22645125 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   22645125 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   22645125 |  442 | `	return rc;` |
|   11323183 |  443 | `}` |
|   21498774 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   21498779 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|    1146346 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  455 | `{` |
|    1146351 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          5 |  457 | `}` |
|     550122 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     550127 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
