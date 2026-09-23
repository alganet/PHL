# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 296/315 lines (93.97%)

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
| 110148584 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 | `{` |
| 110148589 |   16 | `	pSet->nSize = 0 ;` |
| 110148589 |   17 | `	pSet->nUsed = 0;` |
| 110148589 |   18 | `	pSet->nCursor = 0;` |
| 110148589 |   19 | `	pSet->eSize = ElemSize;` |
| 110148589 |   20 | `	pSet->pAllocator = pAllocator;` |
| 110148589 |   21 | `	pSet->pBase =  0;` |
| 110148589 |   22 | `	pSet->pUserData = 0;` |
| 110148589 |   23 | `	return SXRET_OK;` |
|         5 |   24 | `}` |
|  80356374 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 | `{` |
|         - |   27 | `	unsigned char *zbase;` |
|  80356379 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|  14867625 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|  14867625 |   33 | `		if( pSet->nSize <= 0 ){` |
|  14482475 |   34 | `			pSet->nSize = 4;` |
|   7243143 |   35 | `		}` |
|  14867625 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|  14867625 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|  14867625 |   40 | `		pSet->pBase = pNew;` |
|  14867625 |   41 | `		pSet->nSize <<= 1;` |
|   7435718 |   42 | `	}` |
|  80356379 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 535378803 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  80356379 |   45 | `	pSet->nUsed++;` |
|  80356379 |   46 | `	return SXRET_OK;` |
|  40184717 |   47 | `}` |
|   5691918 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 | `{` |
|   5691923 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   5691923 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   5691923 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   5691923 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   5691923 |   60 | `	pSet->nSize = nItem;` |
|   5691923 |   61 | `	return SXRET_OK;` |
|   2845964 |   62 | `}` |
|   6084931 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 | `{` |
|   6084936 |   65 | `	pSet->nUsed   = 0;` |
|   6084936 |   66 | `	pSet->nCursor = 0;` |
|   6084936 |   67 | `	return SXRET_OK;` |
|         5 |   68 | `}` |
|    115302 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 | `{` |
|    115307 |   71 | `	pSet->nCursor = 0;` |
|    115307 |   72 | `	return SXRET_OK;` |
|         5 |   73 | `}` |
|    116158 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 | `{` |
|         - |   76 | `	register unsigned char *zSrc;` |
|    116163 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     52753 |   79 | `		pSet->nCursor = 0;` |
|     52753 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     63415 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     63415 |   83 | `	if( ppEntry ){` |
|     63415 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     31705 |   85 | `	}` |
|     63415 |   86 | `	pSet->nCursor++;` |
|     63415 |   87 | `	return SXRET_OK;` |
|     58084 |   88 | `}` |
|         - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|       ! 0 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|       ! 0 |   91 | `{` |
|         - |   92 | `	register unsigned char *zSrc;` |
|       ! 0 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|       ! 0 |   94 | `		return 0;` |
|         - |   95 | `	}` |
|       ! 0 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|       ! 0 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|       ! 0 |   98 | `}` |
|         - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    149422 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 | `{` |
|    149427 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|      1129 |  103 | `		pSet->nUsed = nNewSize;` |
|       562 |  104 | `	}` |
|    149427 |  105 | `	return SXRET_OK;` |
|         5 |  106 | `}` |
|  57117886 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 | `{` |
|  57117891 |  109 | `	sxi32 rc = SXRET_OK;` |
|  57117891 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|  15709557 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   7856684 |  112 | `	}` |
|  57117891 |  113 | `	pSet->pBase = 0;` |
|  57117891 |  114 | `	pSet->nUsed = 0;` |
|  57117891 |  115 | `	pSet->nCursor = 0;` |
|  57117891 |  116 | `	return rc;` |
|         5 |  117 | `}` |
|  13291130 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 | `{` |
|         - |  120 | `	const char *zBase;` |
|  13291135 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       131 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|  13291009 |  124 | `	zBase = (const char *)pSet->pBase;` |
|  13291009 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   6645570 |  126 | `}` |
|  21750062 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 | `{` |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|  21750067 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2938033 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|  18812039 |  134 | `	zBase = (const char *)pSet->pBase;` |
|  18812039 |  135 | `	pSet->nUsed--;` |
|  18812039 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|  18812039 |  137 | `	return pData;` |
|  10876308 |  138 | `}` |
|  65857213 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	const char *zBase;` |
|  65857218 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       114 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  65857108 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  65857108 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|  32940071 |  148 | `}` |
|         - |  149 | `/* Private hash entry */` |
|         - |  150 | `struct SyHashEntry_Pr` |
|         - |  151 | `{` |
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
|   7941738 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 | `{` |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|   7941743 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|   7941743 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|   7941743 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|   7941743 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|   7941743 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|   7941743 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|   7941743 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|   7941743 |  180 | `	pHash->nEntry = 0;` |
|   7941743 |  181 | `	pHash->apBucket = apNew;` |
|   7941743 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|   7941743 |  183 | `	return SXRET_OK;` |
|   3971086 |  184 | `}` |
|   5225148 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 | `{` |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|   5225153 |  193 | `	pEntry = pHash->pList;` |
|   7979149 |  194 | `	for(;;){` |
|  15955335 |  195 | `		if( pHash->nEntry == 0 ){` |
|   5225153 |  196 | `			break;` |
|         - |  197 | `		}` |
|  10730187 |  198 | `		pNext = pEntry->pNext;` |
|  10730187 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|  10730187 |  200 | `		pEntry = pNext;` |
|  10730187 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|   5225153 |  203 | `	if( pHash->apBucket ){` |
|   5225153 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|   2612786 |  205 | `	}` |
|   5225153 |  206 | `	pHash->apBucket = 0;` |
|   5225153 |  207 | `	pHash->nBucketSize = 0;` |
|   5225153 |  208 | `	pHash->pAllocator = 0;` |
|   5225153 |  209 | `	return SXRET_OK;` |
|         5 |  210 | `}` |
|  93885091 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 | `{` |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  93885096 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  93885096 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  80818139 |  218 | `	for(;;){` |
| 161687155 |  219 | `		if( pEntry == 0 ){` |
|  33751127 |  220 | `			break;` |
|         - |  221 | `		}` |
| 158234236 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|  60612881 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|  60133974 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  67802064 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|  33751127 |  229 | `	return 0;` |
|  46961184 |  230 | `}` |
|  91417655 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 | `{` |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  91417660 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|   4326259 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  87091406 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  87091406 |  244 | `	if( pEntry == 0 ){` |
|  33746027 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|  53345384 |  247 | `	return (SyHashEntry *)pEntry;` |
|  45727678 |  248 | `}` |
|   6795168 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 | `{` |
|         - |  251 | `	sxi32 rc;` |
|   6795173 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|   6779125 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|   3389566 |  254 | `	}else{` |
|     16053 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|   6795173 |  257 | `	if( pEntry->pNextCollide ){` |
|   6435916 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|   3217954 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|   6795173 |  261 | `	if( pHash->pLast == pEntry ){` |
|     42371 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     21183 |  263 | `	}` |
|   6795173 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|   6795173 |  265 | `	pHash->nEntry--;` |
|   6795173 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|        74 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        36 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|   6795173 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|   6795173 |  272 | `	return rc;` |
|         5 |  273 | `}` |
|   6793690 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 | `{` |
|         - |  276 | `	SyHashEntry_Pr *pEntry;` |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( INVALID_HASH(pHash) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|   6793695 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   6793695 |  284 | `	if( pEntry == 0 ){` |
|      5102 |  285 | `		return SXERR_NOTFOUND;` |
|         - |  286 | `	}` |
|   6788595 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|   6788595 |  288 | `	return rc;` |
|   3396850 |  289 | `}` |
|      6578 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 | `{` |
|      6583 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|      6583 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|      6583 |  300 | `	return rc;` |
|         5 |  301 | `}` |
|   8557994 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 | `{` |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   8557999 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   8557999 |  310 | `	return SXRET_OK;` |
|         5 |  311 | `}` |
|  60373058 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 | `{` |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|  60373063 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   8557697 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   8557697 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|  51815371 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|  51815371 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|  51815371 |  328 | `	return (SyHashEntry *)pEntry;` |
|  30186534 |  329 | `}` |
|        30 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         2 |  331 | `{` |
|         - |  332 | `	SyHashEntry_Pr *pEntry;` |
|         - |  333 | `	sxi32 rc;` |
|         - |  334 | `	sxu32 n;` |
|         - |  335 | `#if defined(UNTRUST)` |
|         - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  337 | `		return 0;` |
|         - |  338 | `	}` |
|         - |  339 | `#endif` |
|        32 |  340 | `	pEntry = pHash->pList;` |
|     28616 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|     28586 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|     28586 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|     28586 |  348 | `		pEntry = pEntry->pNext;` |
|     14294 |  349 | `	}` |
|        32 |  350 | `	return SXRET_OK;` |
|        17 |  351 | `}` |
|         - |  352 | `/*` |
|         - |  353 | ` * Like SyHashForEach but walks the entries from the tail (pLast) back to the` |
|         - |  354 | ` * head via pPrev. The frame's local-variable table is built with SyHashInsert` |
|         - |  355 | ` * (head-push), so its forward pList order is reverse-insertion (LIFO); walking` |
|         - |  356 | ` * it backward yields DECLARATION order, which is what php's get_defined_vars()` |
|         - |  357 | ` * reports. Kept as its own primitive so the shared head-push insert path — and` |
|         - |  358 | ` * the SyHashLastEntry()==pList head contract every RefObj install relies on —` |
|         - |  359 | ` * stays untouched.` |
|         - |  360 | ` */` |
|        70 |  361 | `PH7_PRIVATE sxi32 SyHashForEachReverse(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         5 |  362 | `{` |
|         - |  363 | `	SyHashEntry_Pr *pEntry;` |
|         - |  364 | `	sxi32 rc;` |
|         - |  365 | `	sxu32 n;` |
|         - |  366 | `#if defined(UNTRUST)` |
|         - |  367 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  368 | `		return 0;` |
|         - |  369 | `	}` |
|         - |  370 | `#endif` |
|        75 |  371 | `	pEntry = pHash->pLast;` |
|      5807 |  372 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  373 | `		/* Invoke the callback */` |
|      5737 |  374 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      5737 |  375 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  376 | `			return rc;` |
|         - |  377 | `		}` |
|         - |  378 | `		/* Point to the previous entry */` |
|      5737 |  379 | `		pEntry = pEntry->pPrev;` |
|      2871 |  380 | `	}` |
|        75 |  381 | `	return SXRET_OK;` |
|        40 |  382 | `}` |
|     98070 |  383 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  384 | `{` |
|     98075 |  385 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  386 | `	SyHashEntry_Pr *pEntry;` |
|         - |  387 | `	SyHashEntry_Pr **apNew;` |
|         - |  388 | `	sxu32 n,iBucket;` |
|         - |  389 |  |
|         - |  390 | `	/* Allocate a new larger table */` |
|     98075 |  391 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     98075 |  392 | `	if( apNew == 0 ){` |
|         - |  393 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  394 | `		return SXRET_OK;` |
|         - |  395 | `	}` |
|         - |  396 | `	/* Zero the new table */` |
|     98075 |  397 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  398 | `	/* Rehash all entries */` |
|  16215707 |  399 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|  16117637 |  400 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  401 | `		/* Install in the new bucket */` |
|  16117637 |  402 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|  16117637 |  403 | `		pEntry->pNextCollide = apNew[iBucket];` |
|  16117637 |  404 | `		if( apNew[iBucket] != 0 ){` |
|   8059165 |  405 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|   4029584 |  406 | `		}` |
|  16117637 |  407 | `		apNew[iBucket] = pEntry;` |
|         - |  408 | `		/* Point to the next entry */` |
|  16117637 |  409 | `		pEntry = pEntry->pNext;` |
|   8058821 |  410 | `	}` |
|         - |  411 | `	/* Release the old table and reflect the change */` |
|     98075 |  412 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     98075 |  413 | `	pHash->apBucket = apNew;` |
|     98075 |  414 | `	pHash->nBucketSize = nNewSize;` |
|     98075 |  415 | `	return SXRET_OK;` |
|     49040 |  416 | `}` |
|  39655028 |  417 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  418 | `{` |
|  39655033 |  419 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  420 | `	/* Insert the entry in its corresponding bucket */` |
|  39655033 |  421 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|  39655033 |  422 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|  18788528 |  423 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   9394833 |  424 | `	}` |
|  39655033 |  425 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  426 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  427 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  428 | `	 * callers that need a FIFO traversal. */` |
|  39655033 |  429 | `	if( bTail && pHash->pLast != 0 ){` |
|   9902991 |  430 | `		pHash->pLast->pNext = pEntry;` |
|   9902991 |  431 | `		pEntry->pPrev = pHash->pLast;` |
|   9902991 |  432 | `		pHash->pLast = pEntry;` |
|   4951498 |  433 | `	}else{` |
|  29752047 |  434 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  435 | `	}` |
|  39655033 |  436 | `	if( pHash->nEntry == 0 ){` |
|         - |  437 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|   3439831 |  438 | `		pHash->pCurrent = pHash->pList;` |
|   3439831 |  439 | `		pHash->pLast = pEntry;` |
|   1720125 |  440 | `	}` |
|  39655033 |  441 | `	pHash->nEntry++;` |
|  39655033 |  442 | `	return SXRET_OK;` |
|         5 |  443 | `}` |
|  39655028 |  444 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  445 | `{` |
|         - |  446 | `	SyHashEntry_Pr *pEntry;` |
|         - |  447 | `	sxi32 rc;` |
|         - |  448 | `#if defined(UNTRUST)` |
|         - |  449 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  450 | `		return SXERR_CORRUPT;` |
|         - |  451 | `	}` |
|         - |  452 | `#endif` |
|  39655033 |  453 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     98075 |  454 | `		rc = HashGrowTable(&(*pHash));` |
|     98075 |  455 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  456 | `			return rc;` |
|         - |  457 | `		}` |
|     49035 |  458 | `	}` |
|         - |  459 | `	/* Allocate a new hash entry */` |
|  39655033 |  460 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|  39655033 |  461 | `	if( pEntry == 0 ){` |
|       ! 0 |  462 | `		return SXERR_MEM;` |
|         - |  463 | `	}` |
|         - |  464 | `	/* Zero the entry */` |
|  39655033 |  465 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|  39655033 |  466 | `	pEntry->pHash = pHash;` |
|  39655033 |  467 | `	pEntry->pKey = pKey;` |
|  39655033 |  468 | `	pEntry->nKeyLen = nKeyLen;` |
|  39655033 |  469 | `	pEntry->pUserData = pUserData;` |
|  39655033 |  470 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  471 | `	/* Finally insert the entry in its corresponding bucket */` |
|  39655033 |  472 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|  39655033 |  473 | `	return rc;` |
|  19828791 |  474 | `}` |
|  27871672 |  475 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  476 | `{` |
|  27871677 |  477 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|         5 |  478 | `}` |
|         - |  479 | `/*` |
|         - |  480 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|         - |  481 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|         - |  482 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|         - |  483 | ` * object properties, where PHP preserves property-creation order.` |
|         - |  484 | ` */` |
|  11783356 |  485 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  486 | `{` |
|  11783361 |  487 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|         5 |  488 | `}` |
|   1130880 |  489 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  490 | `{` |
|         - |  491 | `#if defined(UNTRUST)` |
|         - |  492 | `	if( INVALID_HASH(pHash) ){` |
|         - |  493 | `		return 0;` |
|         - |  494 | `	}` |
|         - |  495 | `#endif` |
|         - |  496 | `	/* Last inserted entry */` |
|   1130885 |  497 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  498 | `}` |
|         - |  499 |  |
