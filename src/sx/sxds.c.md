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
|  10226914 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  10226916 |   16 | `	pSet->nSize = 0 ;` |
|  10226916 |   17 | `	pSet->nUsed = 0;` |
|  10226916 |   18 | `	pSet->nCursor = 0;` |
|  10226916 |   19 | `	pSet->eSize = ElemSize;` |
|  10226916 |   20 | `	pSet->pAllocator = pAllocator;` |
|  10226916 |   21 | `	pSet->pBase =  0;` |
|  10226916 |   22 | `	pSet->pUserData = 0;` |
|  10226916 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  16139940 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  16139942 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3303020 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3303020 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3240060 |   34 | `			pSet->nSize = 4;` |
|   1620029 |   35 | `		}` |
|   3303020 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3303020 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3303020 |   40 | `		pSet->pBase = pNew;` |
|   3303020 |   41 | `		pSet->nSize <<= 1;` |
|   1651509 |   42 | `	}` |
|  16139942 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 121489874 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  16139942 |   45 | `	pSet->nUsed++;` |
|  16139942 |   46 | `	return SXRET_OK;` |
|   8069994 |   47 |  |
|    450604 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    450606 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    450606 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    450606 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    450606 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    450606 |   60 | `	pSet->nSize = nItem;` |
|    450606 |   61 | `	return SXRET_OK;` |
|    225304 |   62 |  |
|    883538 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|    883540 |   65 | `	pSet->nUsed   = 0;` |
|    883540 |   66 | `	pSet->nCursor = 0;` |
|    883540 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     35760 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     35762 |   71 | `	pSet->nCursor = 0;` |
|     35762 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     39228 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     39230 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     14366 |   79 | `		pSet->nCursor = 0;` |
|     14366 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     24866 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     24866 |   83 | `	if( ppEntry ){` |
|     24866 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     12432 |   85 | `	}` |
|     24866 |   86 | `	pSet->nCursor++;` |
|     24866 |   87 | `	return SXRET_OK;` |
|     19616 |   88 |  |
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
|     57116 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     57118 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     57118 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   6855102 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   6855104 |  109 | `	sxi32 rc = SXRET_OK;` |
|   6855104 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3521960 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1760979 |  112 | `	}` |
|   6855104 |  113 | `	pSet->pBase = 0;` |
|   6855104 |  114 | `	pSet->nUsed = 0;` |
|   6855104 |  115 | `	pSet->nCursor = 0;` |
|   6855104 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3339626 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3339628 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3339538 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3339538 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1669815 |  126 |  |
|   2978064 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   2978066 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2123556 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|    854512 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    854512 |  135 | `	pSet->nUsed--;` |
|    854512 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    854512 |  137 | `	return pData;` |
|   1489034 |  138 |  |
|   8713181 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   8713183 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   8713183 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   8713183 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4356844 |  148 |  |
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
|     81070 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|     81072 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     81072 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|     81072 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|     81072 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|     81072 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|     81072 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|     81072 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|     81072 |  180 | `	pHash->nEntry = 0;` |
|     81072 |  181 | `	pHash->apBucket = apNew;` |
|     81072 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|     81072 |  183 | `	return SXRET_OK;` |
|     40537 |  184 |  |
|     10224 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     10226 |  193 | `	pEntry = pHash->pList;` |
|      6030 |  194 | `	for(;;){` |
|     12062 |  195 | `		if( pHash->nEntry == 0 ){` |
|     10226 |  196 | `			break;` |
|         - |  197 | `		}` |
|      1838 |  198 | `		pNext = pEntry->pNext;` |
|      1838 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      1838 |  200 | `		pEntry = pNext;` |
|      1838 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     10226 |  203 | `	if( pHash->apBucket ){` |
|     10226 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      5112 |  205 | `	}` |
|     10226 |  206 | `	pHash->apBucket = 0;` |
|     10226 |  207 | `	pHash->nBucketSize = 0;` |
|     10226 |  208 | `	pHash->pAllocator = 0;` |
|     10226 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|   8195898 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|   8195900 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   8195900 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   7112130 |  218 | `	for(;;){` |
|  14245370 |  219 | `		if( pEntry == 0 ){` |
|   4443128 |  220 | `			break;` |
|         - |  221 | `		}` |
|  11678500 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   3752776 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   3752774 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   6049472 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   4443128 |  229 | `	return 0;` |
|   4098215 |  230 |  |
|   8241636 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|   8241638 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     45746 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|   8195894 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   8195894 |  244 | `	if( pEntry == 0 ){` |
|   4443128 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   3752768 |  247 | `	return (SyHashEntry *)pEntry;` |
|   4121084 |  248 |  |
|     65480 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     65482 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     49040 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     24521 |  254 | `	}else{` |
|     16444 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     65482 |  257 | `	if( pEntry->pNextCollide ){` |
|      3935 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      1967 |  259 | `	}` |
|     65482 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     65482 |  261 | `	pHash->nEntry--;` |
|     65482 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     65482 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     65482 |  268 | `	return rc;` |
|         2 |  269 |  |
|         6 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         1 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|         7 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|         7 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|         7 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|         7 |  284 | `	return rc;` |
|         4 |  285 |  |
|     65474 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     65476 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     65476 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     65476 |  296 | `	return rc;` |
|         2 |  297 |  |
|    117860 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    117862 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    117862 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|    820306 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|    820308 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    117428 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    117428 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    702882 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    702882 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    702882 |  324 | `	return (SyHashEntry *)pEntry;` |
|    410155 |  325 |  |
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
|      1579 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1569 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1569 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1569 |  344 | `		pEntry = pEntry->pNext;` |
|       785 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     11532 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     11534 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     11534 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     11534 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     11534 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   1580270 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   1568738 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   1568738 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   1568738 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   1568738 |  369 | `		if( apNew[iBucket] != 0 ){` |
|    753316 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    376661 |  371 | `		}` |
|   1568738 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   1568738 |  374 | `		pEntry = pEntry->pNext;` |
|    784370 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     11534 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     11534 |  378 | `	pHash->apBucket = apNew;` |
|     11534 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     11534 |  380 | `	return SXRET_OK;` |
|      5768 |  381 |  |
|   1421446 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1421448 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1421448 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1421448 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    948493 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    474252 |  389 | `	}` |
|   1421448 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1421448 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1421448 |  393 | `	if( pHash->nEntry == 0 ){` |
|     58140 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     29069 |  395 | `	}` |
|   1421448 |  396 | `	pHash->nEntry++;` |
|   1421448 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1421446 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1421448 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     11534 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     11534 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      5766 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1421448 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1421448 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1421448 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1421448 |  421 | `	pEntry->pHash = pHash;` |
|   1421448 |  422 | `	pEntry->pKey = pKey;` |
|   1421448 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1421448 |  424 | `	pEntry->pUserData = pUserData;` |
|   1421448 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1421448 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1421448 |  428 | `	return rc;` |
|    710725 |  429 |  |
|     80008 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     80010 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
