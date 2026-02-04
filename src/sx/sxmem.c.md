# src/sx/sxmem.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 430/503 lines (85.49%)

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
|         - |   10 | `#include "sxmutex.h"` |
|         - |   11 | `#include "sxstr.h"` |
|         - |   12 | `#if defined(__WINNT__)` |
|         - |   13 | `#include <Windows.h>` |
|         - |   14 | `#else` |
|         - |   15 | `#include <stdlib.h>` |
|         - |   16 | `#endif` |
|         - |   17 |  |
|   7518454 |   18 | `static void * SyOSHeapAlloc(sxu32 nByte)` |
|         1 |   19 |  |
|         - |   20 | `	void *pNew;` |
|         - |   21 | `#if defined(__WINNT__)` |
|         1 |   22 | `	pNew = HeapAlloc(GetProcessHeap(),0,nByte);` |
|         - |   23 | `#else` |
|   7518454 |   24 | `	pNew = malloc((size_t)nByte);` |
|         - |   25 | `#endif` |
|   7518455 |   26 | `	return pNew;` |
|         1 |   27 |  |
|    270344 |   28 | `static void * SyOSHeapRealloc(void *pOld,sxu32 nByte)` |
|         1 |   29 |  |
|         - |   30 | `	void *pNew;` |
|         - |   31 | `#if defined(__WINNT__)` |
|         1 |   32 | `	pNew = HeapReAlloc(GetProcessHeap(),0,pOld,nByte);` |
|         - |   33 | `#else` |
|    270344 |   34 | `	pNew = realloc(pOld,(size_t)nByte);` |
|         - |   35 | `#endif` |
|    270345 |   36 | `	return pNew;` |
|         1 |   37 |  |
|   7505406 |   38 | `static void SyOSHeapFree(void *pPtr)` |
|         1 |   39 |  |
|         - |   40 | `#if defined(__WINNT__)` |
|         1 |   41 | `	HeapFree(GetProcessHeap(),0,pPtr);` |
|         - |   42 | `#else` |
|   7505406 |   43 | `	free(pPtr);` |
|         - |   44 | `#endif` |
|   7505407 |   45 |  |
|         - |   46 |  |
|         - |   47 |  |
|  14634020 |   48 | `PH7_PRIVATE void SyZero(void *pSrc,sxu32 nSize)` |
|         1 |   49 |  |
|  14634021 |   50 | `	register unsigned char *zSrc = (unsigned char *)pSrc;` |
|         - |   51 | `	unsigned char *zEnd;` |
|         - |   52 | `#if defined(UNTRUST)` |
|         - |   53 | `	if( zSrc == 0 \|\| nSize <= 0 ){` |
|         - |   54 | `		return ;` |
|         - |   55 | `	}` |
|         - |   56 | `#endif` |
|  14634021 |   57 | `	zEnd = &zSrc[nSize];` |
| 183693640 |   58 | `	for(;;){` |
| 367387319 |   59 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 352753303 |   60 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 352753301 |   61 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 352753301 |   62 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
|         1 |   63 | `	}` |
|  14634021 |   64 |  |
|  18770924 |   65 | `PH7_PRIVATE sxi32 SyMemcmp(const void *pB1,const void *pB2,sxu32 nSize)` |
|         1 |   66 |  |
|         - |   67 | `	sxi32 rc;` |
|  18770925 |   68 | `	if( nSize <= 0 ){` |
|         5 |   69 | `		return 0;` |
|         - |   70 | `	}` |
|  18770921 |   71 | `	if( pB1 == 0 \|\| pB2 == 0 ){` |
|       ! 0 |   72 | `		return pB1 != 0 ? 1 : (pB2 == 0 ? 0 : -1);` |
|         - |   73 | `	}` |
|  23327161 |   74 | `	SX_MACRO_FAST_CMP(pB1,pB2,nSize,rc);` |
|  18770921 |   75 | `	return rc;` |
|   9385462 |   76 |  |
|    814786 |   77 | `PH7_PRIVATE sxu32 SyMemcpy(const void *pSrc,void *pDest,sxu32 nLen)` |
|         1 |   78 |  |
|         - |   79 | `#if defined(UNTRUST)` |
|         - |   80 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|         - |   81 | `		return 0;` |
|         - |   82 | `	}` |
|         - |   83 | `#endif` |
|    814787 |   84 | `	if( pSrc == (const void *)pDest ){` |
|       ! 0 |   85 | `		return nLen;` |
|         - |   86 | `	}` |
|   6017529 |   87 | `	SX_MACRO_FAST_MEMCPY(pSrc,pDest,nLen);` |
|    814787 |   88 | `	return nLen;` |
|    407395 |   89 |  |
|   7518454 |   90 | `static void * MemOSAlloc(sxu32 nBytes)` |
|         1 |   91 |  |
|         - |   92 | `	sxu32 *pChunk;` |
|   7518455 |   93 | `	pChunk = (sxu32 *)SyOSHeapAlloc(nBytes + sizeof(sxu32));` |
|   7518455 |   94 | `	if( pChunk == 0 ){` |
|       ! 0 |   95 | `		return 0;` |
|         - |   96 | `	}` |
|   7518455 |   97 | `	pChunk[0] = nBytes;` |
|   7518455 |   98 | `	return (void *)&pChunk[1];` |
|   3759228 |   99 |  |
|    270344 |  100 | `static void * MemOSRealloc(void *pOld,sxu32 nBytes)` |
|         1 |  101 |  |
|         - |  102 | `	sxu32 *pOldChunk;` |
|         - |  103 | `	sxu32 *pChunk;` |
|    270345 |  104 | `	pOldChunk = (sxu32 *)(((char *)pOld)-sizeof(sxu32));` |
|    270345 |  105 | `	if( pOldChunk[0] >= nBytes ){` |
|       ! 0 |  106 | `		return pOld;` |
|         - |  107 | `	}` |
|    270345 |  108 | `	pChunk = (sxu32 *)SyOSHeapRealloc(pOldChunk,nBytes + sizeof(sxu32));` |
|    270345 |  109 | `	if( pChunk == 0 ){` |
|       ! 0 |  110 | `		return 0;` |
|         - |  111 | `	}` |
|    270345 |  112 | `	pChunk[0] = nBytes;` |
|    270345 |  113 | `	return (void *)&pChunk[1];` |
|    135163 |  114 |  |
|   7505406 |  115 | `static void MemOSFree(void *pBlock)` |
|         1 |  116 |  |
|         - |  117 | `	void *pChunk;` |
|   7505407 |  118 | `	pChunk = (void *)(((char *)pBlock)-sizeof(sxu32));` |
|   7505407 |  119 | `	SyOSHeapFree(pChunk);` |
|   7505407 |  120 |  |
|       ! 0 |  121 | `static sxu32 MemOSChunkSize(void *pBlock)` |
|       ! 0 |  122 |  |
|         - |  123 | `	sxu32 *pChunk;` |
|       ! 0 |  124 | `	pChunk = (sxu32 *)(((char *)pBlock)-sizeof(sxu32));` |
|       ! 0 |  125 | `	return pChunk[0];` |
|       ! 0 |  126 |  |
|         - |  127 | `/* Export OS allocation methods */` |
|         - |  128 | `static const SyMemMethods sOSAllocMethods = {` |
|         - |  129 | `	MemOSAlloc,` |
|         - |  130 | `	MemOSRealloc,` |
|         - |  131 | `	MemOSFree,` |
|         - |  132 | `	MemOSChunkSize,` |
|         - |  133 | `	0,` |
|         - |  134 | `	0,` |
|         - |  135 | `	0` |
|         - |  136 | `};` |
|   7518454 |  137 | `static void * MemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         1 |  138 |  |
|         - |  139 | `	SyMemBlock *pBlock;` |
|   7518455 |  140 | `	sxi32 nRetry = 0;` |
|         - |  141 |  |
|         - |  142 | `	/* Append an extra block so we can tracks allocated chunks and avoid memory` |
|         - |  143 | `	 * leaks.` |
|         - |  144 | `	 */` |
|   7518455 |  145 | `	nByte += sizeof(SyMemBlock);` |
|   3759227 |  146 | `	for(;;){` |
|   3759228 |  147 | `		pBlock = (SyMemBlock *)pBackend->pMethods->xAlloc(nByte);` |
|   7518454 |  148 | `		if( pBlock != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY` |
|         1 |  149 | `			\|\| SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|   3759228 |  150 | `				break;` |
|         - |  151 | `		}` |
|       ! 0 |  152 | `		nRetry++;` |
|       ! 0 |  153 | `	}` |
|   7518455 |  154 | `	if( pBlock  == 0 ){` |
|       ! 0 |  155 | `		return 0;` |
|         - |  156 | `	}` |
|   7518455 |  157 | `	pBlock->pNext = pBlock->pPrev = 0;` |
|         - |  158 | `	/* Link to the list of already tracked blocks */` |
|   7518455 |  159 | `	MACRO_LD_PUSH(pBackend->pBlocks,pBlock);` |
|         - |  160 | `#if defined(UNTRUST)` |
|         - |  161 | `	pBlock->nGuard = SXMEM_BACKEND_MAGIC;` |
|         - |  162 | `#endif` |
|   7518455 |  163 | `	pBackend->nBlock++;` |
|   7518455 |  164 | `	return (void *)&pBlock[1];` |
|   3759228 |  165 |  |
|   5557524 |  166 | `PH7_PRIVATE void * SyMemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         1 |  167 |  |
|         - |  168 | `	void *pChunk;` |
|         - |  169 | `#if defined(UNTRUST)` |
|         - |  170 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  171 | `		return 0;` |
|         - |  172 | `	}` |
|         - |  173 | `#endif` |
|   5557525 |  174 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  175 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  176 | `	}` |
|   5557525 |  177 | `	pChunk = MemBackendAlloc(&(*pBackend),nByte);` |
|   5557525 |  178 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  179 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  180 | `	}` |
|   5557525 |  181 | `	return pChunk;` |
|         1 |  182 |  |
|   2172656 |  183 | `static void * MemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         1 |  184 |  |
|         - |  185 | `	SyMemBlock *pBlock,*pNew,*pPrev,*pNext;` |
|   2172657 |  186 | `	sxu32 nRetry = 0;` |
|         - |  187 |  |
|   2172657 |  188 | `	if( pOld == 0 ){` |
|   1902313 |  189 | `		return MemBackendAlloc(&(*pBackend),nByte);` |
|         - |  190 | `	}` |
|    270345 |  191 | `	pBlock = (SyMemBlock *)(((char *)pOld) - sizeof(SyMemBlock));` |
|         - |  192 | `#if defined(UNTRUST)` |
|         - |  193 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|         - |  194 | `		return 0;` |
|         - |  195 | `	}` |
|         - |  196 | `#endif` |
|    270345 |  197 | `	nByte += sizeof(SyMemBlock);` |
|    270345 |  198 | `	pPrev = pBlock->pPrev;` |
|    270345 |  199 | `	pNext = pBlock->pNext;` |
|    135162 |  200 | `	for(;;){` |
|    135163 |  201 | `		pNew = (SyMemBlock *)pBackend->pMethods->xRealloc(pBlock,nByte);` |
|    270345 |  202 | `		if( pNew != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY \|\|` |
|       ! 0 |  203 | `			SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|    135163 |  204 | `				break;` |
|         - |  205 | `		}` |
|       ! 0 |  206 | `		nRetry++;` |
|       ! 0 |  207 | `	}` |
|    270345 |  208 | `	if( pNew == 0 ){` |
|       ! 0 |  209 | `		return 0;` |
|         - |  210 | `	}` |
|    270345 |  211 | `	if( pNew != pBlock ){` |
|    204308 |  212 | `		if( pPrev == 0 ){` |
|     75782 |  213 | `			pBackend->pBlocks = pNew;` |
|     54856 |  214 | `		}else{` |
|    128527 |  215 | `			pPrev->pNext = pNew;` |
|         - |  216 | `		}` |
|    204308 |  217 | `		if( pNext ){` |
|    204304 |  218 | `			pNext->pPrev = pNew;` |
|    113759 |  219 | `		}` |
|         - |  220 | `#if defined(UNTRUST)` |
|         - |  221 | `		pNew->nGuard = SXMEM_BACKEND_MAGIC;` |
|         - |  222 | `#endif` |
|    113761 |  223 | `	}` |
|    270345 |  224 | `	return (void *)&pNew[1];` |
|   1086319 |  225 |  |
|   2172656 |  226 | `PH7_PRIVATE void * SyMemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         1 |  227 |  |
|         - |  228 | `	void *pChunk;` |
|         - |  229 | `#if defined(UNTRUST)` |
|         - |  230 | `	if( SXMEM_BACKEND_CORRUPT(pBackend)  ){` |
|         - |  231 | `		return 0;` |
|         - |  232 | `	}` |
|         - |  233 | `#endif` |
|   2172657 |  234 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  235 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  236 | `	}` |
|   2172657 |  237 | `	pChunk = MemBackendRealloc(&(*pBackend),pOld,nByte);` |
|   2172657 |  238 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  239 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  240 | `	}` |
|   2172657 |  241 | `	return pChunk;` |
|         1 |  242 |  |
|   2097604 |  243 | `static sxi32 MemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|         1 |  244 |  |
|         - |  245 | `	SyMemBlock *pBlock;` |
|   2097605 |  246 | `	pBlock = (SyMemBlock *)(((char *)pChunk) - sizeof(SyMemBlock));` |
|         - |  247 | `#if defined(UNTRUST)` |
|         - |  248 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|         - |  249 | `		return SXERR_CORRUPT;` |
|         - |  250 | `	}` |
|         - |  251 | `#endif` |
|         - |  252 | `	/* Unlink from the list of active blocks */` |
|   2097605 |  253 | `	if( pBackend->nBlock > 0 ){` |
|         - |  254 | `		/* Release the block */` |
|         - |  255 | `#if defined(UNTRUST)` |
|         - |  256 | `		/* Mark as stale block */` |
|         - |  257 | `		pBlock->nGuard = 0x635B;` |
|         - |  258 | `#endif` |
|   2097605 |  259 | `		MACRO_LD_REMOVE(pBackend->pBlocks,pBlock);` |
|   2097605 |  260 | `		pBackend->nBlock--;` |
|   2097605 |  261 | `		pBackend->pMethods->xFree(pBlock);` |
|   1048802 |  262 | `	}` |
|   2097605 |  263 | `	return SXRET_OK;` |
|         1 |  264 |  |
|   2097604 |  265 | `PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|         1 |  266 |  |
|         - |  267 | `	sxi32 rc;` |
|         - |  268 | `#if defined(UNTRUST)` |
|         - |  269 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  270 | `		return SXERR_CORRUPT;` |
|         - |  271 | `	}` |
|         - |  272 | `#endif` |
|   2097605 |  273 | `	if( pChunk == 0 ){` |
|       ! 0 |  274 | `		return SXRET_OK;` |
|         - |  275 | `	}` |
|   2097605 |  276 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  277 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  278 | `	}` |
|   2097605 |  279 | `	rc = MemBackendFree(&(*pBackend),pChunk);` |
|   2097605 |  280 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  281 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  282 | `	}` |
|   2097605 |  283 | `	return rc;` |
|   1048803 |  284 |  |
|         - |  285 | `#if defined(PH7_ENABLE_THREADS)` |
|      5334 |  286 | `PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods)` |
|         1 |  287 |  |
|         - |  288 | `	SyMutex *pMutex;` |
|         - |  289 | `#if defined(UNTRUST)` |
|         - |  290 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pMethods == 0 \|\| pMethods->xNew == 0){` |
|         - |  291 | `		return SXERR_CORRUPT;` |
|         - |  292 | `	}` |
|         - |  293 | `#endif` |
|      5335 |  294 | `	pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|      5335 |  295 | `	if( pMutex == 0 ){` |
|       ! 0 |  296 | `		return SXERR_OS;` |
|         - |  297 | `	}` |
|         - |  298 | `	/* Attach the mutex to the memory backend */` |
|      5335 |  299 | `	pBackend->pMutex = pMutex;` |
|      5335 |  300 | `	pBackend->pMutexMethods = pMethods;` |
|      5335 |  301 | `	return SXRET_OK;` |
|      2668 |  302 |  |
|      5334 |  303 | `PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend)` |
|         1 |  304 |  |
|         - |  305 | `#if defined(UNTRUST)` |
|         - |  306 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  307 | `		return SXERR_CORRUPT;` |
|         - |  308 | `	}` |
|         - |  309 | `#endif` |
|      5335 |  310 | `	if( pBackend->pMutex == 0 ){` |
|         - |  311 | `		/* There is no mutex subsystem at all */` |
|       ! 0 |  312 | `		return SXRET_OK;` |
|         - |  313 | `	}` |
|      5335 |  314 | `	SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|      5335 |  315 | `	pBackend->pMutexMethods = 0;` |
|      5335 |  316 | `	pBackend->pMutex = 0;` |
|      5335 |  317 | `	return SXRET_OK;` |
|      2668 |  318 |  |
|         - |  319 | `#endif` |
|         - |  320 | `/*` |
|         - |  321 | ` * Memory pool allocator` |
|         - |  322 | ` */` |
|         - |  323 | `#define SXMEM_POOL_MAGIC		0xDEAD` |
|         - |  324 | `#define SXMEM_POOL_MAXALLOC		(1<<(SXMEM_POOL_NBUCKETS+SXMEM_POOL_INCR))` |
|         - |  325 | `#define SXMEM_POOL_MINALLOC		(1<<(SXMEM_POOL_INCR))` |
|     58618 |  326 | `static sxi32 MemPoolBucketAlloc(SyMemBackend *pBackend,sxu32 nBucket)` |
|         1 |  327 |  |
|         - |  328 | `	char *zBucket,*zBucketEnd;` |
|         - |  329 | `	SyMemHeader *pHeader;` |
|         - |  330 | `	sxu32 nBucketSize;` |
|         - |  331 |  |
|         - |  332 | `	/* Allocate one big block first */` |
|     58619 |  333 | `	zBucket = (char *)MemBackendAlloc(&(*pBackend),SXMEM_POOL_MAXALLOC);` |
|     58619 |  334 | `	if( zBucket == 0 ){` |
|       ! 0 |  335 | `		return SXERR_MEM;` |
|         - |  336 | `	}` |
|     58619 |  337 | `	zBucketEnd = &zBucket[SXMEM_POOL_MAXALLOC];` |
|         - |  338 | `	/* Divide the big block into mini bucket pool */` |
|     58619 |  339 | `	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);` |
|     58619 |  340 | `	pBackend->apPool[nBucket] = pHeader = (SyMemHeader *)zBucket;` |
|   6200152 |  341 | `	for(;;){` |
|  12400305 |  342 | `		if( &zBucket[nBucketSize] >= zBucketEnd ){` |
|     58619 |  343 | `			break;` |
|         - |  344 | `		}` |
|  12341687 |  345 | `		pHeader->pNext = (SyMemHeader *)&zBucket[nBucketSize];` |
|         - |  346 | `		/* Advance the cursor to the next available chunk */` |
|  12341687 |  347 | `		pHeader = pHeader->pNext;` |
|  12341687 |  348 | `		zBucket += nBucketSize;` |
|         1 |  349 | `	}` |
|     58619 |  350 | `	pHeader->pNext = 0;` |
|         - |  351 |  |
|     58619 |  352 | `	return SXRET_OK;` |
|     29310 |  353 |  |
|  13846132 |  354 | `static void * MemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         1 |  355 |  |
|         - |  356 | `	SyMemHeader *pBucket,*pNext;` |
|         - |  357 | `	sxu32 nBucketSize;` |
|         - |  358 | `	sxu32 nBucket;` |
|         - |  359 |  |
|  13846133 |  360 | `	if( nByte + sizeof(SyMemHeader) >= SXMEM_POOL_MAXALLOC ){` |
|         - |  361 | `		/* Allocate a big chunk directly */` |
|       ! 0 |  362 | `		pBucket = (SyMemHeader *)MemBackendAlloc(&(*pBackend),nByte+sizeof(SyMemHeader));` |
|       ! 0 |  363 | `		if( pBucket == 0 ){` |
|       ! 0 |  364 | `			return 0;` |
|         - |  365 | `		}` |
|         - |  366 | `		/* Record as big block */` |
|       ! 0 |  367 | `		pBucket->nBucket = (sxu32)(SXMEM_POOL_MAGIC << 16) \| SXU16_HIGH;` |
|       ! 0 |  368 | `		return (void *)(pBucket+1);` |
|         - |  369 | `	}` |
|         - |  370 | `	/* Locate the appropriate bucket */` |
|  13846133 |  371 | `	nBucket = 0;` |
|  13846133 |  372 | `	nBucketSize = SXMEM_POOL_MINALLOC;` |
|  68554567 |  373 | `	while( nByte + sizeof(SyMemHeader) > nBucketSize  ){` |
|  54708435 |  374 | `		nBucketSize <<= 1;` |
|  54708435 |  375 | `		nBucket++;` |
|         1 |  376 | `	}` |
|  13846133 |  377 | `	pBucket = pBackend->apPool[nBucket];` |
|  13846133 |  378 | `	if( pBucket == 0 ){` |
|         - |  379 | `		sxi32 rc;` |
|     58619 |  380 | `		rc = MemPoolBucketAlloc(&(*pBackend),nBucket);` |
|     58619 |  381 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  382 | `			return 0;` |
|         - |  383 | `		}` |
|     58619 |  384 | `		pBucket = pBackend->apPool[nBucket];` |
|     29309 |  385 | `	}` |
|         - |  386 | `	/* Remove from the free list */` |
|  13846133 |  387 | `	pNext = pBucket->pNext;` |
|  13846133 |  388 | `	pBackend->apPool[nBucket] = pNext;` |
|         - |  389 | `	/* Record bucket&magic number */` |
|  13846133 |  390 | `	pBucket->nBucket = (SXMEM_POOL_MAGIC << 16) \| nBucket;` |
|  13846133 |  391 | `	return (void *)&pBucket[1];` |
|   6923067 |  392 |  |
|  13846132 |  393 | `PH7_PRIVATE void * SyMemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         1 |  394 |  |
|         - |  395 | `	void *pChunk;` |
|         - |  396 | `#if defined(UNTRUST)` |
|         - |  397 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  398 | `		return 0;` |
|         - |  399 | `	}` |
|         - |  400 | `#endif` |
|  13846133 |  401 | `	if( pBackend->pMutexMethods ){` |
|      5335 |  402 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|      2667 |  403 | `	}` |
|  13846133 |  404 | `	pChunk = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|  13846133 |  405 | `	if( pBackend->pMutexMethods ){` |
|      5335 |  406 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|      2667 |  407 | `	}` |
|  13846133 |  408 | `	return pChunk;` |
|         1 |  409 |  |
|   4938792 |  410 | `static sxi32 MemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|         1 |  411 |  |
|         - |  412 | `	SyMemHeader *pHeader;` |
|         - |  413 | `	sxu32 nBucket;` |
|         - |  414 | `	/* Get the corresponding bucket */` |
|   4938793 |  415 | `	pHeader = (SyMemHeader *)(((char *)pChunk) - sizeof(SyMemHeader));` |
|         - |  416 | `	/* Sanity check to avoid misuse */` |
|   4938793 |  417 | `	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){` |
|       ! 0 |  418 | `		return SXERR_CORRUPT;` |
|         - |  419 | `	}` |
|   4938793 |  420 | `	nBucket = pHeader->nBucket & 0xFFFF;` |
|   4938793 |  421 | `	if( nBucket == SXU16_HIGH ){` |
|         - |  422 | `		/* Free the big block */` |
|       ! 0 |  423 | `		MemBackendFree(&(*pBackend),pHeader);` |
|       ! 0 |  424 | `	}else{` |
|         - |  425 | `		/* Return to the free list */` |
|   4938793 |  426 | `		pHeader->pNext = pBackend->apPool[nBucket & 0x0f];` |
|   4938793 |  427 | `		pBackend->apPool[nBucket & 0x0f] = pHeader;` |
|         - |  428 | `	}` |
|   4938793 |  429 | `	return SXRET_OK;` |
|   2469397 |  430 |  |
|   4938792 |  431 | `PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|         1 |  432 |  |
|         - |  433 | `	sxi32 rc;` |
|         - |  434 | `#if defined(UNTRUST)` |
|         - |  435 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pChunk == 0 ){` |
|         - |  436 | `		return SXERR_CORRUPT;` |
|         - |  437 | `	}` |
|         - |  438 | `#endif` |
|   4938793 |  439 | `	if( pBackend->pMutexMethods ){` |
|      5085 |  440 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|      2542 |  441 | `	}` |
|   4938793 |  442 | `	rc = MemBackendPoolFree(&(*pBackend),pChunk);` |
|   4938793 |  443 | `	if( pBackend->pMutexMethods ){` |
|      5085 |  444 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|      2542 |  445 | `	}` |
|   4938793 |  446 | `	return rc;` |
|         1 |  447 |  |
|         - |  448 | `#if 0` |
|         - |  449 | `static void * MemBackendPoolRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         - |  450 |  |
|         - |  451 | `	sxu32 nBucket,nBucketSize;` |
|         - |  452 | `	SyMemHeader *pHeader;` |
|         - |  453 | `	void * pNew;` |
|         - |  454 |  |
|         - |  455 | `	if( pOld == 0 ){` |
|         - |  456 | `		/* Allocate a new pool */` |
|         - |  457 | `		pNew = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|         - |  458 | `		return pNew;` |
|         - |  459 | `	}` |
|         - |  460 | `	/* Get the corresponding bucket */` |
|         - |  461 | `	pHeader = (SyMemHeader *)(((char *)pOld) - sizeof(SyMemHeader));` |
|         - |  462 | `	/* Sanity check to avoid misuse */` |
|         - |  463 | `	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){` |
|         - |  464 | `		return 0;` |
|         - |  465 | `	}` |
|         - |  466 | `	nBucket = pHeader->nBucket & 0xFFFF;` |
|         - |  467 | `	if( nBucket == SXU16_HIGH ){` |
|         - |  468 | `		/* Big block */` |
|         - |  469 | `		return MemBackendRealloc(&(*pBackend),pHeader,nByte);` |
|         - |  470 | `	}` |
|         - |  471 | `	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);` |
|         - |  472 | `	if( nBucketSize >= nByte + sizeof(SyMemHeader) ){` |
|         - |  473 | `		/* The old bucket can honor the requested size */` |
|         - |  474 | `		return pOld;` |
|         - |  475 | `	}` |
|         - |  476 | `	/* Allocate a new pool */` |
|         - |  477 | `	pNew = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|         - |  478 | `	if( pNew == 0 ){` |
|         - |  479 | `		return 0;` |
|         - |  480 | `	}` |
|         - |  481 | `	/* Copy the old data into the new block */` |
|         - |  482 | `	SyMemcpy(pOld,pNew,nBucketSize);` |
|         - |  483 | `	/* Free the stale block */` |
|         - |  484 | `	MemBackendPoolFree(&(*pBackend),pOld);` |
|         - |  485 | `	return pNew;` |
|         - |  486 |  |
|         - |  487 | `PH7_PRIVATE void * SyMemBackendPoolRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         - |  488 |  |
|         - |  489 | `	void *pChunk;` |
|         - |  490 | `#if defined(UNTRUST)` |
|         - |  491 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  492 | `		return 0;` |
|         - |  493 | `	}` |
|         - |  494 | `#endif` |
|         - |  495 | `	if( pBackend->pMutexMethods ){` |
|         - |  496 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|         - |  497 | `	}` |
|         - |  498 | `	pChunk = MemBackendPoolRealloc(&(*pBackend),pOld,nByte);` |
|         - |  499 | `	if( pBackend->pMutexMethods ){` |
|         - |  500 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|         - |  501 | `	}` |
|         - |  502 | `	return pChunk;` |
|         - |  503 |  |
|         - |  504 | `#endif` |
|      5334 |  505 | `PH7_PRIVATE sxi32 SyMemBackendInit(SyMemBackend *pBackend,ProcMemError xMemErr,void * pUserData)` |
|         1 |  506 |  |
|         - |  507 | `#if defined(UNTRUST)` |
|         - |  508 | `	if( pBackend == 0 ){` |
|         - |  509 | `		return SXERR_EMPTY;` |
|         - |  510 | `	}` |
|         - |  511 | `#endif` |
|         - |  512 | `	/* Zero the allocator first */` |
|      5335 |  513 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|      5335 |  514 | `	pBackend->xMemError = xMemErr;` |
|      5335 |  515 | `	pBackend->pUserData = pUserData;` |
|         - |  516 | `	/* Switch to the OS memory allocator */` |
|      5335 |  517 | `	pBackend->pMethods = &sOSAllocMethods;` |
|      5335 |  518 | `	if( pBackend->pMethods->xInit ){` |
|         - |  519 | `		/* Initialize the backend  */` |
|       ! 0 |  520 | `		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){` |
|       ! 0 |  521 | `			return SXERR_ABORT;` |
|         - |  522 | `		}` |
|       ! 0 |  523 | `	}` |
|         - |  524 | `#if defined(UNTRUST)` |
|         - |  525 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|         - |  526 | `#endif` |
|      5335 |  527 | `	return SXRET_OK;` |
|      2668 |  528 |  |
|       ! 0 |  529 | `PH7_PRIVATE sxi32 SyMemBackendInitFromOthers(SyMemBackend *pBackend,const SyMemMethods *pMethods,ProcMemError xMemErr,void * pUserData)` |
|       ! 0 |  530 |  |
|         - |  531 | `#if defined(UNTRUST)` |
|         - |  532 | `	if( pBackend == 0 \|\| pMethods == 0){` |
|         - |  533 | `		return SXERR_EMPTY;` |
|         - |  534 | `	}` |
|         - |  535 | `#endif` |
|       ! 0 |  536 | `	if( pMethods->xAlloc == 0 \|\| pMethods->xRealloc == 0 \|\| pMethods->xFree == 0 \|\| pMethods->xChunkSize == 0 ){` |
|         - |  537 | `		/* mandatory methods are missing */` |
|       ! 0 |  538 | `		return SXERR_INVALID;` |
|         - |  539 | `	}` |
|         - |  540 | `	/* Zero the allocator first */` |
|       ! 0 |  541 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|       ! 0 |  542 | `	pBackend->xMemError = xMemErr;` |
|       ! 0 |  543 | `	pBackend->pUserData = pUserData;` |
|         - |  544 | `	/* Switch to the host application memory allocator */` |
|       ! 0 |  545 | `	pBackend->pMethods = pMethods;` |
|       ! 0 |  546 | `	if( pBackend->pMethods->xInit ){` |
|         - |  547 | `		/* Initialize the backend  */` |
|       ! 0 |  548 | `		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){` |
|       ! 0 |  549 | `			return SXERR_ABORT;` |
|         - |  550 | `		}` |
|       ! 0 |  551 | `	}` |
|         - |  552 | `#if defined(UNTRUST)` |
|         - |  553 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|         - |  554 | `#endif` |
|       ! 0 |  555 | `	return SXRET_OK;` |
|       ! 0 |  556 |  |
|     10668 |  557 | `PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend *pBackend,SyMemBackend *pParent)` |
|         1 |  558 |  |
|         - |  559 | `	sxu8 bInheritMutex;` |
|         - |  560 | `#if defined(UNTRUST)` |
|         - |  561 | `	if( pBackend == 0 \|\| SXMEM_BACKEND_CORRUPT(pParent) ){` |
|         - |  562 | `		return SXERR_CORRUPT;` |
|         - |  563 | `	}` |
|         - |  564 | `#endif` |
|         - |  565 | `	/* Zero the allocator first */` |
|     10669 |  566 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|     10669 |  567 | `	pBackend->pMethods  = pParent->pMethods;` |
|     10669 |  568 | `	pBackend->xMemError = pParent->xMemError;` |
|     10669 |  569 | `	pBackend->pUserData = pParent->pUserData;` |
|     10669 |  570 | `	bInheritMutex = pParent->pMutexMethods ? TRUE : FALSE;` |
|     10669 |  571 | `	if( bInheritMutex ){` |
|      5335 |  572 | `		pBackend->pMutexMethods = pParent->pMutexMethods;` |
|         - |  573 | `		/* Create a private mutex */` |
|      5335 |  574 | `		pBackend->pMutex = pBackend->pMutexMethods->xNew(SXMUTEX_TYPE_FAST);` |
|      5335 |  575 | `		if( pBackend->pMutex ==  0){` |
|       ! 0 |  576 | `			return SXERR_OS;` |
|         - |  577 | `		}` |
|      2667 |  578 | `	}` |
|         - |  579 | `#if defined(UNTRUST)` |
|         - |  580 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|         - |  581 | `#endif` |
|     10669 |  582 | `	return SXRET_OK;` |
|      5335 |  583 |  |
|     10894 |  584 | `static sxi32 MemBackendRelease(SyMemBackend *pBackend)` |
|         1 |  585 |  |
|         - |  586 | `	SyMemBlock *pBlock,*pNext;` |
|         - |  587 |  |
|     10895 |  588 | `	pBlock = pBackend->pBlocks;` |
|    679597 |  589 | `	for(;;){` |
|   1359195 |  590 | `		if( pBackend->nBlock == 0 ){` |
|       783 |  591 | `			break;` |
|         - |  592 | `		}` |
|   1358413 |  593 | `		pNext  = pBlock->pNext;` |
|   1358413 |  594 | `		pBackend->pMethods->xFree(pBlock);` |
|   1358413 |  595 | `		pBlock = pNext;` |
|   1358413 |  596 | `		pBackend->nBlock--;` |
|         - |  597 | `		/* LOOP ONE */` |
|   1358413 |  598 | `		if( pBackend->nBlock == 0 ){` |
|      6413 |  599 | `			break;` |
|         - |  600 | `		}` |
|   1352001 |  601 | `		pNext  = pBlock->pNext;` |
|   1352001 |  602 | `		pBackend->pMethods->xFree(pBlock);` |
|   1352001 |  603 | `		pBlock = pNext;` |
|   1352001 |  604 | `		pBackend->nBlock--;` |
|         - |  605 | `		/* LOOP TWO */` |
|   1352001 |  606 | `		if( pBackend->nBlock == 0 ){` |
|      2911 |  607 | `			break;` |
|         - |  608 | `		}` |
|   1349091 |  609 | `		pNext  = pBlock->pNext;` |
|   1349091 |  610 | `		pBackend->pMethods->xFree(pBlock);` |
|   1349091 |  611 | `		pBlock = pNext;` |
|   1349091 |  612 | `		pBackend->nBlock--;` |
|         - |  613 | `		/* LOOP THREE */` |
|   1349091 |  614 | `		if( pBackend->nBlock == 0 ){` |
|       791 |  615 | `			break;` |
|         - |  616 | `		}` |
|   1348301 |  617 | `		pNext  = pBlock->pNext;` |
|   1348301 |  618 | `		pBackend->pMethods->xFree(pBlock);` |
|   1348301 |  619 | `		pBlock = pNext;` |
|   1348301 |  620 | `		pBackend->nBlock--;` |
|         - |  621 | `		/* LOOP FOUR */` |
|         1 |  622 | `	}` |
|     10895 |  623 | `	if( pBackend->pMethods->xRelease ){` |
|       ! 0 |  624 | `		pBackend->pMethods->xRelease(pBackend->pMethods->pUserData);` |
|       ! 0 |  625 | `	}` |
|     10895 |  626 | `	pBackend->pMethods = 0;` |
|     10895 |  627 | `	pBackend->pBlocks  = 0;` |
|         - |  628 | `#if defined(UNTRUST)` |
|         - |  629 | `	pBackend->nMagic = 0x2626;` |
|         - |  630 | `#endif` |
|     10895 |  631 | `	return SXRET_OK;` |
|         1 |  632 |  |
|     10894 |  633 | `PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend *pBackend)` |
|         1 |  634 |  |
|         - |  635 | `#if defined(UNTRUST)` |
|         - |  636 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  637 | `		return SXERR_INVALID;` |
|         - |  638 | `	}` |
|         - |  639 | `#endif` |
|     10895 |  640 | `	if( pBackend->pMutexMethods ){` |
|       243 |  641 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       121 |  642 | `	}` |
|     10895 |  643 | `	(void)MemBackendRelease(&(*pBackend));` |
|     10895 |  644 | `	if( pBackend->pMutexMethods ){` |
|       243 |  645 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       243 |  646 | `		SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|       121 |  647 | `	}` |
|     10895 |  648 | `	return SXRET_OK;` |
|         1 |  649 |  |
|     96950 |  650 | `PH7_PRIVATE void * SyMemBackendDup(SyMemBackend *pBackend,const void *pSrc,sxu32 nSize)` |
|         1 |  651 |  |
|         - |  652 | `	void *pNew;` |
|         - |  653 | `#if defined(UNTRUST)` |
|         - |  654 | `	if( pSrc == 0 \|\| nSize <= 0 ){` |
|         - |  655 | `		return 0;` |
|         - |  656 | `	}` |
|         - |  657 | `#endif` |
|     96951 |  658 | `	pNew = SyMemBackendAlloc(&(*pBackend),nSize);` |
|     96951 |  659 | `	if( pNew ){` |
|     96951 |  660 | `		SyMemcpy(pSrc,pNew,nSize);` |
|     48475 |  661 | `	}` |
|     96951 |  662 | `	return pNew;` |
|         1 |  663 |  |
|   3752836 |  664 | `PH7_PRIVATE char * SyMemBackendStrDup(SyMemBackend *pBackend,const char *zSrc,sxu32 nSize)` |
|         1 |  665 |  |
|         - |  666 | `	char *zDest;` |
|   3752837 |  667 | `	zDest = (char *)SyMemBackendAlloc(&(*pBackend),nSize + 1);` |
|   3752837 |  668 | `	if( zDest ){` |
|   3752837 |  669 | `		Systrcpy(zDest,nSize+1,zSrc,nSize);` |
|   1876418 |  670 | `	}` |
|   3752837 |  671 | `	return zDest;` |
|         1 |  672 |  |
|    155082 |  673 | `PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize)` |
|         1 |  674 |  |
|         - |  675 | `#if defined(UNTRUST)` |
|         - |  676 | `	if( pBlob == 0 \|\| pBuffer == 0 \|\| nSize < 1 ){` |
|         - |  677 | `		return SXERR_EMPTY;` |
|         - |  678 | `	}` |
|         - |  679 | `#endif` |
|    155083 |  680 | `	pBlob->pBlob = pBuffer;` |
|    155083 |  681 | `	pBlob->mByte = nSize;` |
|    155083 |  682 | `	pBlob->nByte = 0;` |
|    155083 |  683 | `	pBlob->pAllocator = 0;` |
|    155083 |  684 | `	pBlob->nFlags = SXBLOB_LOCKED\|SXBLOB_STATIC;` |
|    155083 |  685 | `	return SXRET_OK;` |
|         1 |  686 |  |
|   1516902 |  687 | `PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator)` |
|         1 |  688 |  |
|         - |  689 | `#if defined(UNTRUST)` |
|         - |  690 | `	if( pBlob == 0  ){` |
|         - |  691 | `		return SXERR_EMPTY;` |
|         - |  692 | `	}` |
|         - |  693 | `#endif` |
|   1516903 |  694 | `	pBlob->pBlob = 0;` |
|   1516903 |  695 | `	pBlob->mByte = pBlob->nByte	= 0;` |
|   1516903 |  696 | `	pBlob->pAllocator = &(*pAllocator);` |
|   1516903 |  697 | `	pBlob->nFlags = 0;` |
|   1516903 |  698 | `	return SXRET_OK;` |
|         1 |  699 |  |
|    150246 |  700 | `PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte)` |
|         1 |  701 |  |
|         - |  702 | `#if defined(UNTRUST)` |
|         - |  703 | `	if( pBlob == 0  ){` |
|         - |  704 | `		return SXERR_EMPTY;` |
|         - |  705 | `	}` |
|         - |  706 | `#endif` |
|    150247 |  707 | `	pBlob->pBlob = (void *)pData;` |
|    150247 |  708 | `	pBlob->nByte = nByte;` |
|    150247 |  709 | `	pBlob->mByte = 0;` |
|    150247 |  710 | `	pBlob->nFlags \|= SXBLOB_RDONLY;` |
|    150247 |  711 | `	return SXRET_OK;` |
|         1 |  712 |  |
|         - |  713 | `#ifndef SXBLOB_MIN_GROWTH` |
|         - |  714 | `#define SXBLOB_MIN_GROWTH 16` |
|         - |  715 | `#endif` |
|   2421324 |  716 | `static sxi32 BlobPrepareGrow(SyBlob *pBlob,sxu32 *pByte)` |
|         1 |  717 |  |
|         - |  718 | `	sxu32 nByte;` |
|         - |  719 | `	void *pNew;` |
|   2421325 |  720 | `	nByte = *pByte;` |
|   2421325 |  721 | `	if( pBlob->nFlags & (SXBLOB_LOCKED\|SXBLOB_STATIC) ){` |
|   1239887 |  722 | `		if ( SyBlobFreeSpace(pBlob) < nByte ){` |
|       ! 0 |  723 | `			*pByte = SyBlobFreeSpace(pBlob);` |
|       ! 0 |  724 | `			if( (*pByte) == 0 ){` |
|       ! 0 |  725 | `				return SXERR_SHORT;` |
|         - |  726 | `			}` |
|       ! 0 |  727 | `		}` |
|   1239887 |  728 | `		return SXRET_OK;` |
|         - |  729 | `	}` |
|   1181439 |  730 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|         - |  731 | `		/* Make a copy of the read-only item */` |
|     96951 |  732 | `		if( pBlob->nByte > 0 ){` |
|     96951 |  733 | `			pNew = SyMemBackendDup(pBlob->pAllocator,pBlob->pBlob,pBlob->nByte);` |
|     96951 |  734 | `			if( pNew == 0 ){` |
|       ! 0 |  735 | `				return SXERR_MEM;` |
|         - |  736 | `			}` |
|     96951 |  737 | `			pBlob->pBlob = pNew;` |
|     96951 |  738 | `			pBlob->mByte = pBlob->nByte;` |
|     48476 |  739 | `		}else{` |
|       ! 0 |  740 | `			pBlob->pBlob = 0;` |
|       ! 0 |  741 | `			pBlob->mByte = 0;` |
|         - |  742 | `		}` |
|         - |  743 | `		/* Remove the read-only flag */` |
|     96951 |  744 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|     48475 |  745 | `	}` |
|   1181439 |  746 | `	if( SyBlobFreeSpace(pBlob) >= nByte ){` |
|    333187 |  747 | `		return SXRET_OK;` |
|         - |  748 | `	}` |
|    848253 |  749 | `	if( pBlob->mByte > 0 ){` |
|     99171 |  750 | `		nByte = nByte + pBlob->mByte * 2 + SXBLOB_MIN_GROWTH;` |
|    798658 |  751 | `	}else if ( nByte < SXBLOB_MIN_GROWTH ){` |
|    693336 |  752 | `		nByte = SXBLOB_MIN_GROWTH;` |
|    346552 |  753 | `	}` |
|    848253 |  754 | `	pNew = SyMemBackendRealloc(pBlob->pAllocator,pBlob->pBlob,nByte);` |
|    848253 |  755 | `	if( pNew == 0 ){` |
|       ! 0 |  756 | `		return SXERR_MEM;` |
|         - |  757 | `	}` |
|    848253 |  758 | `	pBlob->pBlob = pNew;` |
|    848253 |  759 | `	pBlob->mByte = nByte;` |
|    848253 |  760 | `	return SXRET_OK;` |
|   1210663 |  761 |  |
|   2427118 |  762 | `PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize)` |
|         1 |  763 |  |
|         - |  764 | `	sxu8 *zBlob;` |
|         - |  765 | `	sxi32 rc;` |
|   2427119 |  766 | `	if( nSize < 1 ){` |
|      5795 |  767 | `		return SXRET_OK;` |
|         - |  768 | `	}` |
|   2421325 |  769 | `	rc = BlobPrepareGrow(&(*pBlob),&nSize);` |
|   2421325 |  770 | `	if( SXRET_OK != rc ){` |
|       ! 0 |  771 | `		return rc;` |
|         - |  772 | `	}` |
|   2421325 |  773 | `	if( pData ){` |
|   2421293 |  774 | `		zBlob = (sxu8 *)pBlob->pBlob ;` |
|   2421293 |  775 | `		zBlob = &zBlob[pBlob->nByte];` |
|   2421293 |  776 | `		pBlob->nByte += nSize;` |
|   4828058 |  777 | `		SX_MACRO_FAST_MEMCPY(pData,zBlob,nSize);` |
|   1210646 |  778 | `	}` |
|   2421325 |  779 | `	return SXRET_OK;` |
|   1213560 |  780 |  |
|     45678 |  781 | `PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob)` |
|         1 |  782 |  |
|         - |  783 | `	sxi32 rc;` |
|         - |  784 | `	sxu32 n;` |
|     45679 |  785 | `	n = pBlob->nByte;` |
|     45679 |  786 | `	rc = SyBlobAppend(&(*pBlob),(const void *)"\0",sizeof(char));` |
|     45679 |  787 | `	if (rc == SXRET_OK ){` |
|     45679 |  788 | `		pBlob->nByte = n;` |
|     22839 |  789 | `	}` |
|     45679 |  790 | `	return rc;` |
|         1 |  791 |  |
|    106910 |  792 | `PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest)` |
|         1 |  793 |  |
|    106911 |  794 | `	sxi32 rc = SXRET_OK;` |
|         - |  795 | `#ifdef UNTRUST` |
|         - |  796 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|         - |  797 | `		return SXERR_EMPTY;` |
|         - |  798 | `	}` |
|         - |  799 | `#endif` |
|    106911 |  800 | `	if( pSrc->nByte > 0 ){` |
|    106911 |  801 | `		rc = SyBlobAppend(&(*pDest),pSrc->pBlob,pSrc->nByte);` |
|     53455 |  802 | `	}` |
|    106911 |  803 | `	return rc;` |
|         1 |  804 |  |
|         8 |  805 | `PH7_PRIVATE sxi32 SyBlobCmp(SyBlob *pLeft,SyBlob *pRight)` |
|         1 |  806 |  |
|         - |  807 | `	sxi32 rc;` |
|         - |  808 | `#ifdef UNTRUST` |
|         - |  809 | `	if( pLeft == 0 \|\| pRight == 0 ){` |
|         - |  810 | `		return pLeft ? 1 : -1;` |
|         - |  811 | `	}` |
|         - |  812 | `#endif` |
|         9 |  813 | `	if( pLeft->nByte != pRight->nByte ){` |
|         - |  814 | `		/* Length differ */` |
|       ! 0 |  815 | `		return pLeft->nByte - pRight->nByte;` |
|         - |  816 | `	}` |
|         9 |  817 | `	if( pLeft->nByte == 0 ){` |
|       ! 0 |  818 | `		return 0;` |
|         - |  819 | `	}` |
|         - |  820 | `	/* Perform a standard memcmp() operation */` |
|         9 |  821 | `	rc = SyMemcmp(pLeft->pBlob,pRight->pBlob,pLeft->nByte);` |
|         9 |  822 | `	return rc;` |
|         5 |  823 |  |
|    166576 |  824 | `PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob)` |
|         1 |  825 |  |
|    166577 |  826 | `	pBlob->nByte = 0;` |
|    166577 |  827 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|       271 |  828 | `		pBlob->pBlob = 0;` |
|       271 |  829 | `		pBlob->mByte = 0;` |
|       271 |  830 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|       135 |  831 | `	}` |
|    166577 |  832 | `	return SXRET_OK;` |
|         1 |  833 |  |
|    814292 |  834 | `PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob)` |
|         1 |  835 |  |
|    814293 |  836 | `	if( (pBlob->nFlags & (SXBLOB_STATIC\|SXBLOB_RDONLY)) == 0 && pBlob->mByte > 0 ){` |
|    246271 |  837 | `		SyMemBackendFree(pBlob->pAllocator,pBlob->pBlob);` |
|    123135 |  838 | `	}` |
|    814293 |  839 | `	pBlob->pBlob = 0;` |
|    814293 |  840 | `	pBlob->nByte = pBlob->mByte = 0;` |
|    814293 |  841 | `	pBlob->nFlags = 0;` |
|    814293 |  842 | `	return SXRET_OK;` |
|         1 |  843 |  |
|         - |  844 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|       294 |  845 | `PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft)` |
|         1 |  846 |  |
|       295 |  847 | `	const char *zIn = (const char *)pBlob;` |
|         - |  848 | `	const char *zEnd;` |
|         - |  849 | `	sxi32 rc;` |
|       295 |  850 | `	if( pLen > nLen ){` |
|        51 |  851 | `		return SXERR_NOTFOUND;` |
|         - |  852 | `	}` |
|       245 |  853 | `	zEnd = &zIn[nLen-pLen];` |
|      1666 |  854 | `	for(;;){` |
|      3359 |  855 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|      3283 |  856 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|      3145 |  857 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|      3098 |  858 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|         1 |  859 | `	}` |
|        79 |  860 | `	return SXERR_NOTFOUND;` |
|       148 |  861 |  |
|         - |  862 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - |  863 |  |
