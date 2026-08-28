# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 290/304 lines (95.39%)

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
|  112804738 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  112804743 |   16 | `	pSet->nSize = 0 ;` |
|  112804743 |   17 | `	pSet->nUsed = 0;` |
|  112804743 |   18 | `	pSet->nCursor = 0;` |
|  112804743 |   19 | `	pSet->eSize = ElemSize;` |
|  112804743 |   20 | `	pSet->pAllocator = pAllocator;` |
|  112804743 |   21 | `	pSet->pBase =  0;` |
|  112804743 |   22 | `	pSet->pUserData = 0;` |
|  112804743 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  248891553 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  248891558 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   15176441 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   15176441 |   33 | `		if( pSet->nSize <= 0 ){` |
|   13100273 |   34 | `			pSet->nSize = 4;` |
|    6550134 |   35 | `		}` |
|   15176441 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   15176441 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   15176441 |   40 | `		pSet->pBase = pNew;` |
|   15176441 |   41 | `		pSet->nSize <<= 1;` |
|    7588218 |   42 | `	}` |
|  248891558 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 1845172262 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  248891558 |   45 | `	pSet->nUsed++;` |
|  248891558 |   46 | `	return SXRET_OK;` |
|  124445824 |   47 | `}` |
|   12054312 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   12054317 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   12054317 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   12054317 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   12054317 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   12054317 |   60 | `	pSet->nSize = nItem;` |
|   12054317 |   61 | `	return SXRET_OK;` |
|    6027161 |   62 | `}` |
|   18040819 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   18040824 |   65 | `	pSet->nUsed   = 0;` |
|   18040824 |   66 | `	pSet->nCursor = 0;` |
|   18040824 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69246 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69251 |   71 | `	pSet->nCursor = 0;` |
|      69251 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73484 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73489 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29805 |   79 | `		pSet->nCursor = 0;` |
|      29805 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43689 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43689 |   83 | `	if( ppEntry ){` |
|      43689 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21842 |   85 | `	}` |
|      43689 |   86 | `	pSet->nCursor++;` |
|      43689 |   87 | `	return SXRET_OK;` |
|      36747 |   88 | `}` |
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
|    1996954 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    1996959 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1179 |  103 | `		pSet->nUsed = nNewSize;` |
|        587 |  104 | `	}` |
|    1996959 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   39192952 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   39192957 |  109 | `	sxi32 rc = SXRET_OK;` |
|   39192957 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   20803747 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   10401871 |  112 | `	}` |
|   39192957 |  113 | `	pSet->pBase = 0;` |
|   39192957 |  114 | `	pSet->nUsed = 0;` |
|   39192957 |  115 | `	pSet->nCursor = 0;` |
|   39192957 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   44540288 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   44540293 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15621 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   44524677 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   44524677 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   22270149 |  126 | `}` |
|    6854008 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    6854013 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2198975 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    4655043 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    4655043 |  135 | `	pSet->nUsed--;` |
|    4655043 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    4655043 |  137 | `	return pData;` |
|    3427009 |  138 | `}` |
|   24717161 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   24717166 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   24717144 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   24717144 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   12358935 |  148 | `}` |
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
|    1510200 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1510205 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1510205 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1510205 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1510205 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1510205 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1510205 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1510205 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1510205 |  180 | `	pHash->nEntry = 0;` |
|    1510205 |  181 | `	pHash->apBucket = apNew;` |
|    1510205 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1510205 |  183 | `	return SXRET_OK;` |
|     755105 |  184 | `}` |
|     346730 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     346735 |  193 | `	pEntry = pHash->pList;` |
|     185601 |  194 | `	for(;;){` |
|     371207 |  195 | `		if( pHash->nEntry == 0 ){` |
|     346735 |  196 | `			break;` |
|          - |  197 | `		}` |
|      24477 |  198 | `		pNext = pEntry->pNext;` |
|      24477 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      24477 |  200 | `		pEntry = pNext;` |
|      24477 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     346735 |  203 | `	if( pHash->apBucket ){` |
|     346735 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     173365 |  205 | `	}` |
|     346735 |  206 | `	pHash->apBucket = 0;` |
|     346735 |  207 | `	pHash->nBucketSize = 0;` |
|     346735 |  208 | `	pHash->pAllocator = 0;` |
|     346735 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   50573585 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   50573590 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   50573590 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   47690001 |  218 | `	for(;;){` |
|   95151047 |  219 | `		if( pEntry == 0 ){` |
|   19394682 |  220 | `			break;` |
|          - |  221 | `		}` |
|   91345601 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   31178972 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   31178913 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   44577462 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   19394682 |  229 | `	return 0;` |
|   25287309 |  230 | `}` |
|   55286543 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   55286548 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    4713285 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   50573268 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   50573268 |  244 | `	if( pEntry == 0 ){` |
|   19394682 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   31178591 |  247 | `	return (SyHashEntry *)pEntry;` |
|   27643788 |  248 | `}` |
|     223352 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     223357 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     179717 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      89861 |  254 | `	}else{` |
|      43645 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     223357 |  257 | `	if( pEntry->pNextCollide ){` |
|       4292 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2145 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     223357 |  261 | `	if( pHash->pLast == pEntry ){` |
|     216441 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     108218 |  263 | `	}` |
|     223357 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     223357 |  265 | `	pHash->nEntry--;` |
|     223357 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|        ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        ! 0 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     223357 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     223357 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        322 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        327 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        327 |  284 | `	if( pEntry == 0 ){` |
|        ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        327 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        327 |  288 | `	return rc;` |
|        166 |  289 | `}` |
|     223030 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     223035 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     223035 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     223035 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2417862 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2417867 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2417867 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   18018872 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   18018877 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2417601 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2417601 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   15601281 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   15601281 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   15601281 |  328 | `	return (SyHashEntry *)pEntry;` |
|    9009441 |  329 | `}` |
|         10 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|          1 |  331 | `{` |
|          - |  332 | `	SyHashEntry_Pr *pEntry;` |
|          - |  333 | `	sxi32 rc;` |
|          - |  334 | `	sxu32 n;` |
|          - |  335 | `#if defined(UNTRUST)` |
|          - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|          - |  337 | `		return 0;` |
|          - |  338 | `	}` |
|          - |  339 | `#endif` |
|         11 |  340 | `	pEntry = pHash->pList;` |
|       3457 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3447 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3447 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3447 |  348 | `		pEntry = pEntry->pNext;` |
|       1724 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      85744 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      85749 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      85749 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      85749 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      85749 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   11367669 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   11281925 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   11281925 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   11281925 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   11281925 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    5381385 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    2691355 |  375 | `		}` |
|   11281925 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   11281925 |  378 | `		pEntry = pEntry->pNext;` |
|    5640965 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      85749 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      85749 |  382 | `	pHash->apBucket = apNew;` |
|      85749 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      85749 |  384 | `	return SXRET_OK;` |
|      42877 |  385 | `}` |
|   14875030 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   14875035 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   14875035 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   14875035 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    9248192 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    4623670 |  393 | `	}` |
|   14875035 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   14875035 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   14874983 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   14875035 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     818235 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     818235 |  408 | `		pHash->pLast = pEntry;` |
|     409115 |  409 | `	}` |
|   14875035 |  410 | `	pHash->nEntry++;` |
|   14875035 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   14875030 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   14875035 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      85749 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      85749 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      42872 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   14875035 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   14875035 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   14875035 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   14875035 |  435 | `	pEntry->pHash = pHash;` |
|   14875035 |  436 | `	pEntry->pKey = pKey;` |
|   14875035 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   14875035 |  438 | `	pEntry->pUserData = pUserData;` |
|   14875035 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   14875035 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   14875035 |  442 | `	return rc;` |
|    7437520 |  443 | `}` |
|   14874902 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   14874907 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        128 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          2 |  455 | `{` |
|        130 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          2 |  457 | `}` |
|     264006 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     264011 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
