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
|  144694692 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  144694697 |   16 | `	pSet->nSize = 0 ;` |
|  144694697 |   17 | `	pSet->nUsed = 0;` |
|  144694697 |   18 | `	pSet->nCursor = 0;` |
|  144694697 |   19 | `	pSet->eSize = ElemSize;` |
|  144694697 |   20 | `	pSet->pAllocator = pAllocator;` |
|  144694697 |   21 | `	pSet->pBase =  0;` |
|  144694697 |   22 | `	pSet->pUserData = 0;` |
|  144694697 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  324055270 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  324055275 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   18868063 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   18868063 |   33 | `		if( pSet->nSize <= 0 ){` |
|   16113037 |   34 | `			pSet->nSize = 4;` |
|    8056516 |   35 | `		}` |
|   18868063 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   18868063 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   18868063 |   40 | `		pSet->pBase = pNew;` |
|   18868063 |   41 | `		pSet->nSize <<= 1;` |
|    9434029 |   42 | `	}` |
|  324055275 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2403061021 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  324055275 |   45 | `	pSet->nUsed++;` |
|  324055275 |   46 | `	return SXRET_OK;` |
|  162027681 |   47 | `}` |
|   15988658 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   15988663 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   15988663 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   15988663 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   15988663 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   15988663 |   60 | `	pSet->nSize = nItem;` |
|   15988663 |   61 | `	return SXRET_OK;` |
|    7994334 |   62 | `}` |
|   22950155 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   22950160 |   65 | `	pSet->nUsed   = 0;` |
|   22950160 |   66 | `	pSet->nCursor = 0;` |
|   22950160 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69644 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69649 |   71 | `	pSet->nCursor = 0;` |
|      69649 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73902 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73907 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29993 |   79 | `		pSet->nCursor = 0;` |
|      29993 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43919 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43919 |   83 | `	if( ppEntry ){` |
|      43919 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21957 |   85 | `	}` |
|      43919 |   86 | `	pSet->nCursor++;` |
|      43919 |   87 | `	return SXRET_OK;` |
|      36956 |   88 | `}` |
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
|    2627420 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2627425 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1181 |  103 | `		pSet->nUsed = nNewSize;` |
|        588 |  104 | `	}` |
|    2627425 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   49651626 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   49651631 |  109 | `	sxi32 rc = SXRET_OK;` |
|   49651631 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   26530243 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   13265119 |  112 | `	}` |
|   49651631 |  113 | `	pSet->pBase = 0;` |
|   49651631 |  114 | `	pSet->nUsed = 0;` |
|   49651631 |  115 | `	pSet->nCursor = 0;` |
|   49651631 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   58523160 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   58523165 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15869 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   58507301 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   58507301 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   29261585 |  126 | `}` |
|    7964066 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    7964071 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2223505 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    5740571 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    5740571 |  135 | `	pSet->nUsed--;` |
|    5740571 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    5740571 |  137 | `	return pData;` |
|    3982038 |  138 | `}` |
|   28744671 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   28744676 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   28744654 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   28744654 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   14372666 |  148 | `}` |
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
|    1784742 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1784747 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1784747 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1784747 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1784747 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1784747 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1784747 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1784747 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1784747 |  180 | `	pHash->nEntry = 0;` |
|    1784747 |  181 | `	pHash->apBucket = apNew;` |
|    1784747 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1784747 |  183 | `	return SXRET_OK;` |
|     892376 |  184 | `}` |
|     386194 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     386199 |  193 | `	pEntry = pHash->pList;` |
|     205816 |  194 | `	for(;;){` |
|     411637 |  195 | `		if( pHash->nEntry == 0 ){` |
|     386199 |  196 | `			break;` |
|          - |  197 | `		}` |
|      25443 |  198 | `		pNext = pEntry->pNext;` |
|      25443 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      25443 |  200 | `		pEntry = pNext;` |
|      25443 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     386199 |  203 | `	if( pHash->apBucket ){` |
|     386199 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     193097 |  205 | `	}` |
|     386199 |  206 | `	pHash->apBucket = 0;` |
|     386199 |  207 | `	pHash->nBucketSize = 0;` |
|     386199 |  208 | `	pHash->pAllocator = 0;` |
|     386199 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   61038423 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   61038428 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   61038428 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   56527359 |  218 | `	for(;;){` |
|  113220747 |  219 | `		if( pEntry == 0 ){` |
|   22475963 |  220 | `			break;` |
|          - |  221 | `		}` |
|  110025821 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   38562569 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   38562470 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   52182324 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   22475963 |  229 | `	return 0;` |
|   30519723 |  230 | `}` |
|   67383835 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   67383840 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    6345769 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   61038076 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   61038076 |  244 | `	if( pEntry == 0 ){` |
|   22475945 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   38562136 |  247 | `	return (SyHashEntry *)pEntry;` |
|   33692429 |  248 | `}` |
|     234692 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     234697 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     189717 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      94861 |  254 | `	}else{` |
|      44985 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     234697 |  257 | `	if( pEntry->pNextCollide ){` |
|       4400 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2199 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     234697 |  261 | `	if( pHash->pLast == pEntry ){` |
|     227617 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     113806 |  263 | `	}` |
|     234697 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     234697 |  265 | `	pHash->nEntry--;` |
|     234697 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     234697 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     234697 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        352 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        357 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        357 |  284 | `	if( pEntry == 0 ){` |
|         19 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        339 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        339 |  288 | `	return rc;` |
|        181 |  289 | `}` |
|     234358 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     234363 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     234363 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     234363 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2928846 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2928851 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2928851 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   21784050 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   21784055 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2928585 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2928585 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18855475 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18855475 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18855475 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10892030 |  329 | `}` |
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
|       3829 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3819 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3819 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3819 |  348 | `		pEntry = pEntry->pNext;` |
|       1910 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      94978 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      94983 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      94983 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      94983 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      94983 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14960583 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14865605 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14865605 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14865605 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14865605 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    7112432 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3556401 |  375 | `		}` |
|   14865605 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14865605 |  378 | `		pEntry = pEntry->pNext;` |
|    7432805 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      94983 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      94983 |  382 | `	pHash->apBucket = apNew;` |
|      94983 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      94983 |  384 | `	return SXRET_OK;` |
|      47494 |  385 | `}` |
|   17968828 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   17968833 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   17968833 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   17968833 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11287104 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5643393 |  393 | `	}` |
|   17968833 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   17968833 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   17968781 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   17968833 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     967997 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     967997 |  408 | `		pHash->pLast = pEntry;` |
|     483996 |  409 | `	}` |
|   17968833 |  410 | `	pHash->nEntry++;` |
|   17968833 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   17968828 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   17968833 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      94983 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      94983 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      47489 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   17968833 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   17968833 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   17968833 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   17968833 |  435 | `	pEntry->pHash = pHash;` |
|   17968833 |  436 | `	pEntry->pKey = pKey;` |
|   17968833 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   17968833 |  438 | `	pEntry->pUserData = pUserData;` |
|   17968833 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   17968833 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   17968833 |  442 | `	return rc;` |
|    8984419 |  443 | `}` |
|   17968696 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   17968701 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        132 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          2 |  455 | `{` |
|        134 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          2 |  457 | `}` |
|     276088 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     276093 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
