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
|  160098894 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  160098899 |   16 | `	pSet->nSize = 0 ;` |
|  160098899 |   17 | `	pSet->nUsed = 0;` |
|  160098899 |   18 | `	pSet->nCursor = 0;` |
|  160098899 |   19 | `	pSet->eSize = ElemSize;` |
|  160098899 |   20 | `	pSet->pAllocator = pAllocator;` |
|  160098899 |   21 | `	pSet->pBase =  0;` |
|  160098899 |   22 | `	pSet->pUserData = 0;` |
|  160098899 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  365384137 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  365384142 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   21014733 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   21014733 |   33 | `		if( pSet->nSize <= 0 ){` |
|   17885951 |   34 | `			pSet->nSize = 4;` |
|    8942973 |   35 | `		}` |
|   21014733 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   21014733 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   21014733 |   40 | `		pSet->pBase = pNew;` |
|   21014733 |   41 | `		pSet->nSize <<= 1;` |
|   10507364 |   42 | `	}` |
|  365384142 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2884488336 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  365384142 |   45 | `	pSet->nUsed++;` |
|  365384142 |   46 | `	return SXRET_OK;` |
|  182692097 |   47 | `}` |
|   18093412 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   18093417 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   18093417 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   18093417 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   18093417 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   18093417 |   60 | `	pSet->nSize = nItem;` |
|   18093417 |   61 | `	return SXRET_OK;` |
|    9046711 |   62 | `}` |
|   26900945 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   26900950 |   65 | `	pSet->nUsed   = 0;` |
|   26900950 |   66 | `	pSet->nCursor = 0;` |
|   26900950 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69748 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69753 |   71 | `	pSet->nCursor = 0;` |
|      69753 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73908 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73913 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30181 |   79 | `		pSet->nCursor = 0;` |
|      30181 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43737 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43737 |   83 | `	if( ppEntry ){` |
|      43737 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21866 |   85 | `	}` |
|      43737 |   86 | `	pSet->nCursor++;` |
|      43737 |   87 | `	return SXRET_OK;` |
|      36959 |   88 | `}` |
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
|    2737130 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2737135 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1181 |  103 | `		pSet->nUsed = nNewSize;` |
|        588 |  104 | `	}` |
|    2737135 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   55147622 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   55147627 |  109 | `	sxi32 rc = SXRET_OK;` |
|   55147627 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   30019919 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   15009957 |  112 | `	}` |
|   55147627 |  113 | `	pSet->pBase = 0;` |
|   55147627 |  114 | `	pSet->nUsed = 0;` |
|   55147627 |  115 | `	pSet->nCursor = 0;` |
|   55147627 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   65012812 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   65012817 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15381 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   64997441 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   64997441 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   32506411 |  126 | `}` |
|    8987704 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    8987709 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2213787 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    6773927 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    6773927 |  135 | `	pSet->nUsed--;` |
|    6773927 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    6773927 |  137 | `	return pData;` |
|    4493857 |  138 | `}` |
|   32694616 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   32694621 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   32694599 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   32694599 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   16347431 |  148 | `}` |
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
|    1766068 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1766073 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1766073 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1766073 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1766073 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1766073 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1766073 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1766073 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1766073 |  180 | `	pHash->nEntry = 0;` |
|    1766073 |  181 | `	pHash->apBucket = apNew;` |
|    1766073 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1766073 |  183 | `	return SXRET_OK;` |
|     883039 |  184 | `}` |
|     410206 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     410211 |  193 | `	pEntry = pHash->pList;` |
|     218941 |  194 | `	for(;;){` |
|     437887 |  195 | `		if( pHash->nEntry == 0 ){` |
|     410211 |  196 | `			break;` |
|          - |  197 | `		}` |
|      27681 |  198 | `		pNext = pEntry->pNext;` |
|      27681 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      27681 |  200 | `		pEntry = pNext;` |
|      27681 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     410211 |  203 | `	if( pHash->apBucket ){` |
|     410211 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     205103 |  205 | `	}` |
|     410211 |  206 | `	pHash->apBucket = 0;` |
|     410211 |  207 | `	pHash->nBucketSize = 0;` |
|     410211 |  208 | `	pHash->pAllocator = 0;` |
|     410211 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   66744667 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   66744672 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   66744672 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   60615012 |  218 | `	for(;;){` |
|  121093266 |  219 | `		if( pEntry == 0 ){` |
|   24168632 |  220 | `			break;` |
|          - |  221 | `		}` |
|  118214504 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   42580012 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   42576045 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   54348599 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   24168632 |  229 | `	return 0;` |
|   33372622 |  230 | `}` |
|   73662353 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   73662358 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    6918043 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   66744320 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   66744320 |  244 | `	if( pEntry == 0 ){` |
|   24168614 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   42575711 |  247 | `	return (SyHashEntry *)pEntry;` |
|   36831465 |  248 | `}` |
|     429924 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     429929 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     353520 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     176762 |  254 | `	}else{` |
|      76414 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     429929 |  257 | `	if( pEntry->pNextCollide ){` |
|       4228 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2113 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     429929 |  261 | `	if( pHash->pLast == pEntry ){` |
|     422207 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     211101 |  263 | `	}` |
|     429929 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     429929 |  265 | `	pHash->nEntry--;` |
|     429929 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     429929 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     429929 |  272 | `	return rc;` |
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
|     429590 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     429595 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     429595 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     429595 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2826958 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2826963 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2826963 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   21075722 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   21075727 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2826697 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2826697 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18249035 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18249035 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18249035 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10537866 |  329 | `}` |
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
|       4033 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4023 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4023 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4023 |  348 | `		pEntry = pEntry->pNext;` |
|       2012 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      91776 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      91781 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      91781 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      91781 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      91781 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14407685 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14315909 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14315909 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14315909 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14315909 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    6884352 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3442190 |  375 | `		}` |
|   14315909 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14315909 |  378 | `		pEntry = pEntry->pNext;` |
|    7157957 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      91781 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      91781 |  382 | `	pHash->apBucket = apNew;` |
|      91781 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      91781 |  384 | `	return SXRET_OK;` |
|      45893 |  385 | `}` |
|   18365150 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   18365155 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   18365155 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   18365155 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11606368 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5803328 |  393 | `	}` |
|   18365155 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   18365155 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   18365103 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   18365155 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     973573 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     973573 |  408 | `		pHash->pLast = pEntry;` |
|     486784 |  409 | `	}` |
|   18365155 |  410 | `	pHash->nEntry++;` |
|   18365155 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   18365150 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   18365155 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      91781 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      91781 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45888 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   18365155 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   18365155 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   18365155 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   18365155 |  435 | `	pEntry->pHash = pHash;` |
|   18365155 |  436 | `	pEntry->pKey = pKey;` |
|   18365155 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   18365155 |  438 | `	pEntry->pUserData = pUserData;` |
|   18365155 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   18365155 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   18365155 |  442 | `	return rc;` |
|    9182580 |  443 | `}` |
|   18365016 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   18365021 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|          5 |  447 | `}` |
|          - |  448 | `/*` |
|          - |  449 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|          - |  450 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|          - |  451 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|          - |  452 | ` * object properties, where PHP preserves property-creation order.` |
|          - |  453 | ` */` |
|        134 |  454 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          2 |  455 | `{` |
|        136 |  456 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|          2 |  457 | `}` |
|     469254 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     469259 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
