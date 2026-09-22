# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 296/315 lines (93.97%)

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
|  283827140 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  283827145 |   16 | `	pSet->nSize = 0 ;` |
|  283827145 |   17 | `	pSet->nUsed = 0;` |
|  283827145 |   18 | `	pSet->nCursor = 0;` |
|  283827145 |   19 | `	pSet->eSize = ElemSize;` |
|  283827145 |   20 | `	pSet->pAllocator = pAllocator;` |
|  283827145 |   21 | `	pSet->pBase =  0;` |
|  283827145 |   22 | `	pSet->pUserData = 0;` |
|  283827145 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  541026042 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  541026047 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   45716485 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   45716485 |   33 | `		if( pSet->nSize <= 0 ){` |
|   41494763 |   34 | `			pSet->nSize = 4;` |
|   20749161 |   35 | `		}` |
|   45716485 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   45716485 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   45716485 |   40 | `		pSet->pBase = pNew;` |
|   45716485 |   41 | `		pSet->nSize <<= 1;` |
|   22860022 |   42 | `	}` |
|  541026047 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 4110680785 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  541026047 |   45 | `	pSet->nUsed++;` |
|  541026047 |   46 | `	return SXRET_OK;` |
|  270519115 |   47 | `}` |
|   24208414 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   24208419 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   24208419 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   24208419 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   24208419 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   24208419 |   60 | `	pSet->nSize = nItem;` |
|   24208419 |   61 | `	return SXRET_OK;` |
|   12104212 |   62 | `}` |
|   39445351 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   39445356 |   65 | `	pSet->nUsed   = 0;` |
|   39445356 |   66 | `	pSet->nCursor = 0;` |
|   39445356 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|     104558 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|     104563 |   71 | `	pSet->nCursor = 0;` |
|     104563 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|     104886 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|     104891 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      47663 |   79 | `		pSet->nCursor = 0;` |
|      47663 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      57233 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      57233 |   83 | `	if( ppEntry ){` |
|      57233 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      28614 |   85 | `	}` |
|      57233 |   86 | `	pSet->nCursor++;` |
|      57233 |   87 | `	return SXRET_OK;` |
|      52448 |   88 | `}` |
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
|    3715422 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3715427 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        961 |  103 | `		pSet->nUsed = nNewSize;` |
|        478 |  104 | `	}` |
|    3715427 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|  134447564 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|  134447569 |  109 | `	sxi32 rc = SXRET_OK;` |
|  134447569 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   57340267 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   28671913 |  112 | `	}` |
|  134447569 |  113 | `	pSet->pBase = 0;` |
|  134447569 |  114 | `	pSet->nUsed = 0;` |
|  134447569 |  115 | `	pSet->nCursor = 0;` |
|  134447569 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   90239296 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   90239301 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       4655 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   90234651 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   90234651 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   45119653 |  126 | `}` |
|   32941714 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|   32941719 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2868921 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|   30072803 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   30072803 |  135 | `	pSet->nUsed--;` |
|   30072803 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   30072803 |  137 | `	return pData;` |
|   16472050 |  138 | `}` |
|  122015193 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|  122015198 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         91 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|  122015110 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  122015110 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   61016042 |  148 | `}` |
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
|    9779318 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    9779323 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    9779323 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    9779323 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    9779323 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    9779323 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    9779323 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    9779323 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    9779323 |  180 | `	pHash->nEntry = 0;` |
|    9779323 |  181 | `	pHash->apBucket = apNew;` |
|    9779323 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    9779323 |  183 | `	return SXRET_OK;` |
|    4889862 |  184 | `}` |
|    7116376 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|    7116381 |  193 | `	pEntry = pHash->pList;` |
|    8214612 |  194 | `	for(;;){` |
|   16428833 |  195 | `		if( pHash->nEntry == 0 ){` |
|    7116381 |  196 | `			break;` |
|          - |  197 | `		}` |
|    9312457 |  198 | `		pNext = pEntry->pNext;` |
|    9312457 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    9312457 |  200 | `		pEntry = pNext;` |
|    9312457 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|    7116381 |  203 | `	if( pHash->apBucket ){` |
|    7116381 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|    3558386 |  205 | `	}` |
|    7116381 |  206 | `	pHash->apBucket = 0;` |
|    7116381 |  207 | `	pHash->nBucketSize = 0;` |
|    7116381 |  208 | `	pHash->pAllocator = 0;` |
|    7116381 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|  163479836 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|  163479841 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  163479841 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  142658537 |  218 | `	for(;;){` |
|  282290653 |  219 | `		if( pEntry == 0 ){` |
|   69563704 |  220 | `			break;` |
|          - |  221 | `		}` |
|  259682356 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   93920921 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   93916142 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|  118810817 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   69563704 |  229 | `	return 0;` |
|   81752082 |  230 | `}` |
|  191339486 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|  191339491 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|   27860189 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|  163479307 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  163479307 |  244 | `	if( pEntry == 0 ){` |
|   69563686 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   93915626 |  247 | `	return (SyHashEntry *)pEntry;` |
|   95682105 |  248 | `}` |
|    6879650 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|    6879655 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    6747782 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|    3374883 |  254 | `	}else{` |
|     131878 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|    6879655 |  257 | `	if( pEntry->pNextCollide ){` |
|       9064 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       4531 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    6879655 |  261 | `	if( pHash->pLast == pEntry ){` |
|    6853773 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|    3428072 |  263 | `	}` |
|    6879655 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    6879655 |  265 | `	pHash->nEntry--;` |
|    6879655 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|    6879655 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    6879655 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        534 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        539 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        539 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        521 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        521 |  288 | `	return rc;` |
|        272 |  289 | `}` |
|    6879134 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|    6879139 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|    6879139 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    6879139 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    8123300 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    8123305 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    8123305 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   52000002 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   52000007 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    8123013 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    8123013 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   43876999 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   43876999 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   43876999 |  328 | `	return (SyHashEntry *)pEntry;` |
|   26000006 |  329 | `}` |
|         10 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|          2 |  331 | `{` |
|          - |  332 | `	SyHashEntry_Pr *pEntry;` |
|          - |  333 | `	sxi32 rc;` |
|          - |  334 | `	sxu32 n;` |
|          - |  335 | `#if defined(UNTRUST)` |
|          - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|          - |  337 | `		return 0;` |
|          - |  338 | `	}` |
|          - |  339 | `#endif` |
|         12 |  340 | `	pEntry = pHash->pList;` |
|       4546 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4536 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4536 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4536 |  348 | `		pEntry = pEntry->pNext;` |
|       2269 |  349 | `	}` |
|         12 |  350 | `	return SXRET_OK;` |
|          7 |  351 | `}` |
|          - |  352 | `/*` |
|          - |  353 | ` * Like SyHashForEach but walks the entries from the tail (pLast) back to the` |
|          - |  354 | ` * head via pPrev. The frame's local-variable table is built with SyHashInsert` |
|          - |  355 | ` * (head-push), so its forward pList order is reverse-insertion (LIFO); walking` |
|          - |  356 | ` * it backward yields DECLARATION order, which is what php's get_defined_vars()` |
|          - |  357 | ` * reports. Kept as its own primitive so the shared head-push insert path — and` |
|          - |  358 | ` * the SyHashLastEntry()==pList head contract every RefObj install relies on —` |
|          - |  359 | ` * stays untouched.` |
|          - |  360 | ` */` |
|         58 |  361 | `PH7_PRIVATE sxi32 SyHashForEachReverse(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|          4 |  362 | `{` |
|          - |  363 | `	SyHashEntry_Pr *pEntry;` |
|          - |  364 | `	sxi32 rc;` |
|          - |  365 | `	sxu32 n;` |
|          - |  366 | `#if defined(UNTRUST)` |
|          - |  367 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|          - |  368 | `		return 0;` |
|          - |  369 | `	}` |
|          - |  370 | `#endif` |
|         62 |  371 | `	pEntry = pHash->pLast;` |
|        824 |  372 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  373 | `		/* Invoke the callback */` |
|        766 |  374 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|        766 |  375 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  376 | `			return rc;` |
|          - |  377 | `		}` |
|          - |  378 | `		/* Point to the previous entry */` |
|        766 |  379 | `		pEntry = pEntry->pPrev;` |
|        385 |  380 | `	}` |
|         62 |  381 | `	return SXRET_OK;` |
|         33 |  382 | `}` |
|     121668 |  383 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  384 | `{` |
|     121673 |  385 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  386 | `	SyHashEntry_Pr *pEntry;` |
|          - |  387 | `	SyHashEntry_Pr **apNew;` |
|          - |  388 | `	sxu32 n,iBucket;` |
|          - |  389 |  |
|          - |  390 | `	/* Allocate a new larger table */` |
|     121673 |  391 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     121673 |  392 | `	if( apNew == 0 ){` |
|          - |  393 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  394 | `		return SXRET_OK;` |
|          - |  395 | `	}` |
|          - |  396 | `	/* Zero the new table */` |
|     121673 |  397 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  398 | `	/* Rehash all entries */` |
|   21887273 |  399 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   21765605 |  400 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  401 | `		/* Install in the new bucket */` |
|   21765605 |  402 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   21765605 |  403 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   21765605 |  404 | `		if( apNew[iBucket] != 0 ){` |
|   10516083 |  405 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    5258435 |  406 | `		}` |
|   21765605 |  407 | `		apNew[iBucket] = pEntry;` |
|          - |  408 | `		/* Point to the next entry */` |
|   21765605 |  409 | `		pEntry = pEntry->pNext;` |
|   10882805 |  410 | `	}` |
|          - |  411 | `	/* Release the old table and reflect the change */` |
|     121673 |  412 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     121673 |  413 | `	pHash->apBucket = apNew;` |
|     121673 |  414 | `	pHash->nBucketSize = nNewSize;` |
|     121673 |  415 | `	return SXRET_OK;` |
|      60839 |  416 | `}` |
|   42367600 |  417 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  418 | `{` |
|   42367605 |  419 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  420 | `	/* Insert the entry in its corresponding bucket */` |
|   42367605 |  421 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   42367605 |  422 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   16832500 |  423 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    8416326 |  424 | `	}` |
|   42367605 |  425 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  426 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  427 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  428 | `	 * callers that need a FIFO traversal. */` |
|   42367605 |  429 | `	if( bTail && pHash->pLast != 0 ){` |
|    8334925 |  430 | `		pHash->pLast->pNext = pEntry;` |
|    8334925 |  431 | `		pEntry->pPrev = pHash->pLast;` |
|    8334925 |  432 | `		pHash->pLast = pEntry;` |
|    4167465 |  433 | `	}else{` |
|   34032685 |  434 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  435 | `	}` |
|   42367605 |  436 | `	if( pHash->nEntry == 0 ){` |
|          - |  437 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    4978455 |  438 | `		pHash->pCurrent = pHash->pList;` |
|    4978455 |  439 | `		pHash->pLast = pEntry;` |
|    2489423 |  440 | `	}` |
|   42367605 |  441 | `	pHash->nEntry++;` |
|   42367605 |  442 | `	return SXRET_OK;` |
|          5 |  443 | `}` |
|   42367600 |  444 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  445 | `{` |
|          - |  446 | `	SyHashEntry_Pr *pEntry;` |
|          - |  447 | `	sxi32 rc;` |
|          - |  448 | `#if defined(UNTRUST)` |
|          - |  449 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  450 | `		return SXERR_CORRUPT;` |
|          - |  451 | `	}` |
|          - |  452 | `#endif` |
|   42367605 |  453 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     121673 |  454 | `		rc = HashGrowTable(&(*pHash));` |
|     121673 |  455 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  456 | `			return rc;` |
|          - |  457 | `		}` |
|      60834 |  458 | `	}` |
|          - |  459 | `	/* Allocate a new hash entry */` |
|   42367605 |  460 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   42367605 |  461 | `	if( pEntry == 0 ){` |
|        ! 0 |  462 | `		return SXERR_MEM;` |
|          - |  463 | `	}` |
|          - |  464 | `	/* Zero the entry */` |
|   42367605 |  465 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   42367605 |  466 | `	pEntry->pHash = pHash;` |
|   42367605 |  467 | `	pEntry->pKey = pKey;` |
|   42367605 |  468 | `	pEntry->nKeyLen = nKeyLen;` |
|   42367605 |  469 | `	pEntry->pUserData = pUserData;` |
|   42367605 |  470 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  471 | `	/* Finally insert the entry in its corresponding bucket */` |
|   42367605 |  472 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   42367605 |  473 | `	return rc;` |
|   21184993 |  474 | `}` |
|   32169842 |  475 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  476 | `{` |
|   32169847 |  477 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  478 | `}` |
|          - |  479 | `/*` |
|          - |  480 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  481 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  482 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  483 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  484 | ` */` |
|   10197758 |  485 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  486 | `{` |
|   10197763 |  487 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          5 |  488 | `}` |
|    6926770 |  489 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  490 | `{` |
|          - |  491 | `#if defined(UNTRUST)` |
|          - |  492 | `	if( INVALID_HASH(pHash) ){` |
|          - |  493 | `		return 0;` |
|          - |  494 | `	}` |
|          - |  495 | `#endif` |
|          - |  496 | `	/* Last inserted entry */` |
|    6926775 |  497 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  498 | `}` |
|          - |  499 |  |
