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
|  11930586 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         2 |   15 |  |
|  11930588 |   16 | `	pSet->nSize = 0 ;` |
|  11930588 |   17 | `	pSet->nUsed = 0;` |
|  11930588 |   18 | `	pSet->nCursor = 0;` |
|  11930588 |   19 | `	pSet->eSize = ElemSize;` |
|  11930588 |   20 | `	pSet->pAllocator = pAllocator;` |
|  11930588 |   21 | `	pSet->pBase =  0;` |
|  11930588 |   22 | `	pSet->pUserData = 0;` |
|  11930588 |   23 | `	return SXRET_OK;` |
|         2 |   24 |  |
|  19418740 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         2 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  19418742 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   3585294 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   3585294 |   33 | `		if( pSet->nSize <= 0 ){` |
|   3495870 |   34 | `			pSet->nSize = 4;` |
|   1747934 |   35 | `		}` |
|   3585294 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   3585294 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   3585294 |   40 | `		pSet->pBase = pNew;` |
|   3585294 |   41 | `		pSet->nSize <<= 1;` |
|   1792646 |   42 | `	}` |
|  19418742 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 144439234 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  19418742 |   45 | `	pSet->nUsed++;` |
|  19418742 |   46 | `	return SXRET_OK;` |
|   9709394 |   47 |  |
|    609548 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         2 |   49 |  |
|    609550 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|    609550 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|    609550 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|    609550 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|    609550 |   60 | `	pSet->nSize = nItem;` |
|    609550 |   61 | `	return SXRET_OK;` |
|    304776 |   62 |  |
|   1092062 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         2 |   64 |  |
|   1092064 |   65 | `	pSet->nUsed   = 0;` |
|   1092064 |   66 | `	pSet->nCursor = 0;` |
|   1092064 |   67 | `	return SXRET_OK;` |
|         2 |   68 |  |
|     40032 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         2 |   70 |  |
|     40034 |   71 | `	pSet->nCursor = 0;` |
|     40034 |   72 | `	return SXRET_OK;` |
|         2 |   73 |  |
|     43912 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         2 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     43914 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     16278 |   79 | `		pSet->nCursor = 0;` |
|     16278 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     27638 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     27638 |   83 | `	if( ppEntry ){` |
|     27638 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     13818 |   85 | `	}` |
|     27638 |   86 | `	pSet->nCursor++;` |
|     27638 |   87 | `	return SXRET_OK;` |
|     21958 |   88 |  |
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
|     74522 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         2 |  101 |  |
|     74524 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|        20 |  103 | `		pSet->nUsed = nNewSize;` |
|         9 |  104 | `	}` |
|     74524 |  105 | `	return SXRET_OK;` |
|         2 |  106 |  |
|   7506602 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         2 |  108 |  |
|   7506604 |  109 | `	sxi32 rc = SXRET_OK;` |
|   7506604 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   3885326 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   1942662 |  112 | `	}` |
|   7506604 |  113 | `	pSet->pBase = 0;` |
|   7506604 |  114 | `	pSet->nUsed = 0;` |
|   7506604 |  115 | `	pSet->nCursor = 0;` |
|   7506604 |  116 | `	return rc;` |
|         2 |  117 |  |
|   3841018 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         2 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   3841020 |  121 | `	if( pSet->nUsed <= 0 ){` |
|        92 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   3840930 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   3840930 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   1920511 |  126 |  |
|   3144848 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         2 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3144850 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2134896 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1009956 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1009956 |  135 | `	pSet->nUsed--;` |
|   1009956 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1009956 |  137 | `	return pData;` |
|   1572426 |  138 |  |
|   9890820 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         2 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|   9890822 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|   9890822 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   9890822 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   4945597 |  148 |  |
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
|    111064 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         2 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    111066 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    111066 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    111066 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    111066 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    111066 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    111066 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    111066 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    111066 |  180 | `	pHash->nEntry = 0;` |
|    111066 |  181 | `	pHash->apBucket = apNew;` |
|    111066 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    111066 |  183 | `	return SXRET_OK;` |
|     55534 |  184 |  |
|     12252 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         2 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     12254 |  193 | `	pEntry = pHash->pList;` |
|      7688 |  194 | `	for(;;){` |
|     15378 |  195 | `		if( pHash->nEntry == 0 ){` |
|     12254 |  196 | `			break;` |
|         - |  197 | `		}` |
|      3126 |  198 | `		pNext = pEntry->pNext;` |
|      3126 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      3126 |  200 | `		pEntry = pNext;` |
|      3126 |  201 | `		pHash->nEntry--;` |
|         2 |  202 | `	}` |
|     12254 |  203 | `	if( pHash->apBucket ){` |
|     12254 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      6126 |  205 | `	}` |
|     12254 |  206 | `	pHash->apBucket = 0;` |
|     12254 |  207 | `	pHash->nBucketSize = 0;` |
|     12254 |  208 | `	pHash->pAllocator = 0;` |
|     12254 |  209 | `	return SXRET_OK;` |
|         2 |  210 |  |
|  10263768 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  10263770 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  10263770 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   8884246 |  218 | `	for(;;){` |
|  17621993 |  219 | `		if( pEntry == 0 ){` |
|   5580370 |  220 | `			break;` |
|         - |  221 | `		}` |
|  14383195 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   4683404 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   4683402 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|   7358225 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         2 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   5580370 |  229 | `	return 0;` |
|   5132150 |  230 |  |
|  10326676 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         2 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  10326678 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|     62916 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  10263764 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  10263764 |  244 | `	if( pEntry == 0 ){` |
|   5580370 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   4683396 |  247 | `	return (SyHashEntry *)pEntry;` |
|   5163604 |  248 |  |
|     76122 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         2 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|     76124 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     57482 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     28742 |  254 | `	}else{` |
|     18644 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|     76124 |  257 | `	if( pEntry->pNextCollide ){` |
|      4133 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2066 |  259 | `	}` |
|     76124 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     76124 |  261 | `	pHash->nEntry--;` |
|     76124 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|     76124 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     76124 |  268 | `	return rc;` |
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
|     76116 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         2 |  287 |  |
|     76118 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|     76118 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     76118 |  296 | `	return rc;` |
|         2 |  297 |  |
|    158606 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         2 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|    158608 |  305 | `	pHash->pCurrent = pHash->pList;` |
|    158608 |  306 | `	return SXRET_OK;` |
|         2 |  307 |  |
|   1134888 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         2 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   1134890 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    158174 |  317 | `		pHash->pCurrent = pHash->pList;` |
|    158174 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|    976718 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|    976718 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|    976718 |  324 | `	return (SyHashEntry *)pEntry;` |
|    567446 |  325 |  |
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
|      1617 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1607 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1607 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1607 |  344 | `		pEntry = pEntry->pNext;` |
|       804 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     15752 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         2 |  349 |  |
|     15754 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     15754 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     15754 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     15754 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   2168650 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   2152898 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   2152898 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   2152898 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   2152898 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1033751 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    516872 |  371 | `		}` |
|   2152898 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   2152898 |  374 | `		pEntry = pEntry->pNext;` |
|   1076450 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     15754 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     15754 |  378 | `	pHash->apBucket = apNew;` |
|     15754 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     15754 |  380 | `	return SXRET_OK;` |
|      7878 |  381 |  |
|   1972120 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         2 |  383 |  |
|   1972122 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   1972122 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   1972122 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   1313792 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    656882 |  389 | `	}` |
|   1972122 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   1972122 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   1972122 |  393 | `	if( pHash->nEntry == 0 ){` |
|     79456 |  394 | `		pHash->pCurrent = pHash->pList;` |
|     39727 |  395 | `	}` |
|   1972122 |  396 | `	pHash->nEntry++;` |
|   1972122 |  397 | `	return SXRET_OK;` |
|         2 |  398 |  |
|   1972120 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         2 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   1972122 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     15754 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     15754 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|      7876 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   1972122 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   1972122 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   1972122 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   1972122 |  421 | `	pEntry->pHash = pHash;` |
|   1972122 |  422 | `	pEntry->pKey = pKey;` |
|   1972122 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   1972122 |  424 | `	pEntry->pUserData = pUserData;` |
|   1972122 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   1972122 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   1972122 |  428 | `	return rc;` |
|    986062 |  429 |  |
|     96128 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         2 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|     96130 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         2 |  439 |  |
|         - |  440 |  |
