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
|  156270062 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  156270067 |   16 | `	pSet->nSize = 0 ;` |
|  156270067 |   17 | `	pSet->nUsed = 0;` |
|  156270067 |   18 | `	pSet->nCursor = 0;` |
|  156270067 |   19 | `	pSet->eSize = ElemSize;` |
|  156270067 |   20 | `	pSet->pAllocator = pAllocator;` |
|  156270067 |   21 | `	pSet->pBase =  0;` |
|  156270067 |   22 | `	pSet->pUserData = 0;` |
|  156270067 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  355468208 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  355468213 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   20582341 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   20582341 |   33 | `		if( pSet->nSize <= 0 ){` |
|   17541389 |   34 | `			pSet->nSize = 4;` |
|    8770692 |   35 | `		}` |
|   20582341 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   20582341 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   20582341 |   40 | `		pSet->pBase = pNew;` |
|   20582341 |   41 | `		pSet->nSize <<= 1;` |
|   10291168 |   42 | `	}` |
|  355468213 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2803889969 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  355468213 |   45 | `	pSet->nUsed++;` |
|  355468213 |   46 | `	return SXRET_OK;` |
|  177734131 |   47 | `}` |
|   17475166 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   17475171 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   17475171 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   17475171 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   17475171 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   17475171 |   60 | `	pSet->nSize = nItem;` |
|   17475171 |   61 | `	return SXRET_OK;` |
|    8737588 |   62 | `}` |
|   26004263 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   26004268 |   65 | `	pSet->nUsed   = 0;` |
|   26004268 |   66 | `	pSet->nCursor = 0;` |
|   26004268 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69466 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69471 |   71 | `	pSet->nCursor = 0;` |
|      69471 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73610 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73615 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30065 |   79 | `		pSet->nCursor = 0;` |
|      30065 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43555 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43555 |   83 | `	if( ppEntry ){` |
|      43555 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21775 |   85 | `	}` |
|      43555 |   86 | `	pSet->nCursor++;` |
|      43555 |   87 | `	return SXRET_OK;` |
|      36810 |   88 | `}` |
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
|    2696124 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2696129 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1181 |  103 | `		pSet->nUsed = nNewSize;` |
|        588 |  104 | `	}` |
|    2696129 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   53945258 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   53945263 |  109 | `	sxi32 rc = SXRET_OK;` |
|   53945263 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   29220447 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   14610221 |  112 | `	}` |
|   53945263 |  113 | `	pSet->pBase = 0;` |
|   53945263 |  114 | `	pSet->nUsed = 0;` |
|   53945263 |  115 | `	pSet->nCursor = 0;` |
|   53945263 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   63338960 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   63338965 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15365 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   63323605 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   63323605 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   31669485 |  126 | `}` |
|    8830482 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    8830487 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2213651 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    6616841 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    6616841 |  135 | `	pSet->nUsed--;` |
|    6616841 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    6616841 |  137 | `	return pData;` |
|    4415246 |  138 | `}` |
|   32071899 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   32071904 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   32071882 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   32071882 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   16036129 |  148 | `}` |
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
|    1763050 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1763055 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1763055 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1763055 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1763055 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1763055 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1763055 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1763055 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1763055 |  180 | `	pHash->nEntry = 0;` |
|    1763055 |  181 | `	pHash->apBucket = apNew;` |
|    1763055 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1763055 |  183 | `	return SXRET_OK;` |
|     881530 |  184 | `}` |
|     408614 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     408619 |  193 | `	pEntry = pHash->pList;` |
|     217541 |  194 | `	for(;;){` |
|     435087 |  195 | `		if( pHash->nEntry == 0 ){` |
|     408619 |  196 | `			break;` |
|          - |  197 | `		}` |
|      26473 |  198 | `		pNext = pEntry->pNext;` |
|      26473 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      26473 |  200 | `		pEntry = pNext;` |
|      26473 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     408619 |  203 | `	if( pHash->apBucket ){` |
|     408619 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     204307 |  205 | `	}` |
|     408619 |  206 | `	pHash->apBucket = 0;` |
|     408619 |  207 | `	pHash->nBucketSize = 0;` |
|     408619 |  208 | `	pHash->pAllocator = 0;` |
|     408619 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   65549965 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   65549970 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   65549970 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   59053603 |  218 | `	for(;;){` |
|  118004051 |  219 | `		if( pEntry == 0 ){` |
|   23813339 |  220 | `			break;` |
|          - |  221 | `		}` |
|  115058969 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   41736781 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   41736636 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   52454086 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   23813339 |  229 | `	return 0;` |
|   32775266 |  230 | `}` |
|   72363837 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   72363842 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    6814229 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   65549618 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   65549618 |  244 | `	if( pEntry == 0 ){` |
|   23813321 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   41736302 |  247 | `	return (SyHashEntry *)pEntry;` |
|   36182202 |  248 | `}` |
|     423786 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     423791 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     348247 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     174126 |  254 | `	}else{` |
|      75549 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     423791 |  257 | `	if( pEntry->pNextCollide ){` |
|       4208 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2103 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     423791 |  261 | `	if( pHash->pLast == pEntry ){` |
|     416079 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     208037 |  263 | `	}` |
|     423791 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     423791 |  265 | `	pHash->nEntry--;` |
|     423791 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     423791 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     423791 |  272 | `	return rc;` |
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
|     423452 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     423457 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     423457 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     423457 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2822846 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2822851 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2822851 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   20975520 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   20975525 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2822585 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2822585 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18152945 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18152945 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18152945 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10487765 |  329 | `}` |
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
|       4007 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3997 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3997 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3997 |  348 | `		pEntry = pEntry->pNext;` |
|       1999 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      91674 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      91679 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      91679 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      91679 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      91679 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14391071 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14299397 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14299397 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14299397 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14299397 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    6897669 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3449003 |  375 | `		}` |
|   14299397 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14299397 |  378 | `		pEntry = pEntry->pNext;` |
|    7149701 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      91679 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      91679 |  382 | `	pHash->apBucket = apNew;` |
|      91679 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      91679 |  384 | `	return SXRET_OK;` |
|      45842 |  385 | `}` |
|   18072936 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   18072941 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   18072941 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   18072941 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11361907 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5680828 |  393 | `	}` |
|   18072941 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   18072941 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   18072889 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   18072941 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     971551 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     971551 |  408 | `		pHash->pLast = pEntry;` |
|     485773 |  409 | `	}` |
|   18072941 |  410 | `	pHash->nEntry++;` |
|   18072941 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   18072936 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   18072941 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      91679 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      91679 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45837 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   18072941 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   18072941 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   18072941 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   18072941 |  435 | `	pEntry->pHash = pHash;` |
|   18072941 |  436 | `	pEntry->pKey = pKey;` |
|   18072941 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   18072941 |  438 | `	pEntry->pUserData = pUserData;` |
|   18072941 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   18072941 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   18072941 |  442 | `	return rc;` |
|    9036473 |  443 | `}` |
|   18072804 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   18072809 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     463066 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     463071 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
