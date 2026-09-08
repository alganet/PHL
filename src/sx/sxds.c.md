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
|  146285446 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  146285451 |   16 | `	pSet->nSize = 0 ;` |
|  146285451 |   17 | `	pSet->nUsed = 0;` |
|  146285451 |   18 | `	pSet->nCursor = 0;` |
|  146285451 |   19 | `	pSet->eSize = ElemSize;` |
|  146285451 |   20 | `	pSet->pAllocator = pAllocator;` |
|  146285451 |   21 | `	pSet->pBase =  0;` |
|  146285451 |   22 | `	pSet->pUserData = 0;` |
|  146285451 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  330240591 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  330240596 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   19292399 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   19292399 |   33 | `		if( pSet->nSize <= 0 ){` |
|   16463433 |   34 | `			pSet->nSize = 4;` |
|    8231714 |   35 | `		}` |
|   19292399 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   19292399 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   19292399 |   40 | `		pSet->pBase = pNew;` |
|   19292399 |   41 | `		pSet->nSize <<= 1;` |
|    9646197 |   42 | `	}` |
|  330240596 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2600432384 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  330240596 |   45 | `	pSet->nUsed++;` |
|  330240596 |   46 | `	return SXRET_OK;` |
|  165120324 |   47 | `}` |
|   16189590 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   16189595 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   16189595 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   16189595 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   16189595 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   16189595 |   60 | `	pSet->nSize = nItem;` |
|   16189595 |   61 | `	return SXRET_OK;` |
|    8094800 |   62 | `}` |
|   23624229 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   23624234 |   65 | `	pSet->nUsed   = 0;` |
|   23624234 |   66 | `	pSet->nCursor = 0;` |
|   23624234 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69086 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69091 |   71 | `	pSet->nCursor = 0;` |
|      69091 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73184 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73189 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29901 |   79 | `		pSet->nCursor = 0;` |
|      29901 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43293 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43293 |   83 | `	if( ppEntry ){` |
|      43293 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21644 |   85 | `	}` |
|      43293 |   86 | `	pSet->nCursor++;` |
|      43293 |   87 | `	return SXRET_OK;` |
|      36597 |   88 | `}` |
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
|    2580144 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2580149 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1181 |  103 | `		pSet->nUsed = nNewSize;` |
|        588 |  104 | `	}` |
|    2580149 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   50460668 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   50460673 |  109 | `	sxi32 rc = SXRET_OK;` |
|   50460673 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   27162459 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   13581227 |  112 | `	}` |
|   50460673 |  113 | `	pSet->pBase = 0;` |
|   50460673 |  114 | `	pSet->nUsed = 0;` |
|   50460673 |  115 | `	pSet->nCursor = 0;` |
|   50460673 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   59288354 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   59288359 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15333 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   59273031 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   59273031 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   29644182 |  126 | `}` |
|    8324040 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    8324045 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2212797 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    6111253 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    6111253 |  135 | `	pSet->nUsed--;` |
|    6111253 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    6111253 |  137 | `	return pData;` |
|    4162025 |  138 | `}` |
|   30557332 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   30557337 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   30557315 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   30557315 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   15278762 |  148 | `}` |
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
|    1758450 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1758455 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1758455 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1758455 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1758455 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1758455 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1758455 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1758455 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1758455 |  180 | `	pHash->nEntry = 0;` |
|    1758455 |  181 | `	pHash->apBucket = apNew;` |
|    1758455 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1758455 |  183 | `	return SXRET_OK;` |
|     879230 |  184 | `}` |
|     406922 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     406927 |  193 | `	pEntry = pHash->pList;` |
|     216513 |  194 | `	for(;;){` |
|     433031 |  195 | `		if( pHash->nEntry == 0 ){` |
|     406927 |  196 | `			break;` |
|          - |  197 | `		}` |
|      26109 |  198 | `		pNext = pEntry->pNext;` |
|      26109 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      26109 |  200 | `		pEntry = pNext;` |
|      26109 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     406927 |  203 | `	if( pHash->apBucket ){` |
|     406927 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     203461 |  205 | `	}` |
|     406927 |  206 | `	pHash->apBucket = 0;` |
|     406927 |  207 | `	pHash->nBucketSize = 0;` |
|     406927 |  208 | `	pHash->pAllocator = 0;` |
|     406927 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   62968105 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   62968110 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   62968110 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   57998201 |  218 | `	for(;;){` |
|  115694953 |  219 | `		if( pEntry == 0 ){` |
|   23293762 |  220 | `			break;` |
|          - |  221 | `		}` |
|  112238268 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   39674426 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   39674353 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   52726848 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   23293762 |  229 | `	return 0;` |
|   31484341 |  230 | `}` |
|   69340417 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   69340422 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    6372669 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   62967758 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   62967758 |  244 | `	if( pEntry == 0 ){` |
|   23293744 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   39674019 |  247 | `	return (SyHashEntry *)pEntry;` |
|   34670497 |  248 | `}` |
|     420954 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     420959 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     345813 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     172909 |  254 | `	}else{` |
|      75151 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     420959 |  257 | `	if( pEntry->pNextCollide ){` |
|       4404 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2201 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     420959 |  261 | `	if( pHash->pLast == pEntry ){` |
|     413931 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     206963 |  263 | `	}` |
|     420959 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     420959 |  265 | `	pHash->nEntry--;` |
|     420959 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     420959 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     420959 |  272 | `	return rc;` |
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
|     420620 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     420625 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     420625 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     420625 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2815932 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2815937 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2815937 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   20917690 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   20917695 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2815671 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2815671 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18102029 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18102029 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18102029 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10458850 |  329 | `}` |
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
|       3913 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3903 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3903 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3903 |  348 | `		pEntry = pEntry->pNext;` |
|       1952 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      91016 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      91021 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      91021 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      91021 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      91021 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14314957 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14223941 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14223941 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14223941 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14223941 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    6843046 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3421890 |  375 | `		}` |
|   14223941 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14223941 |  378 | `		pEntry = pEntry->pNext;` |
|    7111973 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      91021 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      91021 |  382 | `	pHash->apBucket = apNew;` |
|      91021 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      91021 |  384 | `	return SXRET_OK;` |
|      45513 |  385 | `}` |
|   17585716 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   17585721 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   17585721 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   17585721 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   10930789 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5464961 |  393 | `	}` |
|   17585721 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   17585721 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   17585669 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   17585721 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     968907 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     968907 |  408 | `		pHash->pLast = pEntry;` |
|     484451 |  409 | `	}` |
|   17585721 |  410 | `	pHash->nEntry++;` |
|   17585721 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   17585716 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   17585721 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      91021 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      91021 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45508 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   17585721 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   17585721 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   17585721 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   17585721 |  435 | `	pEntry->pHash = pHash;` |
|   17585721 |  436 | `	pEntry->pKey = pKey;` |
|   17585721 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   17585721 |  438 | `	pEntry->pUserData = pUserData;` |
|   17585721 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   17585721 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   17585721 |  442 | `	return rc;` |
|    8792863 |  443 | `}` |
|   17585584 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   17585589 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     460138 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     460143 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
