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
|   83659568 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|   83659573 |   16 | `	pSet->nSize = 0 ;` |
|   83659573 |   17 | `	pSet->nUsed = 0;` |
|   83659573 |   18 | `	pSet->nCursor = 0;` |
|   83659573 |   19 | `	pSet->eSize = ElemSize;` |
|   83659573 |   20 | `	pSet->pAllocator = pAllocator;` |
|   83659573 |   21 | `	pSet->pBase =  0;` |
|   83659573 |   22 | `	pSet->pUserData = 0;` |
|   83659573 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  180113879 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  180113884 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   12073653 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   12073653 |   33 | `		if( pSet->nSize <= 0 ){` |
|   10642515 |   34 | `			pSet->nSize = 4;` |
|    5321255 |   35 | `		}` |
|   12073653 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   12073653 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   12073653 |   40 | `		pSet->pBase = pNew;` |
|   12073653 |   41 | `		pSet->nSize <<= 1;` |
|    6036824 |   42 | `	}` |
|  180113884 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 1321809500 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  180113884 |   45 | `	pSet->nUsed++;` |
|  180113884 |   46 | `	return SXRET_OK;` |
|   90056987 |   47 | `}` |
|    8860570 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|    8860575 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|    8860575 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|    8860575 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    8860575 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|    8860575 |   60 | `	pSet->nSize = nItem;` |
|    8860575 |   61 | `	return SXRET_OK;` |
|    4430290 |   62 | `}` |
|   13813185 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   13813190 |   65 | `	pSet->nUsed   = 0;` |
|   13813190 |   66 | `	pSet->nCursor = 0;` |
|   13813190 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      68920 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      68925 |   71 | `	pSet->nCursor = 0;` |
|      68925 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73104 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73109 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29655 |   79 | `		pSet->nCursor = 0;` |
|      29655 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43459 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43459 |   83 | `	if( ppEntry ){` |
|      43459 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21727 |   85 | `	}` |
|      43459 |   86 | `	pSet->nCursor++;` |
|      43459 |   87 | `	return SXRET_OK;` |
|      36557 |   88 | `}` |
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
|    1414504 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    1414509 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1179 |  103 | `		pSet->nUsed = nNewSize;` |
|        587 |  104 | `	}` |
|    1414509 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   31123628 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   31123633 |  109 | `	sxi32 rc = SXRET_OK;` |
|   31123633 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   16646807 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|    8323401 |  112 | `	}` |
|   31123633 |  113 | `	pSet->pBase = 0;` |
|   31123633 |  114 | `	pSet->nUsed = 0;` |
|   31123633 |  115 | `	pSet->nCursor = 0;` |
|   31123633 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   31400942 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   31400947 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        133 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   31400819 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   31400819 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   15700476 |  126 | `}` |
|    6196642 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    6196647 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2195419 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    4001233 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    4001233 |  135 | `	pSet->nUsed--;` |
|    4001233 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    4001233 |  137 | `	return pData;` |
|    3098326 |  138 | `}` |
|   21370703 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   21370708 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         22 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   21370688 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   21370688 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   10685699 |  148 | `}` |
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
|    1162178 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1162183 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1162183 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1162183 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1162183 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1162183 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1162183 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1162183 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1162183 |  180 | `	pHash->nEntry = 0;` |
|    1162183 |  181 | `	pHash->apBucket = apNew;` |
|    1162183 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1162183 |  183 | `	return SXRET_OK;` |
|     581094 |  184 | `}` |
|     311186 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     311191 |  193 | `	pEntry = pHash->pList;` |
|     164537 |  194 | `	for(;;){` |
|     329079 |  195 | `		if( pHash->nEntry == 0 ){` |
|     311191 |  196 | `			break;` |
|          - |  197 | `		}` |
|      17893 |  198 | `		pNext = pEntry->pNext;` |
|      17893 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      17893 |  200 | `		pEntry = pNext;` |
|      17893 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     311191 |  203 | `	if( pHash->apBucket ){` |
|     311191 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     155593 |  205 | `	}` |
|     311191 |  206 | `	pHash->apBucket = 0;` |
|     311191 |  207 | `	pHash->nBucketSize = 0;` |
|     311191 |  208 | `	pHash->pAllocator = 0;` |
|     311191 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   40598443 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   40598448 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   40598448 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   38039233 |  218 | `	for(;;){` |
|   76122737 |  219 | `		if( pEntry == 0 ){` |
|   16168328 |  220 | `			break;` |
|          - |  221 | `		}` |
|   72169240 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   24430162 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   24430125 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   35524294 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   16168328 |  229 | `	return 0;` |
|   20299738 |  230 | `}` |
|   44243453 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   44243458 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    3645329 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   40598134 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   40598134 |  244 | `	if( pEntry == 0 ){` |
|   16168328 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   24429811 |  247 | `	return (SyHashEntry *)pEntry;` |
|   22122243 |  248 | `}` |
|     211750 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     211755 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     169341 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      84673 |  254 | `	}else{` |
|      42419 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     211755 |  257 | `	if( pEntry->pNextCollide ){` |
|       4182 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2090 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     211755 |  261 | `	if( pHash->pLast == pEntry ){` |
|     205005 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     102500 |  263 | `	}` |
|     211755 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     211755 |  265 | `	pHash->nEntry--;` |
|     211755 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|        ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        ! 0 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     211755 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     211755 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        314 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        319 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        319 |  284 | `	if( pEntry == 0 ){` |
|        ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        319 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        319 |  288 | `	return rc;` |
|        162 |  289 | `}` |
|     211436 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     211441 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     211441 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     211441 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    1758202 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    1758207 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    1758207 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   13373446 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   13373451 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    1757941 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    1757941 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   11615515 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   11615515 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   11615515 |  328 | `	return (SyHashEntry *)pEntry;` |
|    6686728 |  329 | `}` |
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
|       3117 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3107 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3107 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3107 |  348 | `		pEntry = pEntry->pNext;` |
|       1554 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      77790 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      77795 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      77795 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      77795 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      77795 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|    9260867 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|    9183077 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|    9183077 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|    9183077 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|    9183077 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    4414407 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    2207133 |  375 | `		}` |
|    9183077 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|    9183077 |  378 | `		pEntry = pEntry->pNext;` |
|    4591541 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      77795 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      77795 |  382 | `	pHash->apBucket = apNew;` |
|      77795 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      77795 |  384 | `	return SXRET_OK;` |
|      38900 |  385 | `}` |
|   11484388 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   11484393 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   11484393 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   11484393 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    7236659 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    3618374 |  393 | `	}` |
|   11484393 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   11484393 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   11484341 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   11484393 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     605207 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     605207 |  408 | `		pHash->pLast = pEntry;` |
|     302601 |  409 | `	}` |
|   11484393 |  410 | `	pHash->nEntry++;` |
|   11484393 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   11484388 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   11484393 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      77795 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      77795 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      38895 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   11484393 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   11484393 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   11484393 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   11484393 |  435 | `	pEntry->pHash = pHash;` |
|   11484393 |  436 | `	pEntry->pKey = pKey;` |
|   11484393 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   11484393 |  438 | `	pEntry->pUserData = pUserData;` |
|   11484393 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   11484393 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   11484393 |  442 | `	return rc;` |
|    5742199 |  443 | `}` |
|   11484260 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   11484265 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     252558 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     252563 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
