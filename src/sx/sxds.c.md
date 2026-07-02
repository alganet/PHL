# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 289/304 lines (95.07%)

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
|  19955964 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 | `{` |
|  19955969 |   16 | `	pSet->nSize = 0 ;` |
|  19955969 |   17 | `	pSet->nUsed = 0;` |
|  19955969 |   18 | `	pSet->nCursor = 0;` |
|  19955969 |   19 | `	pSet->eSize = ElemSize;` |
|  19955969 |   20 | `	pSet->pAllocator = pAllocator;` |
|  19955969 |   21 | `	pSet->pBase =  0;` |
|  19955969 |   22 | `	pSet->pUserData = 0;` |
|  19955969 |   23 | `	return SXRET_OK;` |
|         5 |   24 | `}` |
|  33011109 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 | `{` |
|         - |   27 | `	unsigned char *zbase;` |
|  33011114 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4688189 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4688189 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4524469 |   34 | `			pSet->nSize = 4;` |
|   2262232 |   35 | `		}` |
|   4688189 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4688189 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4688189 |   40 | `		pSet->pBase = pNew;` |
|   4688189 |   41 | `		pSet->nSize <<= 1;` |
|   2344092 |   42 | `	}` |
|  33011114 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 247372018 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  33011114 |   45 | `	pSet->nUsed++;` |
|  33011114 |   46 | `	return SXRET_OK;` |
|  16505602 |   47 | `}` |
|   1364748 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 | `{` |
|   1364753 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1364753 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1364753 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1364753 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1364753 |   60 | `	pSet->nSize = nItem;` |
|   1364753 |   61 | `	return SXRET_OK;` |
|    682379 |   62 | `}` |
|   1875407 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 | `{` |
|   1875412 |   65 | `	pSet->nUsed   = 0;` |
|   1875412 |   66 | `	pSet->nCursor = 0;` |
|   1875412 |   67 | `	return SXRET_OK;` |
|         5 |   68 | `}` |
|     58806 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 | `{` |
|     58811 |   71 | `	pSet->nCursor = 0;` |
|     58811 |   72 | `	return SXRET_OK;` |
|         5 |   73 | `}` |
|     63010 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 | `{` |
|         - |   76 | `	register unsigned char *zSrc;` |
|     63015 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     24397 |   79 | `		pSet->nCursor = 0;` |
|     24397 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     38623 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     38623 |   83 | `	if( ppEntry ){` |
|     38623 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     19309 |   85 | `	}` |
|     38623 |   86 | `	pSet->nCursor++;` |
|     38623 |   87 | `	return SXRET_OK;` |
|     31510 |   88 | `}` |
|         - |   89 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         8 |   90 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|         1 |   91 | `{` |
|         - |   92 | `	register unsigned char *zSrc;` |
|         9 |   93 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         3 |   94 | `		return 0;` |
|         - |   95 | `	}` |
|         7 |   96 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|         7 |   97 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|         5 |   98 | `}` |
|         - |   99 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    229914 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 | `{` |
|    229919 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       295 |  103 | `		pSet->nUsed = nNewSize;` |
|       145 |  104 | `	}` |
|    229919 |  105 | `	return SXRET_OK;` |
|         5 |  106 | `}` |
|  10235316 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 | `{` |
|  10235321 |  109 | `	sxi32 rc = SXRET_OK;` |
|  10235321 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   5133929 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2566962 |  112 | `	}` |
|  10235321 |  113 | `	pSet->pBase = 0;` |
|  10235321 |  114 | `	pSet->nUsed = 0;` |
|  10235321 |  115 | `	pSet->nCursor = 0;` |
|  10235321 |  116 | `	return rc;` |
|         5 |  117 | `}` |
|   5970628 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 | `{` |
|         - |  120 | `	const char *zBase;` |
|   5970633 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       133 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5970505 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5970505 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2985319 |  126 | `}` |
|   3633616 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 | `{` |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3633621 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2185611 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1448015 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1448015 |  135 | `	pSet->nUsed--;` |
|   1448015 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1448015 |  137 | `	return pData;` |
|   1816813 |  138 | `}` |
|  13711550 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	const char *zBase;` |
|  13711555 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  13711555 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  13711555 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6856146 |  148 | `}` |
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
|    601098 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 | `{` |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    601103 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    601103 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    601103 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    601103 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    601103 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    601103 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    601103 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    601103 |  180 | `	pHash->nEntry = 0;` |
|    601103 |  181 | `	pHash->apBucket = apNew;` |
|    601103 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    601103 |  183 | `	return SXRET_OK;` |
|    300554 |  184 | `}` |
|    107714 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 | `{` |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|    107719 |  193 | `	pEntry = pHash->pList;` |
|     57805 |  194 | `	for(;;){` |
|    115615 |  195 | `		if( pHash->nEntry == 0 ){` |
|    107719 |  196 | `			break;` |
|         - |  197 | `		}` |
|      7901 |  198 | `		pNext = pEntry->pNext;` |
|      7901 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      7901 |  200 | `		pEntry = pNext;` |
|      7901 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|    107719 |  203 | `	if( pHash->apBucket ){` |
|    107719 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     53857 |  205 | `	}` |
|    107719 |  206 | `	pHash->apBucket = 0;` |
|    107719 |  207 | `	pHash->nBucketSize = 0;` |
|    107719 |  208 | `	pHash->pAllocator = 0;` |
|    107719 |  209 | `	return SXRET_OK;` |
|         5 |  210 | `}` |
|  18114110 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 | `{` |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  18114115 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  18114115 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  16359987 |  218 | `	for(;;){` |
|  32696156 |  219 | `		if( pEntry == 0 ){` |
|   9642193 |  220 | `			break;` |
|         - |  221 | `		}` |
|  27289679 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   8471932 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   8471927 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  14582046 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   9642193 |  229 | `	return 0;` |
|   9057570 |  230 | `}` |
|  19021770 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 | `{` |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  19021775 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    907875 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  18113905 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  18113905 |  244 | `	if( pEntry == 0 ){` |
|   9642193 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   8471717 |  247 | `	return (SyHashEntry *)pEntry;` |
|   9511400 |  248 | `}` |
|    133670 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 | `{` |
|         - |  251 | `	sxi32 rc;` |
|    133675 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|    103675 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     51840 |  254 | `	}else{` |
|     30005 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    133675 |  257 | `	if( pEntry->pNextCollide ){` |
|      5111 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2555 |  259 | `	}` |
|         - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|    133675 |  261 | `	if( pHash->pLast == pEntry ){` |
|    127367 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     63681 |  263 | `	}` |
|    133675 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    133675 |  265 | `	pHash->nEntry--;` |
|    133675 |  266 | `	if( ppUserData ){` |
|         - |  267 | `		/* Write a pointer to the user data */` |
|       ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  269 | `	}` |
|         - |  270 | `	/* Release the entry */` |
|    133675 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    133675 |  272 | `	return rc;` |
|         5 |  273 | `}` |
|       210 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  275 | `{` |
|         - |  276 | `	SyHashEntry_Pr *pEntry;` |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( INVALID_HASH(pHash) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|       215 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       215 |  284 | `	if( pEntry == 0 ){` |
|       ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|         - |  286 | `	}` |
|       215 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       215 |  288 | `	return rc;` |
|       110 |  289 | `}` |
|    133460 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  291 | `{` |
|    133465 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  293 | `	sxi32 rc;` |
|         - |  294 | `#if defined(UNTRUST)` |
|         - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  296 | `		return SXERR_CORRUPT;` |
|         - |  297 | `	}` |
|         - |  298 | `#endif` |
|    133465 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    133465 |  300 | `	return rc;` |
|         5 |  301 | `}` |
|   1207290 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  303 | `{` |
|         - |  304 | `#if defined(UNTRUST)` |
|         - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  306 | `		return SXERR_CORRUPT;` |
|         - |  307 | `	}` |
|         - |  308 | `#endif` |
|   1207295 |  309 | `	pHash->pCurrent = pHash->pList;` |
|   1207295 |  310 | `	return SXRET_OK;` |
|         5 |  311 | `}` |
|   7655880 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  313 | `{` |
|         - |  314 | `	SyHashEntry_Pr *pEntry;` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( INVALID_HASH(pHash) ){` |
|         - |  317 | `		return 0;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|   7655885 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1207033 |  321 | `		pHash->pCurrent = pHash->pList;` |
|   1207033 |  322 | `		return 0;` |
|         - |  323 | `	}` |
|   6448857 |  324 | `	pEntry = pHash->pCurrent;` |
|         - |  325 | `	/* Advance the cursor */` |
|   6448857 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  327 | `	/* Return the current entry */` |
|   6448857 |  328 | `	return (SyHashEntry *)pEntry;` |
|   3827945 |  329 | `}` |
|        10 |  330 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         1 |  331 | `{` |
|         - |  332 | `	SyHashEntry_Pr *pEntry;` |
|         - |  333 | `	sxi32 rc;` |
|         - |  334 | `	sxu32 n;` |
|         - |  335 | `#if defined(UNTRUST)` |
|         - |  336 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  337 | `		return 0;` |
|         - |  338 | `	}` |
|         - |  339 | `#endif` |
|        11 |  340 | `	pEntry = pHash->pList;` |
|      2001 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  342 | `		/* Invoke the callback */` |
|      1991 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1991 |  344 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  345 | `			return rc;` |
|         - |  346 | `		}` |
|         - |  347 | `		/* Point to the next entry */` |
|      1991 |  348 | `		pEntry = pEntry->pNext;` |
|       996 |  349 | `	}` |
|        11 |  350 | `	return SXRET_OK;` |
|         6 |  351 | `}` |
|     31534 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  353 | `{` |
|     31539 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  355 | `	SyHashEntry_Pr *pEntry;` |
|         - |  356 | `	SyHashEntry_Pr **apNew;` |
|         - |  357 | `	sxu32 n,iBucket;` |
|         - |  358 |  |
|         - |  359 | `	/* Allocate a new larger table */` |
|     31539 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     31539 |  361 | `	if( apNew == 0 ){` |
|         - |  362 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  363 | `		return SXRET_OK;` |
|         - |  364 | `	}` |
|         - |  365 | `	/* Zero the new table */` |
|     31539 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  367 | `	/* Rehash all entries */` |
|   3975603 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3944069 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  370 | `		/* Install in the new bucket */` |
|   3944069 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3944069 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3944069 |  373 | `		if( apNew[iBucket] != 0 ){` |
|   1893057 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    946488 |  375 | `		}` |
|   3944069 |  376 | `		apNew[iBucket] = pEntry;` |
|         - |  377 | `		/* Point to the next entry */` |
|   3944069 |  378 | `		pEntry = pEntry->pNext;` |
|   1972037 |  379 | `	}` |
|         - |  380 | `	/* Release the old table and reflect the change */` |
|     31539 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     31539 |  382 | `	pHash->apBucket = apNew;` |
|     31539 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     31539 |  384 | `	return SXRET_OK;` |
|     15772 |  385 | `}` |
|   5193484 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  387 | `{` |
|   5193489 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   5193489 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   5193489 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2938283 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1469170 |  393 | `	}` |
|   5193489 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  397 | `	 * callers that need a FIFO traversal. */` |
|   5193489 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|        51 |  399 | `		pHash->pLast->pNext = pEntry;` |
|        51 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|        51 |  401 | `		pHash->pLast = pEntry;` |
|        26 |  402 | `	}else{` |
|   5193439 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  404 | `	}` |
|   5193489 |  405 | `	if( pHash->nEntry == 0 ){` |
|         - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    324365 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    324365 |  408 | `		pHash->pLast = pEntry;` |
|    162180 |  409 | `	}` |
|   5193489 |  410 | `	pHash->nEntry++;` |
|   5193489 |  411 | `	return SXRET_OK;` |
|         5 |  412 | `}` |
|   5193484 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  414 | `{` |
|         - |  415 | `	SyHashEntry_Pr *pEntry;` |
|         - |  416 | `	sxi32 rc;` |
|         - |  417 | `#if defined(UNTRUST)` |
|         - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  419 | `		return SXERR_CORRUPT;` |
|         - |  420 | `	}` |
|         - |  421 | `#endif` |
|   5193489 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     31539 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     31539 |  424 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  425 | `			return rc;` |
|         - |  426 | `		}` |
|     15767 |  427 | `	}` |
|         - |  428 | `	/* Allocate a new hash entry */` |
|   5193489 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   5193489 |  430 | `	if( pEntry == 0 ){` |
|       ! 0 |  431 | `		return SXERR_MEM;` |
|         - |  432 | `	}` |
|         - |  433 | `	/* Zero the entry */` |
|   5193489 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   5193489 |  435 | `	pEntry->pHash = pHash;` |
|   5193489 |  436 | `	pEntry->pKey = pKey;` |
|   5193489 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   5193489 |  438 | `	pEntry->pUserData = pUserData;` |
|   5193489 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   5193489 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   5193489 |  442 | `	return rc;` |
|   2596747 |  443 | `}` |
|   5193368 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  445 | `{` |
|   5193373 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|         5 |  447 | `}` |
|         - |  448 | `/*` |
|         - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|         - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|         - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|         - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|         - |  453 | ` */` |
|       116 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  455 | `{` |
|       118 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|         2 |  457 | `}` |
|    171536 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  459 | `{` |
|         - |  460 | `#if defined(UNTRUST)` |
|         - |  461 | `	if( INVALID_HASH(pHash) ){` |
|         - |  462 | `		return 0;` |
|         - |  463 | `	}` |
|         - |  464 | `#endif` |
|         - |  465 | `	/* Last inserted entry */` |
|    171541 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  467 | `}` |
|         - |  468 |  |
