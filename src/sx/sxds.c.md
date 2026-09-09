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
|  157051208 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|          5 |   15 | `{` |
|  157051213 |   16 | `	pSet->nSize = 0 ;` |
|  157051213 |   17 | `	pSet->nUsed = 0;` |
|  157051213 |   18 | `	pSet->nCursor = 0;` |
|  157051213 |   19 | `	pSet->eSize = ElemSize;` |
|  157051213 |   20 | `	pSet->pAllocator = pAllocator;` |
|  157051213 |   21 | `	pSet->pBase =  0;` |
|  157051213 |   22 | `	pSet->pUserData = 0;` |
|  157051213 |   23 | `	return SXRET_OK;` |
|          5 |   24 | `}` |
|  357332827 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|          5 |   26 | `{` |
|          - |   27 | `	unsigned char *zbase;` |
|  357332832 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|          - |   29 | `		void *pNew;` |
|   20681611 |   30 | `		if( pSet->pAllocator == 0 ){` |
|        ! 0 |   31 | `			return  SXERR_LOCKED;` |
|          - |   32 | `		}` |
|   20681611 |   33 | `		if( pSet->nSize <= 0 ){` |
|   17625307 |   34 | `			pSet->nSize = 4;` |
|    8812651 |   35 | `		}` |
|   20681611 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   20681611 |   37 | `		if( pNew == 0 ){` |
|        ! 0 |   38 | `			return SXERR_MEM;` |
|          - |   39 | `		}` |
|   20681611 |   40 | `		pSet->pBase = pNew;` |
|   20681611 |   41 | `		pSet->nSize <<= 1;` |
|   10340803 |   42 | `	}` |
|  357332832 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 2819146848 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  357332832 |   45 | `	pSet->nUsed++;` |
|  357332832 |   46 | `	return SXRET_OK;` |
|  178666442 |   47 | `}` |
|   17555766 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|          5 |   49 | `{` |
|   17555771 |   50 | `	if( pSet->nSize > 0 ){` |
|        ! 0 |   51 | `		return SXERR_LOCKED;` |
|          - |   52 | `	}` |
|   17555771 |   53 | `	if( nItem < 8 ){` |
|        ! 0 |   54 | `		nItem = 8;` |
|        ! 0 |   55 | `	}` |
|   17555771 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   17555771 |   57 | `	if( pSet->pBase == 0 ){` |
|        ! 0 |   58 | `		return SXERR_MEM;` |
|          - |   59 | `	}` |
|   17555771 |   60 | `	pSet->nSize = nItem;` |
|   17555771 |   61 | `	return SXRET_OK;` |
|    8777888 |   62 | `}` |
|   26103475 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|          5 |   64 | `{` |
|   26103480 |   65 | `	pSet->nUsed   = 0;` |
|   26103480 |   66 | `	pSet->nCursor = 0;` |
|   26103480 |   67 | `	return SXRET_OK;` |
|          5 |   68 | `}` |
|      69624 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|          5 |   70 | `{` |
|      69629 |   71 | `	pSet->nCursor = 0;` |
|      69629 |   72 | `	return SXRET_OK;` |
|          5 |   73 | `}` |
|      73776 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|          5 |   75 | `{` |
|          - |   76 | `	register unsigned char *zSrc;` |
|      73781 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|          - |   78 | `		/* Reset cursor */` |
|      30129 |   79 | `		pSet->nCursor = 0;` |
|      30129 |   80 | `		return SXERR_EOF;` |
|          - |   81 | `	}` |
|      43657 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|      43657 |   83 | `	if( ppEntry ){` |
|      43657 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|      21826 |   85 | `	}` |
|      43657 |   86 | `	pSet->nCursor++;` |
|      43657 |   87 | `	return SXRET_OK;` |
|      36893 |   88 | `}` |
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
|    2707576 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|          5 |  101 | `{` |
|    2707581 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       1181 |  103 | `		pSet->nUsed = nNewSize;` |
|        588 |  104 | `	}` |
|    2707581 |  105 | `	return SXRET_OK;` |
|          5 |  106 | `}` |
|   54158486 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|          5 |  108 | `{` |
|   54158491 |  109 | `	sxi32 rc = SXRET_OK;` |
|   54158491 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   29346799 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   14673397 |  112 | `	}` |
|   54158491 |  113 | `	pSet->pBase = 0;` |
|   54158491 |  114 | `	pSet->nUsed = 0;` |
|   54158491 |  115 | `	pSet->nCursor = 0;` |
|   54158491 |  116 | `	return rc;` |
|          5 |  117 | `}` |
|   63691446 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|          5 |  119 | `{` |
|          - |  120 | `	const char *zBase;` |
|   63691451 |  121 | `	if( pSet->nUsed <= 0 ){` |
|      15365 |  122 | `		return 0;` |
|          - |  123 | `	}` |
|   63676091 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   63676091 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   31845728 |  126 | `}` |
|    8867312 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|          5 |  128 | `{` |
|          - |  129 | `	const char *zBase;` |
|          - |  130 | `	void *pData;` |
|    8867317 |  131 | `	if( pSet->nUsed <= 0 ){` |
|    2213667 |  132 | `		return 0;` |
|          - |  133 | `	}` |
|    6653655 |  134 | `	zBase = (const char *)pSet->pBase;` |
|    6653655 |  135 | `	pSet->nUsed--;` |
|    6653655 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|    6653655 |  137 | `	return pData;` |
|    4433661 |  138 | `}` |
|   32266828 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|          5 |  140 | `{` |
|          - |  141 | `	const char *zBase;` |
|   32266833 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|          - |  143 | `		/* Out of range */` |
|         24 |  144 | `		return 0;` |
|          - |  145 | `	}` |
|   32266811 |  146 | `	zBase = (const char *)pSet->pBase;` |
|   32266811 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   16133549 |  148 | `}` |
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
|    1763710 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|          5 |  163 | `{` |
|          - |  164 | `	SyHashEntry_Pr **apNew;` |
|          - |  165 | `#if defined(UNTRUST)` |
|          - |  166 | `	if( pHash == 0 ){` |
|          - |  167 | `		return SXERR_EMPTY;` |
|          - |  168 | `	}` |
|          - |  169 | `#endif` |
|          - |  170 | `	/* Allocate a new table */` |
|    1763715 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1763715 |  172 | `	if( apNew == 0 ){` |
|        ! 0 |  173 | `		return SXERR_MEM;` |
|          - |  174 | `	}` |
|    1763715 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    1763715 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    1763715 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    1763715 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    1763715 |  179 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|    1763715 |  180 | `	pHash->nEntry = 0;` |
|    1763715 |  181 | `	pHash->apBucket = apNew;` |
|    1763715 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    1763715 |  183 | `	return SXRET_OK;` |
|     881860 |  184 | `}` |
|     409274 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|          5 |  186 | `{` |
|          - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|          - |  188 | `#if defined(UNTRUST)` |
|          - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  190 | `		return SXERR_EMPTY;` |
|          - |  191 | `	}` |
|          - |  192 | `#endif` |
|     409279 |  193 | `	pEntry = pHash->pList;` |
|     218296 |  194 | `	for(;;){` |
|     436597 |  195 | `		if( pHash->nEntry == 0 ){` |
|     409279 |  196 | `			break;` |
|          - |  197 | `		}` |
|      27323 |  198 | `		pNext = pEntry->pNext;` |
|      27323 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      27323 |  200 | `		pEntry = pNext;` |
|      27323 |  201 | `		pHash->nEntry--;` |
|          5 |  202 | `	}` |
|     409279 |  203 | `	if( pHash->apBucket ){` |
|     409279 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     204637 |  205 | `	}` |
|     409279 |  206 | `	pHash->apBucket = 0;` |
|     409279 |  207 | `	pHash->nBucketSize = 0;` |
|     409279 |  208 | `	pHash->pAllocator = 0;` |
|     409279 |  209 | `	return SXRET_OK;` |
|          5 |  210 | `}` |
|   65774415 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  212 | `{` |
|          - |  213 | `	SyHashEntry_Pr *pEntry;` |
|          - |  214 | `	sxu32 nHash;` |
|          - |  215 |  |
|   65774420 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|   65774420 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|   59015037 |  218 | `	for(;;){` |
|  118083031 |  219 | `		if( pEntry == 0 ){` |
|   23871274 |  220 | `			break;` |
|          - |  221 | `		}` |
|  115163269 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   41903296 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   41903151 |  224 | `				return pEntry;` |
|          - |  225 | `		}` |
|   52308616 |  226 | `		pEntry = pEntry->pNextCollide;` |
|          5 |  227 | `	}` |
|          - |  228 | `	/* Entry not found */` |
|   23871274 |  229 | `	return 0;` |
|   32887496 |  230 | `}` |
|   72608065 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|          5 |  232 | `{` |
|          - |  233 | `	SyHashEntry_Pr *pEntry;` |
|          - |  234 | `#if defined(UNTRUST)` |
|          - |  235 | `	if( INVALID_HASH(pHash) ){` |
|          - |  236 | `		return 0;` |
|          - |  237 | `	}` |
|          - |  238 | `#endif` |
|   72608070 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|          - |  240 | `		/* Don't bother hashing,return immediately */` |
|    6834007 |  241 | `		return 0;` |
|          - |  242 | `	}` |
|   65774068 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   65774068 |  244 | `	if( pEntry == 0 ){` |
|   23871256 |  245 | `		return 0;` |
|          - |  246 | `	}` |
|   41902817 |  247 | `	return (SyHashEntry *)pEntry;` |
|   36304321 |  248 | `}` |
|     426186 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|          5 |  250 | `{` |
|          - |  251 | `	sxi32 rc;` |
|     426191 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     350514 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     175259 |  254 | `	}else{` |
|      75682 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|          - |  256 | `	}` |
|     426191 |  257 | `	if( pEntry->pNextCollide ){` |
|       4196 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       2097 |  259 | `	}` |
|          - |  260 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|     426191 |  261 | `	if( pHash->pLast == pEntry ){` |
|     418481 |  262 | `		pHash->pLast = pEntry->pPrev;` |
|     209238 |  263 | `	}` |
|     426191 |  264 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|     426191 |  265 | `	pHash->nEntry--;` |
|     426191 |  266 | `	if( ppUserData ){` |
|          - |  267 | `		/* Write a pointer to the user data */` |
|         13 |  268 | `		*ppUserData = pEntry->pUserData;` |
|          6 |  269 | `	}` |
|          - |  270 | `	/* Release the entry */` |
|     426191 |  271 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|     426191 |  272 | `	return rc;` |
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
|     425852 |  290 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|          5 |  291 | `{` |
|     425857 |  292 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|          - |  293 | `	sxi32 rc;` |
|          - |  294 | `#if defined(UNTRUST)` |
|          - |  295 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|          - |  296 | `		return SXERR_CORRUPT;` |
|          - |  297 | `	}` |
|          - |  298 | `#endif` |
|     425857 |  299 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     425857 |  300 | `	return rc;` |
|          5 |  301 | `}` |
|    2823620 |  302 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|          5 |  303 | `{` |
|          - |  304 | `#if defined(UNTRUST)` |
|          - |  305 | `	if( INVALID_HASH(pHash)  ){` |
|          - |  306 | `		return SXERR_CORRUPT;` |
|          - |  307 | `	}` |
|          - |  308 | `#endif` |
|    2823625 |  309 | `	pHash->pCurrent = pHash->pList;` |
|    2823625 |  310 | `	return SXRET_OK;` |
|          5 |  311 | `}` |
|   21011994 |  312 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|          5 |  313 | `{` |
|          - |  314 | `	SyHashEntry_Pr *pEntry;` |
|          - |  315 | `#if defined(UNTRUST)` |
|          - |  316 | `	if( INVALID_HASH(pHash) ){` |
|          - |  317 | `		return 0;` |
|          - |  318 | `	}` |
|          - |  319 | `#endif` |
|   21011999 |  320 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|    2823359 |  321 | `		pHash->pCurrent = pHash->pList;` |
|    2823359 |  322 | `		return 0;` |
|          - |  323 | `	}` |
|   18188645 |  324 | `	pEntry = pHash->pCurrent;` |
|          - |  325 | `	/* Advance the cursor */` |
|   18188645 |  326 | `	pHash->pCurrent = pEntry->pNext;` |
|          - |  327 | `	/* Return the current entry */` |
|   18188645 |  328 | `	return (SyHashEntry *)pEntry;` |
|   10506002 |  329 | `}` |
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
|       4017 |  341 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|          - |  342 | `		/* Invoke the callback */` |
|       4007 |  343 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|       4007 |  344 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  345 | `			return rc;` |
|          - |  346 | `		}` |
|          - |  347 | `		/* Point to the next entry */` |
|       4007 |  348 | `		pEntry = pEntry->pNext;` |
|       2004 |  349 | `	}` |
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
|    6891573 |  374 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    3445801 |  375 | `		}` |
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
|   18114338 |  386 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|          5 |  387 | `{` |
|   18114343 |  388 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|          - |  389 | `	/* Insert the entry in its corresponding bucket */` |
|   18114343 |  390 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   18114343 |  391 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   11394697 |  392 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|    5697506 |  393 | `	}` |
|   18114343 |  394 | `	pHash->apBucket[iBucket] = pEntry;` |
|          - |  395 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|          - |  396 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|          - |  397 | `	 * callers that need a FIFO traversal. */` |
|   18114343 |  398 | `	if( bTail && pHash->pLast != 0 ){` |
|         53 |  399 | `		pHash->pLast->pNext = pEntry;` |
|         53 |  400 | `		pEntry->pPrev = pHash->pLast;` |
|         53 |  401 | `		pHash->pLast = pEntry;` |
|         27 |  402 | `	}else{` |
|   18114291 |  403 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|          - |  404 | `	}` |
|   18114343 |  405 | `	if( pHash->nEntry == 0 ){` |
|          - |  406 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|     972089 |  407 | `		pHash->pCurrent = pHash->pList;` |
|     972089 |  408 | `		pHash->pLast = pEntry;` |
|     486042 |  409 | `	}` |
|   18114343 |  410 | `	pHash->nEntry++;` |
|   18114343 |  411 | `	return SXRET_OK;` |
|          5 |  412 | `}` |
|   18114338 |  413 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|          5 |  414 | `{` |
|          - |  415 | `	SyHashEntry_Pr *pEntry;` |
|          - |  416 | `	sxi32 rc;` |
|          - |  417 | `#if defined(UNTRUST)` |
|          - |  418 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|          - |  419 | `		return SXERR_CORRUPT;` |
|          - |  420 | `	}` |
|          - |  421 | `#endif` |
|   18114343 |  422 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|      91679 |  423 | `		rc = HashGrowTable(&(*pHash));` |
|      91679 |  424 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  425 | `			return rc;` |
|          - |  426 | `		}` |
|      45837 |  427 | `	}` |
|          - |  428 | `	/* Allocate a new hash entry */` |
|   18114343 |  429 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   18114343 |  430 | `	if( pEntry == 0 ){` |
|        ! 0 |  431 | `		return SXERR_MEM;` |
|          - |  432 | `	}` |
|          - |  433 | `	/* Zero the entry */` |
|   18114343 |  434 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   18114343 |  435 | `	pEntry->pHash = pHash;` |
|   18114343 |  436 | `	pEntry->pKey = pKey;` |
|   18114343 |  437 | `	pEntry->nKeyLen = nKeyLen;` |
|   18114343 |  438 | `	pEntry->pUserData = pUserData;` |
|   18114343 |  439 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|          - |  440 | `	/* Finally insert the entry in its corresponding bucket */` |
|   18114343 |  441 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|   18114343 |  442 | `	return rc;` |
|    9057174 |  443 | `}` |
|   18114206 |  444 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|          5 |  445 | `{` |
|   18114211 |  446 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
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
|     465466 |  458 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|          5 |  459 | `{` |
|          - |  460 | `#if defined(UNTRUST)` |
|          - |  461 | `	if( INVALID_HASH(pHash) ){` |
|          - |  462 | `		return 0;` |
|          - |  463 | `	}` |
|          - |  464 | `#endif` |
|          - |  465 | `	/* Last inserted entry */` |
|     465471 |  466 | `	return (SyHashEntry *)pHash->pList;` |
|          5 |  467 | `}` |
|          - |  468 |  |
