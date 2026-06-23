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
|  18307470 |   14 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   15 |  |
|  18307475 |   16 | `	pSet->nSize = 0 ;` |
|  18307475 |   17 | `	pSet->nUsed = 0;` |
|  18307475 |   18 | `	pSet->nCursor = 0;` |
|  18307475 |   19 | `	pSet->eSize = ElemSize;` |
|  18307475 |   20 | `	pSet->pAllocator = pAllocator;` |
|  18307475 |   21 | `	pSet->pBase =  0;` |
|  18307475 |   22 | `	pSet->pUserData = 0;` |
|  18307475 |   23 | `	return SXRET_OK;` |
|         5 |   24 |  |
|  30049957 |   25 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   26 |  |
|         - |   27 | `	unsigned char *zbase;` |
|  30049962 |   28 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   29 | `		void *pNew;` |
|   4434157 |   30 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   31 | `			return  SXERR_LOCKED;` |
|         - |   32 | `		}` |
|   4434157 |   33 | `		if( pSet->nSize <= 0 ){` |
|   4286473 |   34 | `			pSet->nSize = 4;` |
|   2143234 |   35 | `		}` |
|   4434157 |   36 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nSize * 2);` |
|   4434157 |   37 | `		if( pNew == 0 ){` |
|       ! 0 |   38 | `			return SXERR_MEM;` |
|         - |   39 | `		}` |
|   4434157 |   40 | `		pSet->pBase = pNew;` |
|   4434157 |   41 | `		pSet->nSize <<= 1;` |
|   2217076 |   42 | `	}` |
|  30049962 |   43 | `	zbase = (unsigned char *)pSet->pBase;` |
| 224885590 |   44 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  30049962 |   45 | `	pSet->nUsed++;` |
|  30049962 |   46 | `	return SXRET_OK;` |
|  15025026 |   47 |  |
|   1219006 |   48 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   49 |  |
|   1219011 |   50 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   51 | `		return SXERR_LOCKED;` |
|         - |   52 | `	}` |
|   1219011 |   53 | `	if( nItem < 8 ){` |
|       ! 0 |   54 | `		nItem = 8;` |
|       ! 0 |   55 | `	}` |
|   1219011 |   56 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|   1219011 |   57 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   58 | `		return SXERR_MEM;` |
|         - |   59 | `	}` |
|   1219011 |   60 | `	pSet->nSize = nItem;` |
|   1219011 |   61 | `	return SXRET_OK;` |
|    609508 |   62 |  |
|   1709089 |   63 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   64 |  |
|   1709094 |   65 | `	pSet->nUsed   = 0;` |
|   1709094 |   66 | `	pSet->nCursor = 0;` |
|   1709094 |   67 | `	return SXRET_OK;` |
|         5 |   68 |  |
|     55552 |   69 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   70 |  |
|     55557 |   71 | `	pSet->nCursor = 0;` |
|     55557 |   72 | `	return SXRET_OK;` |
|         5 |   73 |  |
|     59736 |   74 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   75 |  |
|         - |   76 | `	register unsigned char *zSrc;` |
|     59741 |   77 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   78 | `		/* Reset cursor */` |
|     22933 |   79 | `		pSet->nCursor = 0;` |
|     22933 |   80 | `		return SXERR_EOF;` |
|         - |   81 | `	}` |
|     36813 |   82 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     36813 |   83 | `	if( ppEntry ){` |
|     36813 |   84 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     18404 |   85 | `	}` |
|     36813 |   86 | `	pSet->nCursor++;` |
|     36813 |   87 | `	return SXRET_OK;` |
|     29873 |   88 |  |
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
|    206366 |  100 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  101 |  |
|    206371 |  102 | `	if( nNewSize < pSet->nUsed ){` |
|       118 |  103 | `		pSet->nUsed = nNewSize;` |
|        57 |  104 | `	}` |
|    206371 |  105 | `	return SXRET_OK;` |
|         5 |  106 |  |
|   9674658 |  107 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  108 |  |
|   9674663 |  109 | `	sxi32 rc = SXRET_OK;` |
|   9674663 |  110 | `	if( pSet->pAllocator && pSet->pBase ){` |
|   4841917 |  111 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   2420956 |  112 | `	}` |
|   9674663 |  113 | `	pSet->pBase = 0;` |
|   9674663 |  114 | `	pSet->nUsed = 0;` |
|   9674663 |  115 | `	pSet->nCursor = 0;` |
|   9674663 |  116 | `	return rc;` |
|         5 |  117 |  |
|   5514882 |  118 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  119 |  |
|         - |  120 | `	const char *zBase;` |
|   5514887 |  121 | `	if( pSet->nUsed <= 0 ){` |
|       121 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|   5514771 |  124 | `	zBase = (const char *)pSet->pBase;` |
|   5514771 |  125 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   2757446 |  126 |  |
|   3520694 |  127 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  128 |  |
|         - |  129 | `	const char *zBase;` |
|         - |  130 | `	void *pData;` |
|   3520699 |  131 | `	if( pSet->nUsed <= 0 ){` |
|   2175029 |  132 | `		return 0;` |
|         - |  133 | `	}` |
|   1345675 |  134 | `	zBase = (const char *)pSet->pBase;` |
|   1345675 |  135 | `	pSet->nUsed--;` |
|   1345675 |  136 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   1345675 |  137 | `	return pData;` |
|   1760352 |  138 |  |
|  12962424 |  139 | `PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|         5 |  140 |  |
|         - |  141 | `	const char *zBase;` |
|  12962429 |  142 | `	if( nIdx >= pSet->nUsed ){` |
|         - |  143 | `		/* Out of range */` |
|       ! 0 |  144 | `		return 0;` |
|         - |  145 | `	}` |
|  12962429 |  146 | `	zBase = (const char *)pSet->pBase;` |
|  12962429 |  147 | `	return (void *)&zBase[nIdx * pSet->eSize];` |
|   6481491 |  148 |  |
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
|    529856 |  162 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  163 |  |
|         - |  164 | `	SyHashEntry_Pr **apNew;` |
|         - |  165 | `#if defined(UNTRUST)` |
|         - |  166 | `	if( pHash == 0 ){` |
|         - |  167 | `		return SXERR_EMPTY;` |
|         - |  168 | `	}` |
|         - |  169 | `#endif` |
|         - |  170 | `	/* Allocate a new table */` |
|    529861 |  171 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    529861 |  172 | `	if( apNew == 0 ){` |
|       ! 0 |  173 | `		return SXERR_MEM;` |
|         - |  174 | `	}` |
|    529861 |  175 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|    529861 |  176 | `	pHash->pAllocator = &(*pAllocator);` |
|    529861 |  177 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|    529861 |  178 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|    529861 |  179 | `	pHash->pCurrent = pHash->pList = 0;` |
|    529861 |  180 | `	pHash->nEntry = 0;` |
|    529861 |  181 | `	pHash->apBucket = apNew;` |
|    529861 |  182 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|    529861 |  183 | `	return SXRET_OK;` |
|    264933 |  184 |  |
|     97254 |  185 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  186 |  |
|         - |  187 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  188 | `#if defined(UNTRUST)` |
|         - |  189 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  190 | `		return SXERR_EMPTY;` |
|         - |  191 | `	}` |
|         - |  192 | `#endif` |
|     97259 |  193 | `	pEntry = pHash->pList;` |
|     51941 |  194 | `	for(;;){` |
|    103887 |  195 | `		if( pHash->nEntry == 0 ){` |
|     97259 |  196 | `			break;` |
|         - |  197 | `		}` |
|      6633 |  198 | `		pNext = pEntry->pNext;` |
|      6633 |  199 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|      6633 |  200 | `		pEntry = pNext;` |
|      6633 |  201 | `		pHash->nEntry--;` |
|         5 |  202 | `	}` |
|     97259 |  203 | `	if( pHash->apBucket ){` |
|     97259 |  204 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     48627 |  205 | `	}` |
|     97259 |  206 | `	pHash->apBucket = 0;` |
|     97259 |  207 | `	pHash->nBucketSize = 0;` |
|     97259 |  208 | `	pHash->pAllocator = 0;` |
|     97259 |  209 | `	return SXRET_OK;` |
|         5 |  210 |  |
|  16550786 |  211 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  212 |  |
|         - |  213 | `	SyHashEntry_Pr *pEntry;` |
|         - |  214 | `	sxu32 nHash;` |
|         - |  215 |  |
|  16550791 |  216 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|  16550791 |  217 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
|  14695450 |  218 | `	for(;;){` |
|  29434024 |  219 | `		if( pEntry == 0 ){` |
|   8721697 |  220 | `			break;` |
|         - |  221 | `		}` |
|  24626626 |  222 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|   7829098 |  223 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|   7829099 |  224 | `				return pEntry;` |
|         - |  225 | `		}` |
|  12883238 |  226 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  227 | `	}` |
|         - |  228 | `	/* Entry not found */` |
|   8721697 |  229 | `	return 0;` |
|   8275908 |  230 |  |
|  17331358 |  231 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  232 |  |
|         - |  233 | `	SyHashEntry_Pr *pEntry;` |
|         - |  234 | `#if defined(UNTRUST)` |
|         - |  235 | `	if( INVALID_HASH(pHash) ){` |
|         - |  236 | `		return 0;` |
|         - |  237 | `	}` |
|         - |  238 | `#endif` |
|  17331363 |  239 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  240 | `		/* Don't bother hashing,return immediately */` |
|    780771 |  241 | `		return 0;` |
|         - |  242 | `	}` |
|  16550597 |  243 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|  16550597 |  244 | `	if( pEntry == 0 ){` |
|   8721697 |  245 | `		return 0;` |
|         - |  246 | `	}` |
|   7828905 |  247 | `	return (SyHashEntry *)pEntry;` |
|   8666194 |  248 |  |
|    117192 |  249 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  250 |  |
|         - |  251 | `	sxi32 rc;` |
|    117197 |  252 | `	if( pEntry->pPrevCollide == 0 ){` |
|     90021 |  253 | `		pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)] = pEntry->pNextCollide;` |
|     45013 |  254 | `	}else{` |
|     27181 |  255 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|         - |  256 | `	}` |
|    117197 |  257 | `	if( pEntry->pNextCollide ){` |
|      4971 |  258 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      2485 |  259 | `	}` |
|    117197 |  260 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|    117197 |  261 | `	pHash->nEntry--;` |
|    117197 |  262 | `	if( ppUserData ){` |
|         - |  263 | `		/* Write a pointer to the user data */` |
|       ! 0 |  264 | `		*ppUserData = pEntry->pUserData;` |
|       ! 0 |  265 | `	}` |
|         - |  266 | `	/* Release the entry */` |
|    117197 |  267 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|    117197 |  268 | `	return rc;` |
|         5 |  269 |  |
|       194 |  270 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  271 |  |
|         - |  272 | `	SyHashEntry_Pr *pEntry;` |
|         - |  273 | `	sxi32 rc;` |
|         - |  274 | `#if defined(UNTRUST)` |
|         - |  275 | `	if( INVALID_HASH(pHash) ){` |
|         - |  276 | `		return SXERR_CORRUPT;` |
|         - |  277 | `	}` |
|         - |  278 | `#endif` |
|       199 |  279 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|       199 |  280 | `	if( pEntry == 0 ){` |
|       ! 0 |  281 | `		return SXERR_NOTFOUND;` |
|         - |  282 | `	}` |
|       199 |  283 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|       199 |  284 | `	return rc;` |
|       102 |  285 |  |
|    116998 |  286 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  287 |  |
|    117003 |  288 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  289 | `	sxi32 rc;` |
|         - |  290 | `#if defined(UNTRUST)` |
|         - |  291 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  292 | `		return SXERR_CORRUPT;` |
|         - |  293 | `	}` |
|         - |  294 | `#endif` |
|    117003 |  295 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|    117003 |  296 | `	return rc;` |
|         5 |  297 |  |
|   1076754 |  298 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  299 |  |
|         - |  300 | `#if defined(UNTRUST)` |
|         - |  301 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  302 | `		return SXERR_CORRUPT;` |
|         - |  303 | `	}` |
|         - |  304 | `#endif` |
|   1076759 |  305 | `	pHash->pCurrent = pHash->pList;` |
|   1076759 |  306 | `	return SXRET_OK;` |
|         5 |  307 |  |
|   6741452 |  308 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  309 |  |
|         - |  310 | `	SyHashEntry_Pr *pEntry;` |
|         - |  311 | `#if defined(UNTRUST)` |
|         - |  312 | `	if( INVALID_HASH(pHash) ){` |
|         - |  313 | `		return 0;` |
|         - |  314 | `	}` |
|         - |  315 | `#endif` |
|   6741457 |  316 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|   1076309 |  317 | `		pHash->pCurrent = pHash->pList;` |
|   1076309 |  318 | `		return 0;` |
|         - |  319 | `	}` |
|   5665153 |  320 | `	pEntry = pHash->pCurrent;` |
|         - |  321 | `	/* Advance the cursor */` |
|   5665153 |  322 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  323 | `	/* Return the current entry */` |
|   5665153 |  324 | `	return (SyHashEntry *)pEntry;` |
|   3370731 |  325 |  |
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
|      1919 |  337 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  338 | `		/* Invoke the callback */` |
|      1909 |  339 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|      1909 |  340 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `			return rc;` |
|         - |  342 | `		}` |
|         - |  343 | `		/* Point to the next entry */` |
|      1909 |  344 | `		pEntry = pEntry->pNext;` |
|       955 |  345 | `	}` |
|        11 |  346 | `	return SXRET_OK;` |
|         6 |  347 |  |
|     27432 |  348 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  349 |  |
|     27437 |  350 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  351 | `	SyHashEntry_Pr *pEntry;` |
|         - |  352 | `	SyHashEntry_Pr **apNew;` |
|         - |  353 | `	sxu32 n,iBucket;` |
|         - |  354 |  |
|         - |  355 | `	/* Allocate a new larger table */` |
|     27437 |  356 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|     27437 |  357 | `	if( apNew == 0 ){` |
|         - |  358 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  359 | `		return SXRET_OK;` |
|         - |  360 | `	}` |
|         - |  361 | `	/* Zero the new table */` |
|     27437 |  362 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  363 | `	/* Rehash all entries */` |
|   3484109 |  364 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|   3456677 |  365 | `		pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  366 | `		/* Install in the new bucket */` |
|   3456677 |  367 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|   3456677 |  368 | `		pEntry->pNextCollide = apNew[iBucket];` |
|   3456677 |  369 | `		if( apNew[iBucket] != 0 ){` |
|   1649514 |  370 | `			apNew[iBucket]->pPrevCollide = pEntry;` |
|    824862 |  371 | `		}` |
|   3456677 |  372 | `		apNew[iBucket] = pEntry;` |
|         - |  373 | `		/* Point to the next entry */` |
|   3456677 |  374 | `		pEntry = pEntry->pNext;` |
|   1728341 |  375 | `	}` |
|         - |  376 | `	/* Release the old table and reflect the change */` |
|     27437 |  377 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|     27437 |  378 | `	pHash->apBucket = apNew;` |
|     27437 |  379 | `	pHash->nBucketSize = nNewSize;` |
|     27437 |  380 | `	return SXRET_OK;` |
|     13721 |  381 |  |
|   4502964 |  382 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry)` |
|         5 |  383 |  |
|   4502969 |  384 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  385 | `	/* Insert the entry in its corresponding bucket */` |
|   4502969 |  386 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|   4502969 |  387 | `	if( pHash->apBucket[iBucket] != 0 ){` |
|   2513442 |  388 | `		pHash->apBucket[iBucket]->pPrevCollide = pEntry;` |
|   1256711 |  389 | `	}` |
|   4502969 |  390 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  391 | `	/* Link to the entry list */` |
|   4502969 |  392 | `	MACRO_LD_PUSH(pHash->pList,pEntry);` |
|   4502969 |  393 | `	if( pHash->nEntry == 0 ){` |
|    289145 |  394 | `		pHash->pCurrent = pHash->pList;` |
|    144570 |  395 | `	}` |
|   4502969 |  396 | `	pHash->nEntry++;` |
|   4502969 |  397 | `	return SXRET_OK;` |
|         5 |  398 |  |
|   4502964 |  399 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  400 |  |
|         - |  401 | `	SyHashEntry_Pr *pEntry;` |
|         - |  402 | `	sxi32 rc;` |
|         - |  403 | `#if defined(UNTRUST)` |
|         - |  404 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  405 | `		return SXERR_CORRUPT;` |
|         - |  406 | `	}` |
|         - |  407 | `#endif` |
|   4502969 |  408 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|     27437 |  409 | `		rc = HashGrowTable(&(*pHash));` |
|     27437 |  410 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  411 | `			return rc;` |
|         - |  412 | `		}` |
|     13716 |  413 | `	}` |
|         - |  414 | `	/* Allocate a new hash entry */` |
|   4502969 |  415 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|   4502969 |  416 | `	if( pEntry == 0 ){` |
|       ! 0 |  417 | `		return SXERR_MEM;` |
|         - |  418 | `	}` |
|         - |  419 | `	/* Zero the entry */` |
|   4502969 |  420 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|   4502969 |  421 | `	pEntry->pHash = pHash;` |
|   4502969 |  422 | `	pEntry->pKey = pKey;` |
|   4502969 |  423 | `	pEntry->nKeyLen = nKeyLen;` |
|   4502969 |  424 | `	pEntry->pUserData = pUserData;` |
|   4502969 |  425 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  426 | `	/* Finally insert the entry in its corresponding bucket */` |
|   4502969 |  427 | `	rc = HashInsert(&(*pHash),pEntry);` |
|   4502969 |  428 | `	return rc;` |
|   2251487 |  429 |  |
|    150942 |  430 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  431 |  |
|         - |  432 | `#if defined(UNTRUST)` |
|         - |  433 | `	if( INVALID_HASH(pHash) ){` |
|         - |  434 | `		return 0;` |
|         - |  435 | `	}` |
|         - |  436 | `#endif` |
|         - |  437 | `	/* Last inserted entry */` |
|    150947 |  438 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  439 |  |
|         - |  440 |  |
