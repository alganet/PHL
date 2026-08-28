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
|  101357326 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  101357331 |   16 | `	pSet->nSize = 0 ;` |
|  101357331 |   17 | `	pSet->nUsed = 0;` |
|  101357331 |   18 | `	pSet->nCursor = 0;` |
|  101357331 |   19 | `	pSet->eSize = ElemSize;` |
|  101357331 |   20 | `	pSet->pAllocator = pAllocator;` |
|  101357331 |   21 | `	pSet->pBase =  0;` |
|  101357331 |   22 | `	pSet->pUserData = 0;` |
|  101357331 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  223779553 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  223779558 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   14125071 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   14125071 |   33 | `		if( pSet->nSize <= 0 ){` |
|   12285419 |   34 | `			pSet->nSize = 4;` |
|    6142707 |   35 | `		}` |
|   14125071 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   14125071 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   14125071 |   40 | `		pSet->pBase = pNew;` |
|   14125071 |   41 | `		pSet->nSize <<= 1;` |
|    7062533 |   42 | `	}` |
|  223779558 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 1655174074 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  223779558 |   45 | `	pSet->nUsed++;` |
|  223779558 |   46 | `	return SXRET_OK;` |
|  111889824 |   47 | `}` |
|   10785816 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   10785821 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   10785821 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   10785821 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   10785821 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   10785821 |   60 | `	pSet->nSize = nItem;` |
|   10785821 |   61 | `	return SXRET_OK;` |
|    5392913 |   62 | `}` |
|   16257691 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   16257696 |   65 | `	pSet->nUsed   = 0;` |
|   16257696 |   66 | `	pSet->nCursor = 0;` |
|   16257696 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69166 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69171 |   71 | `	pSet->nCursor = 0;` |
|      69171 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73404 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73409 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29773 |   79 | `		pSet->nCursor = 0;` |
|      29773 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43641 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43641 |   83 | `	if( ppEntry ){` |
|      43641 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21818 |   85 | `	}` |
|      43641 |   86 | `	pSet->nCursor++;` |
|      43641 |   87 | `	return SXRET_OK;` |
|      36707 |   88 | `}` |
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
|    1686852 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    1686857 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1179 |  103 | `		pSet->nUsed = nNewSize;` |
|        587 |  104 | `	}` |
|    1686857 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   35947436 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   35947441 |  109 | `	sxi32 rc = SXRET_OK;` |
|   35947441 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   19348485 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|    9674240 |  112 | `	}` |
|   35947441 |  113 | `	pSet->pBase = 0;` |
|   35947441 |  114 | `	pSet->nUsed = 0;` |
|   35947441 |  115 | `	pSet->nCursor = 0;` |
|   35947441 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   39537322 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   39537327 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15621 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   39521711 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   39521711 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   19768666 |  126 | `}` |
|    6650010 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    6650015 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2198967 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    4451053 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    4451053 |  135 | `	pSet->nUsed--;` |
|    4451053 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    4451053 |  137 | `	return pData;` |
|    3325010 |  138 | `}` |
|   23617232 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   23617237 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   23617215 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   23617215 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   11808974 |  148 | `}` |
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
|    1329884 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1329889 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1329889 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1329889 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1329889 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1329889 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1329889 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1329889 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1329889 |  180 | `	pHash->nEntry = 0;` |
|    1329889 |  181 | `	pHash->apBucket = apNew;` |
|    1329889 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1329889 |  183 | `	return SXRET_OK;` |
|     664947 |  184 | `}` |
|     329280 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     329285 |  193 | `	pEntry = pHash->pList;` |
|     176490 |  194 | `	for(;;){` |
|     352985 |  195 | `		if( pHash->nEntry == 0 ){` |
|     329285 |  196 | `			break;` |
|          - |  197 | `		}` |
|      23705 |  198 | `		pNext = pEntry->pNext;` |
|      23705 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      23705 |  200 | `		pEntry = pNext;` |
|      23705 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     329285 |  203 | `	if( pHash->apBucket ){` |
|     329285 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     164640 |  205 | `	}` |
|     329285 |  206 | `	pHash->apBucket = 0;` |
|     329285 |  207 | `	pHash->nBucketSize = 0;` |
|     329285 |  208 | `	pHash->pAllocator = 0;` |
|     329285 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   46518489 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   46518494 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   46518494 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   43834659 |  218 | `	for(;;){` |
|   87508668 |  219 | `		if( pEntry == 0 ){` |
|   18044710 |  220 | `			break;` |
|          - |  221 | `		}` |
|   83700630 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   28473844 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   28473789 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   40990179 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   18044710 |  229 | `	return 0;` |
|   23259761 |  230 | `}` |
|   50799023 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   50799028 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    4280861 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   46518172 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   46518172 |  244 | `	if( pEntry == 0 ){` |
|   18044710 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   28473467 |  247 | `	return (SyHashEntry *)pEntry;` |
|   25400028 |  248 | `}` |
|     220492 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     220497 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     176989 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      88497 |  254 | `	}else{` |
|      43513 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     220497 |  257 | `	if( pEntry->pNextCollide ){` |
|       4196 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2097 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     220497 |  261 | `	if( pHash->pLast == pEntry ){` |
|     213655 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     106825 |  263 | `	}` |
|     220497 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     220497 |  265 | `	pHash->nEntry--;` |
|     220497 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|        ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        ! 0 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     220497 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     220497 |  272 | `	return rc;` |
|          5 |  273 | `}` |
|        322 |  274 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|          5 |  275 | `{` |
|          - |  276 | `	SyHashEntry_Pr *pEntry;` |
|          - |  277 | `	sxi32 rc;` |
|          - |  278 | `#if defined(UNTRUST)` |
|          - |  279 | `	if( INVALID_HASH(pHash) ){` |
|          - |  280 | `		return SXERR_CORRUPT;` |
|          - |  281 | `	}` |
|          - |  282 | `#endif` |
|        327 |  283 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|        327 |  284 | `	if( pEntry == 0 ){` |
|        ! 0 |  285 | `		return SXERR_NOTFOUND;` |
|          - |  286 | `	}` |
|        327 |  287 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|        327 |  288 | `	return rc;` |
|        166 |  289 | `}` |
|     220170 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     220175 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     220175 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     220175 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2078274 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2078279 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2078279 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   15772900 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   15772905 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2078013 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2078013 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   13694897 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   13694897 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   13694897 |  328 | `	return (SyHashEntry *)pEntry;` |
|    7886455 |  329 | `}` |
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
|       3291 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3281 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3281 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3281 |  348 | `		pEntry = pEntry->pNext;` |
|       1641 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      81878 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      81883 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      81883 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      81883 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      81883 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   10626331 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   10544453 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   10544453 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   10544453 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   10544453 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    5061994 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    2531118 |  375 | `		}` |
|   10544453 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   10544453 |  378 | `		pEntry = pEntry->pNext;` |
|    5272229 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      81883 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      81883 |  382 | `	pHash->apBucket = apNew;` |
|      81883 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      81883 |  384 | `	return SXRET_OK;` |
|      40944 |  385 | `}` |
|   13429972 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   13429977 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   13429977 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   13429977 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    8398928 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    4199426 |  393 | `	}` |
|   13429977 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   13429977 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   13429925 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   13429977 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     707847 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     707847 |  408 | `		pHash->pLast = pEntry;` |
|     353921 |  409 | `	}` |
|   13429977 |  410 | `	pHash->nEntry++;` |
|   13429977 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   13429972 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   13429977 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      81883 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      81883 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      40939 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   13429977 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   13429977 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   13429977 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   13429977 |  435 | `	pEntry->pHash = pHash;` |
|   13429977 |  436 | `	pEntry->pKey = pKey;` |
|   13429977 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   13429977 |  438 | `	pEntry->pUserData = pUserData;` |
|   13429977 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   13429977 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   13429977 |  442 | `	return rc;` |
|    6714991 |  443 | `}` |
|   13429844 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   13429849 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     261148 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     261153 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
