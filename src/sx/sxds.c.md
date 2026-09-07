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
|  140137466 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  140137471 |   16 | `	pSet->nSize = 0 ;` |
|  140137471 |   17 | `	pSet->nUsed = 0;` |
|  140137471 |   18 | `	pSet->nCursor = 0;` |
|  140137471 |   19 | `	pSet->eSize = ElemSize;` |
|  140137471 |   20 | `	pSet->pAllocator = pAllocator;` |
|  140137471 |   21 | `	pSet->pBase =  0;` |
|  140137471 |   22 | `	pSet->pUserData = 0;` |
|  140137471 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  313585218 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  313585223 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   18593497 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   18593497 |   33 | `		if( pSet->nSize <= 0 ){` |
|   15922199 |   34 | `			pSet->nSize = 4;` |
|    7961097 |   35 | `		}` |
|   18593497 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   18593497 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   18593497 |   40 | `		pSet->pBase = pNew;` |
|   18593497 |   41 | `		pSet->nSize <<= 1;` |
|    9296746 |   42 | `	}` |
|  313585223 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2320441205 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  313585223 |   45 | `	pSet->nUsed++;` |
|  313585223 |   46 | `	return SXRET_OK;` |
|  156792636 |   47 | `}` |
|   15382846 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   15382851 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   15382851 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   15382851 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   15382851 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   15382851 |   60 | `	pSet->nSize = nItem;` |
|   15382851 |   61 | `	return SXRET_OK;` |
|    7691428 |   62 | `}` |
|   22189842 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   22189847 |   65 | `	pSet->nUsed   = 0;` |
|   22189847 |   66 | `	pSet->nCursor = 0;` |
|   22189847 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      68964 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      68969 |   71 | `	pSet->nCursor = 0;` |
|      68969 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73064 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73069 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      29841 |   79 | `		pSet->nCursor = 0;` |
|      29841 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43233 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43233 |   83 | `	if( ppEntry ){` |
|      43233 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21614 |   85 | `	}` |
|      43233 |   86 | `	pSet->nCursor++;` |
|      43233 |   87 | `	return SXRET_OK;` |
|      36537 |   88 | `}` |
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
|    2528870 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2528875 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1181 |  103 | `		pSet->nUsed = nNewSize;` |
|        588 |  104 | `	}` |
|    2528875 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   48515796 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   48515801 |  109 | `	sxi32 rc = SXRET_OK;` |
|   48515801 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   25940045 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   12970020 |  112 | `	}` |
|   48515801 |  113 | `	pSet->pBase = 0;` |
|   48515801 |  114 | `	pSet->nUsed = 0;` |
|   48515801 |  115 | `	pSet->nCursor = 0;` |
|   48515801 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   56415624 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   56415629 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15277 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   56400357 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   56400357 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   28207817 |  126 | `}` |
|    7981884 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    7981889 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2218031 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    5763863 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    5763863 |  135 | `	pSet->nUsed--;` |
|    5763863 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    5763863 |  137 | `	return pData;` |
|    3990947 |  138 | `}` |
|   29765524 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   29765529 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   29765507 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   29765507 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   14882845 |  148 | `}` |
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
|    1750694 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1750699 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1750699 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1750699 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1750699 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1750699 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1750699 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1750699 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1750699 |  180 | `	pHash->nEntry = 0;` |
|    1750699 |  181 | `	pHash->apBucket = apNew;` |
|    1750699 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1750699 |  183 | `	return SXRET_OK;` |
|     875352 |  184 | `}` |
|     404326 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     404331 |  193 | `	pEntry = pHash->pList;` |
|     215031 |  194 | `	for(;;){` |
|     430067 |  195 | `		if( pHash->nEntry == 0 ){` |
|     404331 |  196 | `			break;` |
|          - |  197 | `		}` |
|      25741 |  198 | `		pNext = pEntry->pNext;` |
|      25741 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      25741 |  200 | `		pEntry = pNext;` |
|      25741 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     404331 |  203 | `	if( pHash->apBucket ){` |
|     404331 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     202163 |  205 | `	}` |
|     404331 |  206 | `	pHash->apBucket = 0;` |
|     404331 |  207 | `	pHash->nBucketSize = 0;` |
|     404331 |  208 | `	pHash->pAllocator = 0;` |
|     404331 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   61281111 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   61281116 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   61281116 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   55978733 |  218 | `	for(;;){` |
|  111954542 |  219 | `		if( pEntry == 0 ){` |
|   23058056 |  220 | `			break;` |
|          - |  221 | `		}` |
|  108007936 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   38223154 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   38223065 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   50673431 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   23058056 |  229 | `	return 0;` |
|   30640826 |  230 | `}` |
|   67421789 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   67421794 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    6141035 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   61280764 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   61280764 |  244 | `	if( pEntry == 0 ){` |
|   23058038 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   38222731 |  247 | `	return (SyHashEntry *)pEntry;` |
|   33711165 |  248 | `}` |
|     417462 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     417467 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     342765 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     171385 |  254 | `	}else{` |
|      74707 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     417467 |  257 | `	if( pEntry->pNextCollide ){` |
|       4390 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2194 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     417467 |  261 | `	if( pHash->pLast == pEntry ){` |
|     410419 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     205207 |  263 | `	}` |
|     417467 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     417467 |  265 | `	pHash->nEntry--;` |
|     417467 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     417467 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     417467 |  272 | `	return rc;` |
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
|     417128 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     417133 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     417133 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     417133 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2802840 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2802845 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2802845 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   20826200 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   20826205 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2802579 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2802579 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18023631 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18023631 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18023631 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10413105 |  329 | `}` |
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
|       3823 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       3813 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       3813 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       3813 |  348 | `		pEntry = pEntry->pNext;` |
|       1907 |  349 | `	}` |
|         11 |  350 | `	return SXRET_OK;` |
|          6 |  351 | `}` |
|      90666 |  352 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|          5 |  353 | `{` |
|      90671 |  354 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|          - |  355 | `	SyHashEntry_Pr *pEntry;` |
|          - |  356 | `	SyHashEntry_Pr **apNew;` |
|          - |  357 | `	sxu32 n,iBucket;` |
|          - |  358 |  |
|          - |  359 | `	/* Allocate a new larger table */` |
|      90671 |  360 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|      90671 |  361 | `	if( apNew == 0 ){` |
|          - |  362 | `		/* Not so fatal,simply a performance hit */` |
|        ! 0 |  363 | `		return SXRET_OK;` |
|          - |  364 | `	}` |
|          - |  365 | `	/* Zero the new table */` |
|      90671 |  366 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|          - |  367 | `	/* Rehash all entries */` |
|   14259503 |  368 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   14168837 |  369 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|          - |  370 | `		/* Install in the new bucket */` |
|   14168837 |  371 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   14168837 |  372 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   14168837 |  373 | `		if( apNew[iBucket] != 0 ){` |
|    6778188 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3389182 |  375 | `		}` |
|   14168837 |  376 | `		apNew[iBucket] = pEntry;` |
|          - |  377 | `		/* Point to the next entry */` |
|   14168837 |  378 | `		pEntry = pEntry->pNext;` |
|    7084421 |  379 | `	}` |
|          - |  380 | `	/* Release the old table and reflect the change */` |
|      90671 |  381 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|      90671 |  382 | `	pHash->apBucket = apNew;` |
|      90671 |  383 | `	pHash->nBucketSize = nNewSize;` |
|      90671 |  384 | `	return SXRET_OK;` |
|      45338 |  385 | `}` |
|   17361106 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   17361111 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   17361111 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   17361111 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   10784670 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5392224 |  393 | `	}` |
|   17361111 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   17361111 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   17361059 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   17361111 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     964207 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     964207 |  408 | `		pHash->pLast = pEntry;` |
|     482101 |  409 | `	}` |
|   17361111 |  410 | `	pHash->nEntry++;` |
|   17361111 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   17361106 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   17361111 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      90671 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      90671 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45333 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   17361111 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   17361111 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   17361111 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   17361111 |  435 | `	pEntry->pHash = pHash;` |
|   17361111 |  436 | `	pEntry->pKey = pKey;` |
|   17361111 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   17361111 |  438 | `	pEntry->pUserData = pUserData;` |
|   17361111 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   17361111 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   17361111 |  442 | `	return rc;` |
|    8680558 |  443 | `}` |
|   17360974 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   17360979 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     456468 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     456473 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
