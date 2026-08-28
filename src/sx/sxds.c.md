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
|   89711576 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|   89711581 |   16 | `	pSet->nSize = 0 ;` |
|   89711581 |   17 | `	pSet->nUsed = 0;` |
|   89711581 |   18 | `	pSet->nCursor = 0;` |
|   89711581 |   19 | `	pSet->eSize = ElemSize;` |
|   89711581 |   20 | `	pSet->pAllocator = pAllocator;` |
|   89711581 |   21 | `	pSet->pBase =  0;` |
|   89711581 |   22 | `	pSet->pUserData = 0;` |
|   89711581 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  194791715 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  194791720 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   12993757 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   12993757 |   33 | `		if( pSet->nSize <= 0 ){` |
|   11365207 |   34 | `			pSet->nSize = 4;` |
|    5682601 |   35 | `		}` |
|   12993757 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   12993757 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   12993757 |   40 | `		pSet->pBase = pNew;` |
|   12993757 |   41 | `		pSet->nSize <<= 1;` |
|    6496876 |   42 | `	}` |
|  194791720 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 1432326092 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  194791720 |   45 | `	pSet->nUsed++;` |
|  194791720 |   46 | `	return SXRET_OK;` |
|   97395905 |   47 | `}` |
|    9446066 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|    9446071 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|    9446071 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|    9446071 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    9446071 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|    9446071 |   60 | `	pSet->nSize = nItem;` |
|    9446071 |   61 | `	return SXRET_OK;` |
|    4723038 |   62 | `}` |
|   14626903 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   14626908 |   65 | `	pSet->nUsed   = 0;` |
|   14626908 |   66 | `	pSet->nCursor = 0;` |
|   14626908 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69062 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69067 |   71 | `	pSet->nCursor = 0;` |
|      69067 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73288 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73293 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29729 |   79 | `		pSet->nCursor = 0;` |
|      29729 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43569 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43569 |   83 | `	if( ppEntry ){` |
|      43569 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21782 |   85 | `	}` |
|      43569 |   86 | `	pSet->nCursor++;` |
|      43569 |   87 | `	return SXRET_OK;` |
|      36649 |   88 | `}` |
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
|    1503122 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    1503127 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1179 |  103 | `		pSet->nUsed = nNewSize;` |
|        587 |  104 | `	}` |
|    1503127 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   32754782 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   32754787 |  109 | `	sxi32 rc = SXRET_OK;` |
|   32754787 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   17599617 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|    8799806 |  112 | `	}` |
|   32754787 |  113 | `	pSet->pBase = 0;` |
|   32754787 |  114 | `	pSet->nUsed = 0;` |
|   32754787 |  115 | `	pSet->nCursor = 0;` |
|   32754787 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   34153202 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   34153207 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15605 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   34137607 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   34137607 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   17076606 |  126 | `}` |
|    6353212 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    6353217 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2195385 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    4157837 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    4157837 |  135 | `	pSet->nUsed--;` |
|    4157837 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    4157837 |  137 | `	return pData;` |
|    3176611 |  138 | `}` |
|   22173699 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   22173704 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   22173682 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   22173682 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   11087208 |  148 | `}` |
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
|    1268820 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1268825 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1268825 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1268825 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1268825 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1268825 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1268825 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1268825 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1268825 |  180 | `	pHash->nEntry = 0;` |
|    1268825 |  181 | `	pHash->apBucket = apNew;` |
|    1268825 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1268825 |  183 | `	return SXRET_OK;` |
|     634415 |  184 | `}` |
|     327332 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     327337 |  193 | `	pEntry = pHash->pList;` |
|     173309 |  194 | `	for(;;){` |
|     346623 |  195 | `		if( pHash->nEntry == 0 ){` |
|     327337 |  196 | `			break;` |
|          - |  197 | `		}` |
|      19291 |  198 | `		pNext = pEntry->pNext;` |
|      19291 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      19291 |  200 | `		pEntry = pNext;` |
|      19291 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     327337 |  203 | `	if( pHash->apBucket ){` |
|     327337 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     163666 |  205 | `	}` |
|     327337 |  206 | `	pHash->apBucket = 0;` |
|     327337 |  207 | `	pHash->nBucketSize = 0;` |
|     327337 |  208 | `	pHash->pAllocator = 0;` |
|     327337 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   42858781 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   42858786 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   42858786 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   40790205 |  218 | `	for(;;){` |
|   81347964 |  219 | `		if( pEntry == 0 ){` |
|   17001268 |  220 | `			break;` |
|          - |  221 | `		}` |
|   77275226 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   25857560 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   25857523 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   38489183 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   17001268 |  229 | `	return 0;` |
|   21429907 |  230 | `}` |
|   46823561 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   46823566 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    3965107 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   42858464 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   42858464 |  244 | `	if( pEntry == 0 ){` |
|   17001268 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   25857201 |  247 | `	return (SyHashEntry *)pEntry;` |
|   23412297 |  248 | `}` |
|     216404 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     216409 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     173289 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|      86647 |  254 | `	}else{` |
|      43125 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     216409 |  257 | `	if( pEntry->pNextCollide ){` |
|       4196 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2098 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     216409 |  261 | `	if( pHash->pLast == pEntry ){` |
|     209625 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     104810 |  263 | `	}` |
|     216409 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     216409 |  265 | `	pHash->nEntry--;` |
|     216409 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|        ! 0 |  268 | `		*ppUserData = pEntry->pUserData;` |
|        ! 0 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     216409 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     216409 |  272 | `	return rc;` |
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
|     216082 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     216087 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     216087 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     216087 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    1950468 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    1950473 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    1950473 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   14741738 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   14741743 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    1950207 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    1950207 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   12791541 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   12791541 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   12791541 |  328 | `	return (SyHashEntry *)pEntry;` |
|    7370874 |  329 | `}` |
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
|       3189 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3179 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3179 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3179 |  348 | `		pEntry = pEntry->pNext;` |
|       1590 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      80942 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      80947 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      80947 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      80947 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      80947 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   10563091 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   10482149 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   10482149 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   10482149 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   10482149 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    5032419 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    2515889 |  375 | `		}` |
|   10482149 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   10482149 |  378 | `		pEntry = pEntry->pNext;` |
|    5241077 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      80947 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      80947 |  382 | `	pHash->apBucket = apNew;` |
|      80947 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      80947 |  384 | `	return SXRET_OK;` |
|      40476 |  385 | `}` |
|   12361392 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   12361397 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   12361397 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   12361397 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|    7710393 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    3855350 |  393 | `	}` |
|   12361397 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   12361397 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   12361345 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   12361397 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     662985 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     662985 |  408 | `		pHash->pLast = pEntry;` |
|     331490 |  409 | `	}` |
|   12361397 |  410 | `	pHash->nEntry++;` |
|   12361397 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   12361392 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   12361397 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      80947 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      80947 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      40471 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   12361397 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   12361397 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   12361397 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   12361397 |  435 | `	pEntry->pHash = pHash;` |
|   12361397 |  436 | `	pEntry->pKey = pKey;` |
|   12361397 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   12361397 |  438 | `	pEntry->pUserData = pUserData;` |
|   12361397 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   12361397 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   12361397 |  442 | `	return rc;` |
|    6180701 |  443 | `}` |
|   12361264 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   12361269 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     257016 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     257021 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
