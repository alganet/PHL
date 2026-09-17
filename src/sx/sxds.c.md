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
|  184442743 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  184442748 |   16 | `	pSet->nSize = 0 ;` |
|  184442748 |   17 | `	pSet->nUsed = 0;` |
|  184442748 |   18 | `	pSet->nCursor = 0;` |
|  184442748 |   19 | `	pSet->eSize = ElemSize;` |
|  184442748 |   20 | `	pSet->pAllocator = pAllocator;` |
|  184442748 |   21 | `	pSet->pBase =  0;` |
|  184442748 |   22 | `	pSet->pUserData = 0;` |
|  184442748 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  417935844 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  417935849 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   24169113 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   24169113 |   33 | `		if( pSet->nSize <= 0 ){` |
|   20601425 |   34 | `			pSet->nSize = 4;` |
|   10301659 |   35 | `		}` |
|   24169113 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   24169113 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   24169113 |   40 | `		pSet->pBase = pNew;` |
|   24169113 |   41 | `		pSet->nSize <<= 1;` |
|   12085503 |   42 | `	}` |
|  417935849 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3299284813 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  417935849 |   45 | `	pSet->nUsed++;` |
|  417935849 |   46 | `	return SXRET_OK;` |
|  208971146 |   47 | `}` |
|   20768230 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   20768235 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   20768235 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   20768235 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   20768235 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   20768235 |   60 | `	pSet->nSize = nItem;` |
|   20768235 |   61 | `	return SXRET_OK;` |
|   10384120 |   62 | `}` |
|   30757841 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   30757846 |   65 | `	pSet->nUsed   = 0;` |
|   30757846 |   66 | `	pSet->nCursor = 0;` |
|   30757846 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69238 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69243 |   71 | `	pSet->nCursor = 0;` |
|      69243 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      69492 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      69497 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30151 |   79 | `		pSet->nCursor = 0;` |
|      30151 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      39351 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      39351 |   83 | `	if( ppEntry ){` |
|      39351 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      19673 |   85 | `	}` |
|      39351 |   86 | `	pSet->nCursor++;` |
|      39351 |   87 | `	return SXRET_OK;` |
|      34751 |   88 | `}` |
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
|    3281556 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3281561 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1253 |  103 | `		pSet->nUsed = nNewSize;` |
|        624 |  104 | `	}` |
|    3281561 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   63116121 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   63116126 |  109 | `	sxi32 rc = SXRET_OK;` |
|   63116126 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   34165143 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   17083518 |  112 | `	}` |
|   63116126 |  113 | `	pSet->pBase = 0;` |
|   63116126 |  114 | `	pSet->nUsed = 0;` |
|   63116126 |  115 | `	pSet->nCursor = 0;` |
|   63116126 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   74264610 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   74264615 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       4009 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   74260611 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   74260611 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   37132310 |  126 | `}` |
|    9866847 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9866852 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2237639 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7629218 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7629218 |  135 | `	pSet->nUsed--;` |
|    7629218 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7629218 |  137 | `	return pData;` |
|    4934061 |  138 | `}` |
|   37312252 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   37312257 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   37312205 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   37312205 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   18660327 |  148 | `}` |
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
|    2185135 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    2185140 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2185140 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    2185140 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2185140 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    2185140 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    2185140 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    2185140 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    2185140 |  180 | `	pHash->nEntry = 0;` |
|    2185140 |  181 | `	pHash->apBucket = apNew;` |
|    2185140 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    2185140 |  183 | `	return SXRET_OK;` |
|    1092678 |  184 | `}` |
|     521613 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     521618 |  193 | `	pEntry = pHash->pList;` |
|     276445 |  194 | `	for(;;){` |
|     552684 |  195 | `		if( pHash->nEntry == 0 ){` |
|     521618 |  196 | `			break;` |
|          - |  197 | `		}` |
|      31071 |  198 | `		pNext = pEntry->pNext;` |
|      31071 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      31071 |  200 | `		pEntry = pNext;` |
|      31071 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     521618 |  203 | `	if( pHash->apBucket ){` |
|     521618 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     260912 |  205 | `	}` |
|     521618 |  206 | `	pHash->apBucket = 0;` |
|     521618 |  207 | `	pHash->nBucketSize = 0;` |
|     521618 |  208 | `	pHash->pAllocator = 0;` |
|     521618 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   79507382 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   79507387 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   79507387 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   71383496 |  218 | `	for(;;){` |
|  142713799 |  219 | `		if( pEntry == 0 ){` |
|   31592544 |  220 | `			break;` |
|          - |  221 | `		}` |
|  135078089 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   47918847 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   47914848 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   63206417 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   31592544 |  229 | `	return 0;` |
|   39759950 |  230 | `}` |
|   89424993 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   89424998 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    9918074 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   79506929 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   79506929 |  244 | `	if( pEntry == 0 ){` |
|   31592526 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   47914408 |  247 | `	return (SyHashEntry *)pEntry;` |
|   44718861 |  248 | `}` |
|     516250 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     516255 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     420347 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     210705 |  254 | `	}else{` |
|      95913 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     516255 |  257 | `	if( pEntry->pNextCollide ){` |
|       4394 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2197 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     516255 |  261 | `	if( pHash->pLast == pEntry ){` |
|     506691 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     253976 |  263 | `	}` |
|     516255 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     516255 |  265 | `	pHash->nEntry--;` |
|     516255 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     516255 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     516255 |  272 | `	return rc;` |
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
|     515810 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     515815 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     515815 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     515815 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3423170 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3423175 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3423175 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   26371438 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   26371443 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3422909 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3422909 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   22948539 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   22948539 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   22948539 |  328 | `	return (SyHashEntry *)pEntry;` |
|   13185724 |  329 | `}` |
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
|       4859 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4845 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4845 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4845 |  348 | `		pEntry = pEntry->pNext;` |
|       2423 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|     100520 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100525 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100525 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100525 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100525 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18602125 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18501605 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18501605 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18501605 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18501605 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8925256 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4462425 |  375 | `		}` |
|   18501605 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18501605 |  378 | `		pEntry = pEntry->pNext;` |
|    9250805 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100525 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100525 |  382 | `	pHash->apBucket = apNew;` |
|     100525 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100525 |  384 | `	return SXRET_OK;` |
|      50265 |  385 | `}` |
|   22564086 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   22564091 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   22564091 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   22564091 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   14167261 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    7083597 |  393 | `	}` |
|   22564091 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   22564091 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|     880423 |  399 | `		pHash->pLast->pNext = pEntry;` |
|     880423 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|     880423 |  401 | `		pHash->pLast = pEntry;` |
|     440214 |  402 | `	}else{` |
|   21683673 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   22564091 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1213648 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1213648 |  408 | `		pHash->pLast = pEntry;` |
|     606927 |  409 | `	}` |
|   22564091 |  410 | `	pHash->nEntry++;` |
|   22564091 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   22564086 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   22564091 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100525 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100525 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50260 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   22564091 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   22564091 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   22564091 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   22564091 |  435 | `	pEntry->pHash = pHash;` |
|   22564091 |  436 | `	pEntry->pKey = pKey;` |
|   22564091 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   22564091 |  438 | `	pEntry->pUserData = pUserData;` |
|   22564091 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   22564091 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   22564091 |  442 | `	return rc;` |
|   11282681 |  443 | `}` |
|   21422284 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   21422289 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|    1141802 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  455 | `{` |
|    1141807 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          5 |  457 | `}` |
|     556158 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     556163 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
