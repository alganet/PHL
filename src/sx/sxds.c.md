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
|  183973329 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  183973334 |   16 | `	pSet->nSize = 0 ;` |
|  183973334 |   17 | `	pSet->nUsed = 0;` |
|  183973334 |   18 | `	pSet->nCursor = 0;` |
|  183973334 |   19 | `	pSet->eSize = ElemSize;` |
|  183973334 |   20 | `	pSet->pAllocator = pAllocator;` |
|  183973334 |   21 | `	pSet->pBase =  0;` |
|  183973334 |   22 | `	pSet->pUserData = 0;` |
|  183973334 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  416847576 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  416847581 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   24106363 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   24106363 |   33 | `		if( pSet->nSize <= 0 ){` |
|   20547867 |   34 | `			pSet->nSize = 4;` |
|   10274853 |   35 | `		}` |
|   24106363 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   24106363 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   24106363 |   40 | `		pSet->pBase = pNew;` |
|   24106363 |   41 | `		pSet->nSize <<= 1;` |
|   12054101 |   42 | `	}` |
|  416847581 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 3290808161 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  416847581 |   45 | `	pSet->nUsed++;` |
|  416847581 |   46 | `	return SXRET_OK;` |
|  208426922 |   47 | `}` |
|   20714738 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   20714743 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   20714743 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   20714743 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   20714743 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   20714743 |   60 | `	pSet->nSize = nItem;` |
|   20714743 |   61 | `	return SXRET_OK;` |
|   10357374 |   62 | `}` |
|   30676000 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   30676005 |   65 | `	pSet->nUsed   = 0;` |
|   30676005 |   66 | `	pSet->nCursor = 0;` |
|   30676005 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69148 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69153 |   71 | `	pSet->nCursor = 0;` |
|      69153 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      69408 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      69413 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30087 |   79 | `		pSet->nCursor = 0;` |
|      30087 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      39331 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      39331 |   83 | `	if( ppEntry ){` |
|      39331 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      19663 |   85 | `	}` |
|      39331 |   86 | `	pSet->nCursor++;` |
|      39331 |   87 | `	return SXRET_OK;` |
|      34709 |   88 | `}` |
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
|    3273090 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    3273095 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1253 |  103 | `		pSet->nUsed = nNewSize;` |
|        624 |  104 | `	}` |
|    3273095 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   62955211 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   62955216 |  109 | `	sxi32 rc = SXRET_OK;` |
|   62955216 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   34076281 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   17039060 |  112 | `	}` |
|   62955216 |  113 | `	pSet->pBase = 0;` |
|   62955216 |  114 | `	pSet->nUsed = 0;` |
|   62955216 |  115 | `	pSet->nCursor = 0;` |
|   62955216 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   74078388 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   74078393 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       3995 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   74074403 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   74074403 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   37039199 |  126 | `}` |
|    9844259 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    9844264 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2237635 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    7606634 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    7606634 |  135 | `	pSet->nUsed--;` |
|    7606634 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    7606634 |  137 | `	return pData;` |
|    4922749 |  138 | `}` |
|   37138226 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   37138231 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         54 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   37138179 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   37138179 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   18573222 |  148 | `}` |
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
|    2178709 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    2178714 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2178714 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    2178714 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    2178714 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    2178714 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    2178714 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    2178714 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    2178714 |  180 | `	pHash->nEntry = 0;` |
|    2178714 |  181 | `	pHash->apBucket = apNew;` |
|    2178714 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    2178714 |  183 | `	return SXRET_OK;` |
|    1089462 |  184 | `}` |
|     519499 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     519504 |  193 | `	pEntry = pHash->pList;` |
|     275383 |  194 | `	for(;;){` |
|     550566 |  195 | `		if( pHash->nEntry == 0 ){` |
|     519504 |  196 | `			break;` |
|          - |  197 | `		}` |
|      31067 |  198 | `		pNext = pEntry->pNext;` |
|      31067 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      31067 |  200 | `		pEntry = pNext;` |
|      31067 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     519504 |  203 | `	if( pHash->apBucket ){` |
|     519504 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     259852 |  205 | `	}` |
|     519504 |  206 | `	pHash->apBucket = 0;` |
|     519504 |  207 | `	pHash->nBucketSize = 0;` |
|     519504 |  208 | `	pHash->pAllocator = 0;` |
|     519504 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   75026234 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   75026239 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   75026239 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   67872828 |  218 | `	for(;;){` |
|  135794909 |  219 | `		if( pEntry == 0 ){` |
|   27281479 |  220 | `			break;` |
|          - |  221 | `		}` |
|  132385288 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   47748754 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   47744765 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   60768675 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   27281479 |  229 | `	return 0;` |
|   37519205 |  230 | `}` |
|   83996635 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   83996640 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    8970864 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   75025781 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   75025781 |  244 | `	if( pEntry == 0 ){` |
|   27281461 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   47744325 |  247 | `	return (SyHashEntry *)pEntry;` |
|   42004508 |  248 | `}` |
|     507848 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     507853 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     413349 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     207190 |  254 | `	}else{` |
|      94509 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     507853 |  257 | `	if( pEntry->pNextCollide ){` |
|       4412 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2206 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     507853 |  261 | `	if( pHash->pLast == pEntry ){` |
|     498303 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     249764 |  263 | `	}` |
|     507853 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     507853 |  265 | `	pHash->nEntry--;` |
|     507853 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     507853 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     507853 |  272 | `	return rc;` |
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
|     507408 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     507413 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     507413 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     507413 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    3419112 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    3419117 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    3419117 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   26352786 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   26352791 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    3418851 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    3418851 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   22933945 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   22933945 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   22933945 |  328 | `	return (SyHashEntry *)pEntry;` |
|   13176398 |  329 | `}` |
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
|       4845 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4831 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4831 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4831 |  348 | `		pEntry = pEntry->pNext;` |
|       2416 |  349 | `	}` |
|         15 |  350 | `	return SXRET_OK;` |
|          8 |  351 | `}` |
|     100358 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|     100363 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|     100363 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     100363 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|     100363 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   18582763 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   18482405 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   18482405 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   18482405 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   18482405 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    8917211 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    4459148 |  375 | `		}` |
|   18482405 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   18482405 |  378 | `		pEntry = pEntry->pNext;` |
|    9241205 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|     100363 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     100363 |  382 | `	pHash->apBucket = apNew;` |
|     100363 |  383 | `	pHash->nBucketSize = nNewSize;` |
|     100363 |  384 | `	return SXRET_OK;` |
|      50184 |  385 | `}` |
|   22517074 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   22517079 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   22517079 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   22517079 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   14144703 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    7072555 |  393 | `	}` |
|   22517079 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   22517079 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|     878193 |  399 | `		pHash->pLast->pNext = pEntry;` |
|     878193 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|     878193 |  401 | `		pHash->pLast = pEntry;` |
|     439099 |  402 | `	}else{` |
|   21638891 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   22517079 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|    1209470 |  407 | `		pHash->pCurrent = pHash->pList;` |
|    1209470 |  408 | `		pHash->pLast = pEntry;` |
|     604835 |  409 | `	}` |
|   22517079 |  410 | `	pHash->nEntry++;` |
|   22517079 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   22517074 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   22517079 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     100363 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|     100363 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      50179 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   22517079 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   22517079 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   22517079 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   22517079 |  435 | `	pEntry->pHash = pHash;` |
|   22517079 |  436 | `	pEntry->pKey = pKey;` |
|   22517079 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   22517079 |  438 | `	pEntry->pUserData = pUserData;` |
|   22517079 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   22517079 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   22517079 |  442 | `	return rc;` |
|   11259157 |  443 | `}` |
|   21378174 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   21378179 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|    1138900 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  455 | `{` |
|    1138905 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          5 |  457 | `}` |
|     547770 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     547775 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
