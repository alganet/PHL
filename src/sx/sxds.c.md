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
|  184255826 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  184255831 |   16 | `	pSet->nSize = 0 ;` |
|  184255831 |   17 | `	pSet->nUsed = 0;` |
|  184255831 |   18 | `	pSet->nCursor = 0;` |
|  184255831 |   19 | `	pSet->eSize = ElemSize;` |
|  184255831 |   20 | `	pSet->pAllocator = pAllocator;` |
|  184255831 |   21 | `	pSet->pBase =  0;` |
|  184255831 |   22 | `	pSet->pUserData = 0;` |
|  184255831 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  416479127 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  416479132 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   23898242 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   23898242 |   33 | `		if( pSet->nSize <= 0 ){` |
|   20340346 |   34 | `			pSet->nSize = 4;` |
|   10170845 |   35 | `		}` |
|   23898242 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   23898242 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   23898242 |   40 | `		pSet->pBase = pNew;` |
|   23898242 |   41 | `		pSet->nSize <<= 1;` |
|   11949793 |   42 | `	}` |
|  416479132 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3292783134 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  416479132 |   45 | `	pSet->nUsed++;` |
|  416479132 |   46 | `	return SXRET_OK;` |
|  208242009 |   47 | `}` |
|   20702236 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   20702241 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   20702241 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   20702241 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   20702241 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   20702241 |   60 | `	pSet->nSize = nItem;` |
|   20702241 |   61 | `	return SXRET_OK;` |
|   10351123 |   62 | `}` |
|   30742954 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   30742959 |   65 | `	pSet->nUsed   = 0;` |
|   30742959 |   66 | `	pSet->nCursor = 0;` |
|   30742959 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      70208 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      70213 |   71 | `	pSet->nCursor = 0;` |
|      70213 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      70456 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      70461 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30449 |   79 | `		pSet->nCursor = 0;` |
|      30449 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      40017 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      40017 |   83 | `	if( ppEntry ){` |
|      40017 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      20006 |   85 | `	}` |
|      40017 |   86 | `	pSet->nCursor++;` |
|      40017 |   87 | `	return SXRET_OK;` |
|      35233 |   88 | `}` |
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
|    3283154 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3283159 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1213 |  103 | `		pSet->nUsed = nNewSize;` |
|        604 |  104 | `	}` |
|    3283159 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   62777670 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   62777675 |  109 | `	sxi32 rc = SXRET_OK;` |
|   62777675 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   33847046 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   16924195 |  112 | `	}` |
|   62777675 |  113 | `	pSet->pBase = 0;` |
|   62777675 |  114 | `	pSet->nUsed = 0;` |
|   62777675 |  115 | `	pSet->nCursor = 0;` |
|   62777675 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   74184986 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   74184991 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      19539 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   74165457 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   74165457 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   37092498 |  126 | `}` |
|    9868337 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9868342 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2236951 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7631396 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7631396 |  135 | `	pSet->nUsed--;` |
|    7631396 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7631396 |  137 | `	return pData;` |
|    4934623 |  138 | `}` |
|   37210885 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   37210890 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   37210838 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   37210838 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   18609443 |  148 | `}` |
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
|    2145552 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    2145557 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2145557 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    2145557 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2145557 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    2145557 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    2145557 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    2145557 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    2145557 |  180 | `	pHash->nEntry = 0;` |
|    2145557 |  181 | `	pHash->apBucket = apNew;` |
|    2145557 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    2145557 |  183 | `	return SXRET_OK;` |
|    1072856 |  184 | `}` |
|     520264 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     520269 |  193 | `	pEntry = pHash->pList;` |
|     275168 |  194 | `	for(;;){` |
|     550191 |  195 | `		if( pHash->nEntry == 0 ){` |
|     520269 |  196 | `			break;` |
|          - |  197 | `		}` |
|      29927 |  198 | `		pNext = pEntry->pNext;` |
|      29927 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      29927 |  200 | `		pEntry = pNext;` |
|      29927 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     520269 |  203 | `	if( pHash->apBucket ){` |
|     520269 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     260207 |  205 | `	}` |
|     520269 |  206 | `	pHash->apBucket = 0;` |
|     520269 |  207 | `	pHash->nBucketSize = 0;` |
|     520269 |  208 | `	pHash->pAllocator = 0;` |
|     520269 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   76201323 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   76201328 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   76201328 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   68724838 |  218 | `	for(;;){` |
|  137496303 |  219 | `		if( pEntry == 0 ){` |
|   28223722 |  220 | `			break;` |
|          - |  221 | `		}` |
|  133260595 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   47981610 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   47977611 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   61294980 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   28223722 |  229 | `	return 0;` |
|   38107052 |  230 | `}` |
|   84312121 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   84312126 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    8111243 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   76200888 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   76200888 |  244 | `	if( pEntry == 0 ){` |
|   28223704 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   47977189 |  247 | `	return (SyHashEntry *)pEntry;` |
|   42162526 |  248 | `}` |
|     490100 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     490105 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     401327 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     201041 |  254 | `	}else{` |
|      88783 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     490105 |  257 | `	if( pEntry->pNextCollide ){` |
|       4461 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2228 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     490105 |  261 | `	if( pHash->pLast == pEntry ){` |
|     480311 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     240603 |  263 | `	}` |
|     490105 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     490105 |  265 | `	pHash->nEntry--;` |
|     490105 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     490105 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     490105 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        440 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        445 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        445 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        427 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        427 |  288 | `	return rc;` |
|        225 |  289 | `}` |
|     489678 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     489683 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     489683 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     489683 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3353122 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3353127 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3353127 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   25996372 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   25996377 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3352861 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3352861 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   22643521 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   22643521 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   22643521 |  328 | `	return (SyHashEntry *)pEntry;` |
|   12998191 |  329 | `}` |
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
|       4873 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4859 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4859 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4859 |  348 | `		pEntry = pEntry->pNext;` |
|       2430 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|     100788 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100793 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100793 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100793 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100793 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18675833 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18575045 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18575045 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18575045 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18575045 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8969479 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4484671 |  375 | `		}` |
|   18575045 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18575045 |  378 | `		pEntry = pEntry->pNext;` |
|    9287525 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100793 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100793 |  382 | `	pHash->apBucket = apNew;` |
|     100793 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100793 |  384 | `	return SXRET_OK;` |
|      50399 |  385 | `}` |
|   22413026 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   22413031 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   22413031 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   22413031 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   14200918 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    7100471 |  393 | `	}` |
|   22413031 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   22413031 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         63 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         63 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         63 |  401 | `		pHash->pLast = pEntry;` |
|         33 |  402 | `	}else{` |
|   22412971 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   22413031 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1184235 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1184235 |  408 | `		pHash->pLast = pEntry;` |
|     592190 |  409 | `	}` |
|   22413031 |  410 | `	pHash->nEntry++;` |
|   22413031 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   22413026 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   22413031 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100793 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100793 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50394 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   22413031 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   22413031 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   22413031 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   22413031 |  435 | `	pEntry->pHash = pHash;` |
|   22413031 |  436 | `	pEntry->pKey = pKey;` |
|   22413031 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   22413031 |  438 | `	pEntry->pUserData = pUserData;` |
|   22413031 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   22413031 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   22413031 |  442 | `	return rc;` |
|   11206968 |  443 | `}` |
|   22412874 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   22412879 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        152 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          4 |  455 | `{` |
|        156 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          4 |  457 | `}` |
|     530202 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     530207 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
