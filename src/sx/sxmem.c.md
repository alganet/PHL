# src/sx/sxmem.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 431/505 lines (85.35%)

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
|  11640594 |   18 | `static void * SyOSHeapAlloc(sxu32 nByte)` |
|         2 |   19 |  |
|         - |   20 | `	void *pNew;` |
|         - |   21 | `#if defined(__WINNT__)` |
|         2 |   22 | `	pNew = HeapAlloc(GetProcessHeap(),0,nByte);` |
|         - |   23 | `#else` |
|  11640594 |   24 | `	pNew = malloc((size_t)nByte);` |
|         - |   25 | `#endif` |
|  11640596 |   26 | `	return pNew;` |
|         2 |   27 |  |
|    732103 |   28 | `static void * SyOSHeapRealloc(void *pOld,sxu32 nByte)` |
|         2 |   29 |  |
|         - |   30 | `	void *pNew;` |
|         - |   31 | `#if defined(__WINNT__)` |
|         2 |   32 | `	pNew = HeapReAlloc(GetProcessHeap(),0,pOld,nByte);` |
|         - |   33 | `#else` |
|    732103 |   34 | `	pNew = realloc(pOld,(size_t)nByte);` |
|         - |   35 | `#endif` |
|    732105 |   36 | `	return pNew;` |
|         2 |   37 |  |
|  11628920 |   38 | `static void SyOSHeapFree(void *pPtr)` |
|         2 |   39 |  |
|         - |   40 | `#if defined(__WINNT__)` |
|         2 |   41 | `	HeapFree(GetProcessHeap(),0,pPtr);` |
|         - |   42 | `#else` |
|  11628920 |   43 | `	free(pPtr);` |
|         - |   44 | `#endif` |
|  11628922 |   45 |  |
|         - |   46 |  |
|         - |   47 |  |
|  20309820 |   48 | `PH7_PRIVATE void SyZero(void *pSrc,sxu32 nSize)` |
|         2 |   49 |  |
|  20309822 |   50 | `	register unsigned char *zSrc = (unsigned char *)pSrc;` |
|         - |   51 | `	unsigned char *zEnd;` |
|         - |   52 | `#if defined(UNTRUST)` |
|         - |   53 | `	if( zSrc == 0 \|\| nSize <= 0 ){` |
|         - |   54 | `		return ;` |
|         - |   55 | `	}` |
|         - |   56 | `#endif` |
|  20309822 |   57 | `	zEnd = &zSrc[nSize];` |
| 267781287 |   58 | `	for(;;){` |
| 535559688 |   59 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 515249872 |   60 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 515249870 |   61 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 515249870 |   62 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
|         2 |   63 | `	}` |
|  20309822 |   64 |  |
|  18197762 |   65 | `PH7_PRIVATE sxi32 SyMemcmp(const void *pB1,const void *pB2,sxu32 nSize)` |
|         2 |   66 |  |
|         - |   67 | `	sxi32 rc;` |
|  18197764 |   68 | `	if( nSize <= 0 ){` |
|        86 |   69 | `		return 0;` |
|         - |   70 | `	}` |
|  18197680 |   71 | `	if( pB1 == 0 \|\| pB2 == 0 ){` |
|       ! 0 |   72 | `		return pB1 != 0 ? 1 : (pB2 == 0 ? 0 : -1);` |
|         - |   73 | `	}` |
|  36230373 |   74 | `	SX_MACRO_FAST_CMP(pB1,pB2,nSize,rc);` |
|  18197680 |   75 | `	return rc;` |
|   9099457 |   76 |  |
|   9402294 |   77 | `PH7_PRIVATE sxu32 SyMemcpy(const void *pSrc,void *pDest,sxu32 nLen)` |
|         2 |   78 |  |
|         - |   79 | `#if defined(UNTRUST)` |
|         - |   80 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|         - |   81 | `		return 0;` |
|         - |   82 | `	}` |
|         - |   83 | `#endif` |
|   9402296 |   84 | `	if( pSrc == (const void *)pDest ){` |
|       ! 0 |   85 | `		return nLen;` |
|         - |   86 | `	}` |
|  76829804 |   87 | `	SX_MACRO_FAST_MEMCPY(pSrc,pDest,nLen);` |
|   9402296 |   88 | `	return nLen;` |
|   4701355 |   89 |  |
|  11640594 |   90 | `static void * MemOSAlloc(sxu32 nBytes)` |
|         2 |   91 |  |
|         - |   92 | `	sxu32 *pChunk;` |
|  11640596 |   93 | `	pChunk = (sxu32 *)SyOSHeapAlloc(nBytes + sizeof(sxu32));` |
|  11640596 |   94 | `	if( pChunk == 0 ){` |
|       ! 0 |   95 | `		return 0;` |
|         - |   96 | `	}` |
|  11640596 |   97 | `	pChunk[0] = nBytes;` |
|  11640596 |   98 | `	return (void *)&pChunk[1];` |
|   5820321 |   99 |  |
|    732103 |  100 | `static void * MemOSRealloc(void *pOld,sxu32 nBytes)` |
|         2 |  101 |  |
|         - |  102 | `	sxu32 *pOldChunk;` |
|         - |  103 | `	sxu32 *pChunk;` |
|    732105 |  104 | `	pOldChunk = (sxu32 *)(((char *)pOld)-sizeof(sxu32));` |
|    732105 |  105 | `	if( pOldChunk[0] >= nBytes ){` |
|       ! 0 |  106 | `		return pOld;` |
|         - |  107 | `	}` |
|    732105 |  108 | `	pChunk = (sxu32 *)SyOSHeapRealloc(pOldChunk,nBytes + sizeof(sxu32));` |
|    732105 |  109 | `	if( pChunk == 0 ){` |
|       ! 0 |  110 | `		return 0;` |
|         - |  111 | `	}` |
|    732105 |  112 | `	pChunk[0] = nBytes;` |
|    732105 |  113 | `	return (void *)&pChunk[1];` |
|    366046 |  114 |  |
|  11628920 |  115 | `static void MemOSFree(void *pBlock)` |
|         2 |  116 |  |
|         - |  117 | `	void *pChunk;` |
|  11628922 |  118 | `	pChunk = (void *)(((char *)pBlock)-sizeof(sxu32));` |
|  11628922 |  119 | `	SyOSHeapFree(pChunk);` |
|  11628922 |  120 |  |
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
|         - |  133 |  |
|         - |  134 |  |
|         - |  135 |  |
|         - |  136 | `};` |
|  11640594 |  137 | `static void * MemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         2 |  138 |  |
|         - |  139 | `	SyMemBlock *pBlock;` |
|  11640596 |  140 | `	sxi32 nRetry = 0;` |
|         - |  141 |  |
|         - |  142 | `	/* Append an extra block so we can tracks allocated chunks and avoid memory` |
|         - |  143 | `	 * leaks.` |
|         - |  144 | `	 */` |
|  11640596 |  145 | `	nByte += sizeof(SyMemBlock);` |
|   5820319 |  146 | `	for(;;){` |
|   5820321 |  147 | `		pBlock = (SyMemBlock *)pBackend->pMethods->xAlloc(nByte);` |
|  11640594 |  148 | `		if( pBlock != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY` |
|         2 |  149 | `			\|\| SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|   5820321 |  150 | `				break;` |
|         - |  151 | `		}` |
|       ! 0 |  152 | `		nRetry++;` |
|       ! 0 |  153 | `	}` |
|  11640596 |  154 | `	if( pBlock  == 0 ){` |
|       ! 0 |  155 | `		return 0;` |
|         - |  156 | `	}` |
|  11640596 |  157 | `	pBlock->pNext = pBlock->pPrev = 0;` |
|         - |  158 | `	/* Link to the list of already tracked blocks */` |
|  11640596 |  159 | `	MACRO_LD_PUSH(pBackend->pBlocks,pBlock);` |
|         - |  160 | `#if defined(UNTRUST)` |
|         - |  161 | `	pBlock->nGuard = SXMEM_BACKEND_MAGIC;` |
|         - |  162 | `#endif` |
|  11640596 |  163 | `	pBackend->nBlock++;` |
|  11640596 |  164 | `	return (void *)&pBlock[1];` |
|   5820321 |  165 |  |
|   3706488 |  166 | `PH7_PRIVATE void * SyMemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         2 |  167 |  |
|         - |  168 | `	void *pChunk;` |
|         - |  169 | `#if defined(UNTRUST)` |
|         - |  170 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  171 | `		return 0;` |
|         - |  172 | `	}` |
|         - |  173 | `#endif` |
|   3706490 |  174 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  175 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  176 | `	}` |
|   3706490 |  177 | `	pChunk = MemBackendAlloc(&(*pBackend),nByte);` |
|   3706490 |  178 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  179 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  180 | `	}` |
|   3706490 |  181 | `	return pChunk;` |
|         2 |  182 |  |
|   8619393 |  183 | `static void * MemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         2 |  184 |  |
|         - |  185 | `	SyMemBlock *pBlock,*pNew,*pPrev,*pNext;` |
|   8619395 |  186 | `	sxu32 nRetry = 0;` |
|         - |  187 |  |
|   8619395 |  188 | `	if( pOld == 0 ){` |
|   7887292 |  189 | `		return MemBackendAlloc(&(*pBackend),nByte);` |
|         - |  190 | `	}` |
|    732105 |  191 | `	pBlock = (SyMemBlock *)(((char *)pOld) - sizeof(SyMemBlock));` |
|         - |  192 | `#if defined(UNTRUST)` |
|         - |  193 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|         - |  194 | `		return 0;` |
|         - |  195 | `	}` |
|         - |  196 | `#endif` |
|    732105 |  197 | `	nByte += sizeof(SyMemBlock);` |
|    732105 |  198 | `	pPrev = pBlock->pPrev;` |
|    732105 |  199 | `	pNext = pBlock->pNext;` |
|    366044 |  200 | `	for(;;){` |
|    366046 |  201 | `		pNew = (SyMemBlock *)pBackend->pMethods->xRealloc(pBlock,nByte);` |
|    732105 |  202 | `		if( pNew != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY \|\|` |
|       ! 0 |  203 | `			SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|    366046 |  204 | `				break;` |
|         - |  205 | `		}` |
|       ! 0 |  206 | `		nRetry++;` |
|       ! 0 |  207 | `	}` |
|    732105 |  208 | `	if( pNew == 0 ){` |
|       ! 0 |  209 | `		return 0;` |
|         - |  210 | `	}` |
|    732105 |  211 | `	if( pNew != pBlock ){` |
|    651985 |  212 | `		if( pPrev == 0 ){` |
|    511772 |  213 | `			pBackend->pBlocks = pNew;` |
|    278902 |  214 | `		}else{` |
|    140215 |  215 | `			pPrev->pNext = pNew;` |
|         - |  216 | `		}` |
|    651985 |  217 | `		if( pNext ){` |
|    651975 |  218 | `			pNext->pPrev = pNew;` |
|    349005 |  219 | `		}` |
|         - |  220 | `#if defined(UNTRUST)` |
|         - |  221 | `		pNew->nGuard = SXMEM_BACKEND_MAGIC;` |
|         - |  222 | `#endif` |
|    349010 |  223 | `	}` |
|    732105 |  224 | `	return (void *)&pNew[1];` |
|   4309713 |  225 |  |
|   8619393 |  226 | `PH7_PRIVATE void * SyMemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         2 |  227 |  |
|         - |  228 | `	void *pChunk;` |
|         - |  229 | `#if defined(UNTRUST)` |
|         - |  230 | `	if( SXMEM_BACKEND_CORRUPT(pBackend)  ){` |
|         - |  231 | `		return 0;` |
|         - |  232 | `	}` |
|         - |  233 | `#endif` |
|   8619395 |  234 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  235 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  236 | `	}` |
|   8619395 |  237 | `	pChunk = MemBackendRealloc(&(*pBackend),pOld,nByte);` |
|   8619395 |  238 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  239 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  240 | `	}` |
|   8619395 |  241 | `	return pChunk;` |
|         2 |  242 |  |
|   8733972 |  243 | `static sxi32 MemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|         2 |  244 |  |
|         - |  245 | `	SyMemBlock *pBlock;` |
|   8733974 |  246 | `	pBlock = (SyMemBlock *)(((char *)pChunk) - sizeof(SyMemBlock));` |
|         - |  247 | `#if defined(UNTRUST)` |
|         - |  248 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|         - |  249 | `		return SXERR_CORRUPT;` |
|         - |  250 | `	}` |
|         - |  251 | `#endif` |
|         - |  252 | `	/* Unlink from the list of active blocks */` |
|   8733974 |  253 | `	if( pBackend->nBlock > 0 ){` |
|         - |  254 | `		/* Release the block */` |
|         - |  255 | `#if defined(UNTRUST)` |
|         - |  256 | `		/* Mark as stale block */` |
|         - |  257 | `		pBlock->nGuard = 0x635B;` |
|         - |  258 | `#endif` |
|   8733974 |  259 | `		MACRO_LD_REMOVE(pBackend->pBlocks,pBlock);` |
|   8733974 |  260 | `		pBackend->nBlock--;` |
|   8733974 |  261 | `		pBackend->pMethods->xFree(pBlock);` |
|   4367008 |  262 | `	}` |
|   8733974 |  263 | `	return SXRET_OK;` |
|         2 |  264 |  |
|   8733972 |  265 | `PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|         2 |  266 |  |
|         - |  267 | `	sxi32 rc;` |
|         - |  268 | `#if defined(UNTRUST)` |
|         - |  269 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  270 | `		return SXERR_CORRUPT;` |
|         - |  271 | `	}` |
|         - |  272 | `#endif` |
|   8733974 |  273 | `	if( pChunk == 0 ){` |
|       ! 0 |  274 | `		return SXRET_OK;` |
|         - |  275 | `	}` |
|   8733974 |  276 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  277 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  278 | `	}` |
|   8733974 |  279 | `	rc = MemBackendFree(&(*pBackend),pChunk);` |
|   8733974 |  280 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  281 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  282 | `	}` |
|   8733974 |  283 | `	return rc;` |
|   4367010 |  284 |  |
|         - |  285 | `#if defined(PH7_ENABLE_THREADS)` |
|      2578 |  286 | `PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods)` |
|         2 |  287 |  |
|         - |  288 | `	SyMutex *pMutex;` |
|         - |  289 | `#if defined(UNTRUST)` |
|         - |  290 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pMethods == 0 \|\| pMethods->xNew == 0){` |
|         - |  291 | `		return SXERR_CORRUPT;` |
|         - |  292 | `	}` |
|         - |  293 | `#endif` |
|      2580 |  294 | `	pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|      2580 |  295 | `	if( pMutex == 0 ){` |
|       ! 0 |  296 | `		return SXERR_OS;` |
|         - |  297 | `	}` |
|         - |  298 | `	/* Attach the mutex to the memory backend */` |
|      2580 |  299 | `	pBackend->pMutex = pMutex;` |
|      2580 |  300 | `	pBackend->pMutexMethods = pMethods;` |
|      2580 |  301 | `	return SXRET_OK;` |
|      1291 |  302 |  |
|      2578 |  303 | `PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend)` |
|         2 |  304 |  |
|         - |  305 | `#if defined(UNTRUST)` |
|         - |  306 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  307 | `		return SXERR_CORRUPT;` |
|         - |  308 | `	}` |
|         - |  309 | `#endif` |
|      2580 |  310 | `	if( pBackend->pMutex == 0 ){` |
|         - |  311 | `		/* There is no mutex subsystem at all */` |
|       ! 0 |  312 | `		return SXRET_OK;` |
|         - |  313 | `	}` |
|      2580 |  314 | `	SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|      2580 |  315 | `	pBackend->pMutexMethods = 0;` |
|      2580 |  316 | `	pBackend->pMutex = 0;` |
|      2580 |  317 | `	return SXRET_OK;` |
|      1291 |  318 |  |
|         - |  319 | `#endif` |
|         - |  320 | `/*` |
|         - |  321 | ` * Memory pool allocator` |
|         - |  322 | ` */` |
|         - |  323 | `#define SXMEM_POOL_MAGIC		0xDEAD` |
|         - |  324 | `#define SXMEM_POOL_MAXALLOC		(1<<(SXMEM_POOL_NBUCKETS+SXMEM_POOL_INCR))` |
|         - |  325 | `#define SXMEM_POOL_MINALLOC		(1<<(SXMEM_POOL_INCR))` |
|     46816 |  326 | `static sxi32 MemPoolBucketAlloc(SyMemBackend *pBackend,sxu32 nBucket)` |
|         2 |  327 |  |
|         - |  328 | `	char *zBucket,*zBucketEnd;` |
|         - |  329 | `	SyMemHeader *pHeader;` |
|         - |  330 | `	sxu32 nBucketSize;` |
|         - |  331 |  |
|         - |  332 | `	/* Allocate one big block first */` |
|     46818 |  333 | `	zBucket = (char *)MemBackendAlloc(&(*pBackend),SXMEM_POOL_MAXALLOC);` |
|     46818 |  334 | `	if( zBucket == 0 ){` |
|       ! 0 |  335 | `		return SXERR_MEM;` |
|         - |  336 | `	}` |
|     46818 |  337 | `	zBucketEnd = &zBucket[SXMEM_POOL_MAXALLOC];` |
|         - |  338 | `	/* Divide the big block into mini bucket pool */` |
|     46818 |  339 | `	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);` |
|     46818 |  340 | `	pBackend->apPool[nBucket] = pHeader = (SyMemHeader *)zBucket;` |
|   5510392 |  341 | `	for(;;){` |
|  11020786 |  342 | `		if( &zBucket[nBucketSize] >= zBucketEnd ){` |
|     46818 |  343 | `			break;` |
|         - |  344 | `		}` |
|  10973970 |  345 | `		pHeader->pNext = (SyMemHeader *)&zBucket[nBucketSize];` |
|         - |  346 | `		/* Advance the cursor to the next available chunk */` |
|  10973970 |  347 | `		pHeader = pHeader->pNext;` |
|  10973970 |  348 | `		zBucket += nBucketSize;` |
|         2 |  349 | `	}` |
|     46818 |  350 | `	pHeader->pNext = 0;` |
|         - |  351 |  |
|     46818 |  352 | `	return SXRET_OK;` |
|     23410 |  353 |  |
|  13995234 |  354 | `static void * MemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         2 |  355 |  |
|         - |  356 | `	SyMemHeader *pBucket,*pNext;` |
|         - |  357 | `	sxu32 nBucketSize;` |
|         - |  358 | `	sxu32 nBucket;` |
|         - |  359 |  |
|  13995236 |  360 | `	if( nByte + sizeof(SyMemHeader) >= SXMEM_POOL_MAXALLOC ){` |
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
|  13995236 |  371 | `	nBucket = 0;` |
|  13995236 |  372 | `	nBucketSize = SXMEM_POOL_MINALLOC;` |
|  69840672 |  373 | `	while( nByte + sizeof(SyMemHeader) > nBucketSize  ){` |
|  55845438 |  374 | `		nBucketSize <<= 1;` |
|  55845438 |  375 | `		nBucket++;` |
|         2 |  376 | `	}` |
|  13995236 |  377 | `	pBucket = pBackend->apPool[nBucket];` |
|  13995236 |  378 | `	if( pBucket == 0 ){` |
|         - |  379 | `		sxi32 rc;` |
|     46818 |  380 | `		rc = MemPoolBucketAlloc(&(*pBackend),nBucket);` |
|     46818 |  381 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  382 | `			return 0;` |
|         - |  383 | `		}` |
|     46818 |  384 | `		pBucket = pBackend->apPool[nBucket];` |
|     23408 |  385 | `	}` |
|         - |  386 | `	/* Remove from the free list */` |
|  13995236 |  387 | `	pNext = pBucket->pNext;` |
|  13995236 |  388 | `	pBackend->apPool[nBucket] = pNext;` |
|         - |  389 | `	/* Record bucket&magic number */` |
|  13995236 |  390 | `	pBucket->nBucket = (SXMEM_POOL_MAGIC << 16) \| nBucket;` |
|  13995236 |  391 | `	return (void *)&pBucket[1];` |
|   6997619 |  392 |  |
|  13995234 |  393 | `PH7_PRIVATE void * SyMemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         2 |  394 |  |
|         - |  395 | `	void *pChunk;` |
|         - |  396 | `#if defined(UNTRUST)` |
|         - |  397 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  398 | `		return 0;` |
|         - |  399 | `	}` |
|         - |  400 | `#endif` |
|  13995236 |  401 | `	if( pBackend->pMutexMethods ){` |
|      2580 |  402 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1289 |  403 | `	}` |
|  13995236 |  404 | `	pChunk = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|  13995236 |  405 | `	if( pBackend->pMutexMethods ){` |
|      2580 |  406 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1289 |  407 | `	}` |
|  13995236 |  408 | `	return pChunk;` |
|         2 |  409 |  |
|   9071004 |  410 | `static sxi32 MemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|         2 |  411 |  |
|         - |  412 | `	SyMemHeader *pHeader;` |
|         - |  413 | `	sxu32 nBucket;` |
|         - |  414 | `	/* Get the corresponding bucket */` |
|   9071006 |  415 | `	pHeader = (SyMemHeader *)(((char *)pChunk) - sizeof(SyMemHeader));` |
|         - |  416 | `	/* Sanity check to avoid misuse */` |
|   9071006 |  417 | `	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){` |
|       ! 0 |  418 | `		return SXERR_CORRUPT;` |
|         - |  419 | `	}` |
|   9071006 |  420 | `	nBucket = pHeader->nBucket & 0xFFFF;` |
|   9071006 |  421 | `	if( nBucket == SXU16_HIGH ){` |
|         - |  422 | `		/* Free the big block */` |
|       ! 0 |  423 | `		MemBackendFree(&(*pBackend),pHeader);` |
|   9071006 |  424 | `	}else if( nBucket >= SXMEM_POOL_NBUCKETS + SXMEM_POOL_INCR ){` |
|         - |  425 | `		/* Corrupted or misused bucket index */` |
|       ! 0 |  426 | `		return SXERR_CORRUPT;` |
|       ! 0 |  427 | `	}else{` |
|         - |  428 | `		/* Return to the free list */` |
|   9071006 |  429 | `		pHeader->pNext = pBackend->apPool[nBucket];` |
|   9071006 |  430 | `		pBackend->apPool[nBucket] = pHeader;` |
|         - |  431 | `	}` |
|   9071006 |  432 | `	return SXRET_OK;` |
|   4535504 |  433 |  |
|   9071004 |  434 | `PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|         2 |  435 |  |
|         - |  436 | `	sxi32 rc;` |
|         - |  437 | `#if defined(UNTRUST)` |
|         - |  438 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pChunk == 0 ){` |
|         - |  439 | `		return SXERR_CORRUPT;` |
|         - |  440 | `	}` |
|         - |  441 | `#endif` |
|   9071006 |  442 | `	if( pBackend->pMutexMethods ){` |
|      2312 |  443 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1155 |  444 | `	}` |
|   9071006 |  445 | `	rc = MemBackendPoolFree(&(*pBackend),pChunk);` |
|   9071006 |  446 | `	if( pBackend->pMutexMethods ){` |
|      2312 |  447 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1155 |  448 | `	}` |
|   9071006 |  449 | `	return rc;` |
|         2 |  450 |  |
|         - |  451 | `#if 0` |
|         - |  452 | `static void * MemBackendPoolRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         - |  453 |  |
|         - |  454 | `	sxu32 nBucket,nBucketSize;` |
|         - |  455 | `	SyMemHeader *pHeader;` |
|         - |  456 | `	void * pNew;` |
|         - |  457 |  |
|         - |  458 | `	if( pOld == 0 ){` |
|         - |  459 | `		/* Allocate a new pool */` |
|         - |  460 | `		pNew = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|         - |  461 | `		return pNew;` |
|         - |  462 | `	}` |
|         - |  463 | `	/* Get the corresponding bucket */` |
|         - |  464 | `	pHeader = (SyMemHeader *)(((char *)pOld) - sizeof(SyMemHeader));` |
|         - |  465 | `	/* Sanity check to avoid misuse */` |
|         - |  466 | `	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){` |
|         - |  467 | `		return 0;` |
|         - |  468 | `	}` |
|         - |  469 | `	nBucket = pHeader->nBucket & 0xFFFF;` |
|         - |  470 | `	if( nBucket == SXU16_HIGH ){` |
|         - |  471 | `		/* Big block */` |
|         - |  472 | `		return MemBackendRealloc(&(*pBackend),pHeader,nByte);` |
|         - |  473 | `	}` |
|         - |  474 | `	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);` |
|         - |  475 | `	if( nBucketSize >= nByte + sizeof(SyMemHeader) ){` |
|         - |  476 | `		/* The old bucket can honor the requested size */` |
|         - |  477 | `		return pOld;` |
|         - |  478 | `	}` |
|         - |  479 | `	/* Allocate a new pool */` |
|         - |  480 | `	pNew = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|         - |  481 | `	if( pNew == 0 ){` |
|         - |  482 | `		return 0;` |
|         - |  483 | `	}` |
|         - |  484 | `	/* Copy the old data into the new block */` |
|         - |  485 | `	SyMemcpy(pOld,pNew,nBucketSize);` |
|         - |  486 | `	/* Free the stale block */` |
|         - |  487 | `	MemBackendPoolFree(&(*pBackend),pOld);` |
|         - |  488 | `	return pNew;` |
|         - |  489 |  |
|         - |  490 | `PH7_PRIVATE void * SyMemBackendPoolRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         - |  491 |  |
|         - |  492 | `	void *pChunk;` |
|         - |  493 | `#if defined(UNTRUST)` |
|         - |  494 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  495 | `		return 0;` |
|         - |  496 | `	}` |
|         - |  497 | `#endif` |
|         - |  498 | `	if( pBackend->pMutexMethods ){` |
|         - |  499 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|         - |  500 | `	}` |
|         - |  501 | `	pChunk = MemBackendPoolRealloc(&(*pBackend),pOld,nByte);` |
|         - |  502 | `	if( pBackend->pMutexMethods ){` |
|         - |  503 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|         - |  504 | `	}` |
|         - |  505 | `	return pChunk;` |
|         - |  506 |  |
|         - |  507 | `#endif` |
|      2578 |  508 | `PH7_PRIVATE sxi32 SyMemBackendInit(SyMemBackend *pBackend,ProcMemError xMemErr,void * pUserData)` |
|         2 |  509 |  |
|         - |  510 | `#if defined(UNTRUST)` |
|         - |  511 | `	if( pBackend == 0 ){` |
|         - |  512 | `		return SXERR_EMPTY;` |
|         - |  513 | `	}` |
|         - |  514 | `#endif` |
|         - |  515 | `	/* Zero the allocator first */` |
|      2580 |  516 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|      2580 |  517 | `	pBackend->xMemError = xMemErr;` |
|      2580 |  518 | `	pBackend->pUserData = pUserData;` |
|         - |  519 | `	/* Switch to the OS memory allocator */` |
|      2580 |  520 | `	pBackend->pMethods = &sOSAllocMethods;` |
|      2580 |  521 | `	if( pBackend->pMethods->xInit ){` |
|         - |  522 | `		/* Initialize the backend  */` |
|       ! 0 |  523 | `		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){` |
|       ! 0 |  524 | `			return SXERR_ABORT;` |
|         - |  525 | `		}` |
|       ! 0 |  526 | `	}` |
|         - |  527 | `#if defined(UNTRUST)` |
|         - |  528 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|         - |  529 | `#endif` |
|      2580 |  530 | `	return SXRET_OK;` |
|      1291 |  531 |  |
|       ! 0 |  532 | `PH7_PRIVATE sxi32 SyMemBackendInitFromOthers(SyMemBackend *pBackend,const SyMemMethods *pMethods,ProcMemError xMemErr,void * pUserData)` |
|       ! 0 |  533 |  |
|         - |  534 | `#if defined(UNTRUST)` |
|         - |  535 | `	if( pBackend == 0 \|\| pMethods == 0){` |
|         - |  536 | `		return SXERR_EMPTY;` |
|         - |  537 | `	}` |
|         - |  538 | `#endif` |
|       ! 0 |  539 | `	if( pMethods->xAlloc == 0 \|\| pMethods->xRealloc == 0 \|\| pMethods->xFree == 0 \|\| pMethods->xChunkSize == 0 ){` |
|         - |  540 | `		/* mandatory methods are missing */` |
|       ! 0 |  541 | `		return SXERR_INVALID;` |
|         - |  542 | `	}` |
|         - |  543 | `	/* Zero the allocator first */` |
|       ! 0 |  544 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|       ! 0 |  545 | `	pBackend->xMemError = xMemErr;` |
|       ! 0 |  546 | `	pBackend->pUserData = pUserData;` |
|         - |  547 | `	/* Switch to the host application memory allocator */` |
|       ! 0 |  548 | `	pBackend->pMethods = pMethods;` |
|       ! 0 |  549 | `	if( pBackend->pMethods->xInit ){` |
|         - |  550 | `		/* Initialize the backend  */` |
|       ! 0 |  551 | `		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){` |
|       ! 0 |  552 | `			return SXERR_ABORT;` |
|         - |  553 | `		}` |
|       ! 0 |  554 | `	}` |
|         - |  555 | `#if defined(UNTRUST)` |
|         - |  556 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|         - |  557 | `#endif` |
|       ! 0 |  558 | `	return SXRET_OK;` |
|       ! 0 |  559 |  |
|      5154 |  560 | `PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend *pBackend,SyMemBackend *pParent)` |
|         2 |  561 |  |
|         - |  562 | `	sxu8 bInheritMutex;` |
|         - |  563 | `#if defined(UNTRUST)` |
|         - |  564 | `	if( pBackend == 0 \|\| SXMEM_BACKEND_CORRUPT(pParent) ){` |
|         - |  565 | `		return SXERR_CORRUPT;` |
|         - |  566 | `	}` |
|         - |  567 | `#endif` |
|         - |  568 | `	/* Zero the allocator first */` |
|      5156 |  569 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|      5156 |  570 | `	pBackend->pMethods  = pParent->pMethods;` |
|      5156 |  571 | `	pBackend->xMemError = pParent->xMemError;` |
|      5156 |  572 | `	pBackend->pUserData = pParent->pUserData;` |
|      5156 |  573 | `	bInheritMutex = pParent->pMutexMethods ? TRUE : FALSE;` |
|      5156 |  574 | `	if( bInheritMutex ){` |
|      2580 |  575 | `		pBackend->pMutexMethods = pParent->pMutexMethods;` |
|         - |  576 | `		/* Create a private mutex */` |
|      2580 |  577 | `		pBackend->pMutex = pBackend->pMutexMethods->xNew(SXMUTEX_TYPE_FAST);` |
|      2580 |  578 | `		if( pBackend->pMutex ==  0){` |
|       ! 0 |  579 | `			return SXERR_OS;` |
|         - |  580 | `		}` |
|      1289 |  581 | `	}` |
|         - |  582 | `#if defined(UNTRUST)` |
|         - |  583 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|         - |  584 | `#endif` |
|      5156 |  585 | `	return SXRET_OK;` |
|      2579 |  586 |  |
|      5398 |  587 | `static sxi32 MemBackendRelease(SyMemBackend *pBackend)` |
|         2 |  588 |  |
|         - |  589 | `	SyMemBlock *pBlock,*pNext;` |
|         - |  590 |  |
|      5400 |  591 | `	pBlock = pBackend->pBlocks;` |
|    363723 |  592 | `	for(;;){` |
|    727448 |  593 | `		if( pBackend->nBlock == 0 ){` |
|       374 |  594 | `			break;` |
|         - |  595 | `		}` |
|    727076 |  596 | `		pNext  = pBlock->pNext;` |
|    727076 |  597 | `		pBackend->pMethods->xFree(pBlock);` |
|    727076 |  598 | `		pBlock = pNext;` |
|    727076 |  599 | `		pBackend->nBlock--;` |
|         - |  600 | `		/* LOOP ONE */` |
|    727076 |  601 | `		if( pBackend->nBlock == 0 ){` |
|      3590 |  602 | `			break;` |
|         - |  603 | `		}` |
|    723488 |  604 | `		pNext  = pBlock->pNext;` |
|    723488 |  605 | `		pBackend->pMethods->xFree(pBlock);` |
|    723488 |  606 | `		pBlock = pNext;` |
|    723488 |  607 | `		pBackend->nBlock--;` |
|         - |  608 | `		/* LOOP TWO */` |
|    723488 |  609 | `		if( pBackend->nBlock == 0 ){` |
|      1147 |  610 | `			break;` |
|         - |  611 | `		}` |
|    722342 |  612 | `		pNext  = pBlock->pNext;` |
|    722342 |  613 | `		pBackend->pMethods->xFree(pBlock);` |
|    722342 |  614 | `		pBlock = pNext;` |
|    722342 |  615 | `		pBackend->nBlock--;` |
|         - |  616 | `		/* LOOP THREE */` |
|    722342 |  617 | `		if( pBackend->nBlock == 0 ){` |
|       293 |  618 | `			break;` |
|         - |  619 | `		}` |
|    722050 |  620 | `		pNext  = pBlock->pNext;` |
|    722050 |  621 | `		pBackend->pMethods->xFree(pBlock);` |
|    722050 |  622 | `		pBlock = pNext;` |
|    722050 |  623 | `		pBackend->nBlock--;` |
|         - |  624 | `		/* LOOP FOUR */` |
|         2 |  625 | `	}` |
|      5400 |  626 | `	if( pBackend->pMethods->xRelease ){` |
|       ! 0 |  627 | `		pBackend->pMethods->xRelease(pBackend->pMethods->pUserData);` |
|       ! 0 |  628 | `	}` |
|      5400 |  629 | `	pBackend->pMethods = 0;` |
|      5400 |  630 | `	pBackend->pBlocks  = 0;` |
|         - |  631 | `#if defined(UNTRUST)` |
|         - |  632 | `	pBackend->nMagic = 0x2626;` |
|         - |  633 | `#endif` |
|      5400 |  634 | `	return SXRET_OK;` |
|         2 |  635 |  |
|      5398 |  636 | `PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend *pBackend)` |
|         2 |  637 |  |
|         - |  638 | `#if defined(UNTRUST)` |
|         - |  639 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  640 | `		return SXERR_INVALID;` |
|         - |  641 | `	}` |
|         - |  642 | `#endif` |
|      5400 |  643 | `	if( pBackend->pMutexMethods ){` |
|       261 |  644 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       130 |  645 | `	}` |
|      5400 |  646 | `	(void)MemBackendRelease(&(*pBackend));` |
|      5400 |  647 | `	if( pBackend->pMutexMethods ){` |
|       261 |  648 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       261 |  649 | `		SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|       130 |  650 | `	}` |
|      5400 |  651 | `	return SXRET_OK;` |
|         2 |  652 |  |
|    538134 |  653 | `PH7_PRIVATE void * SyMemBackendDup(SyMemBackend *pBackend,const void *pSrc,sxu32 nSize)` |
|         2 |  654 |  |
|         - |  655 | `	void *pNew;` |
|         - |  656 | `#if defined(UNTRUST)` |
|         - |  657 | `	if( pSrc == 0 \|\| nSize <= 0 ){` |
|         - |  658 | `		return 0;` |
|         - |  659 | `	}` |
|         - |  660 | `#endif` |
|    538136 |  661 | `	pNew = SyMemBackendAlloc(&(*pBackend),nSize);` |
|    538136 |  662 | `	if( pNew ){` |
|    538136 |  663 | `		SyMemcpy(pSrc,pNew,nSize);` |
|    269067 |  664 | `	}` |
|    538136 |  665 | `	return pNew;` |
|         2 |  666 |  |
|   1918520 |  667 | `PH7_PRIVATE char * SyMemBackendStrDup(SyMemBackend *pBackend,const char *zSrc,sxu32 nSize)` |
|         2 |  668 |  |
|         - |  669 | `	char *zDest;` |
|   1918522 |  670 | `	zDest = (char *)SyMemBackendAlloc(&(*pBackend),nSize + 1);` |
|   1918522 |  671 | `	if( zDest ){` |
|   1918522 |  672 | `		Systrcpy(zDest,nSize+1,zSrc,nSize);` |
|    959260 |  673 | `	}` |
|   1918522 |  674 | `	return zDest;` |
|         2 |  675 |  |
|    124444 |  676 | `PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize)` |
|         2 |  677 |  |
|         - |  678 | `#if defined(UNTRUST)` |
|         - |  679 | `	if( pBlob == 0 \|\| pBuffer == 0 \|\| nSize < 1 ){` |
|         - |  680 | `		return SXERR_EMPTY;` |
|         - |  681 | `	}` |
|         - |  682 | `#endif` |
|    124446 |  683 | `	pBlob->pBlob = pBuffer;` |
|    124446 |  684 | `	pBlob->mByte = nSize;` |
|    124446 |  685 | `	pBlob->nByte = 0;` |
|    124446 |  686 | `	pBlob->pAllocator = 0;` |
|    124446 |  687 | `	pBlob->nFlags = SXBLOB_LOCKED\|SXBLOB_STATIC;` |
|    124446 |  688 | `	return SXRET_OK;` |
|         2 |  689 |  |
|   6655426 |  690 | `PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator)` |
|         2 |  691 |  |
|         - |  692 | `#if defined(UNTRUST)` |
|         - |  693 | `	if( pBlob == 0  ){` |
|         - |  694 | `		return SXERR_EMPTY;` |
|         - |  695 | `	}` |
|         - |  696 | `#endif` |
|   6655428 |  697 | `	pBlob->pBlob = 0;` |
|   6655428 |  698 | `	pBlob->mByte = pBlob->nByte	= 0;` |
|   6655428 |  699 | `	pBlob->pAllocator = &(*pAllocator);` |
|   6655428 |  700 | `	pBlob->nFlags = 0;` |
|   6655428 |  701 | `	return SXRET_OK;` |
|         2 |  702 |  |
|   2562954 |  703 | `PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte)` |
|         2 |  704 |  |
|         - |  705 | `#if defined(UNTRUST)` |
|         - |  706 | `	if( pBlob == 0  ){` |
|         - |  707 | `		return SXERR_EMPTY;` |
|         - |  708 | `	}` |
|         - |  709 | `#endif` |
|   2562956 |  710 | `	pBlob->pBlob = (void *)pData;` |
|   2562956 |  711 | `	pBlob->nByte = nByte;` |
|   2562956 |  712 | `	pBlob->mByte = 0;` |
|   2562956 |  713 | `	pBlob->nFlags \|= SXBLOB_RDONLY;` |
|   2562956 |  714 | `	return SXRET_OK;` |
|         2 |  715 |  |
|         - |  716 | `#ifndef SXBLOB_MIN_GROWTH` |
|         - |  717 | `#define SXBLOB_MIN_GROWTH 16` |
|         - |  718 | `#endif` |
|   6934840 |  719 | `static sxi32 BlobPrepareGrow(SyBlob *pBlob,sxu32 *pByte)` |
|         2 |  720 |  |
|         - |  721 | `	sxu32 nByte;` |
|         - |  722 | `	void *pNew;` |
|   6934842 |  723 | `	nByte = *pByte;` |
|   6934842 |  724 | `	if( pBlob->nFlags & (SXBLOB_LOCKED\|SXBLOB_STATIC) ){` |
|    994180 |  725 | `		if ( SyBlobFreeSpace(pBlob) < nByte ){` |
|       ! 0 |  726 | `			*pByte = SyBlobFreeSpace(pBlob);` |
|       ! 0 |  727 | `			if( (*pByte) == 0 ){` |
|       ! 0 |  728 | `				return SXERR_SHORT;` |
|         - |  729 | `			}` |
|       ! 0 |  730 | `		}` |
|    994180 |  731 | `		return SXRET_OK;` |
|         - |  732 | `	}` |
|   5940664 |  733 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|         - |  734 | `		/* Make a copy of the read-only item */` |
|    538136 |  735 | `		if( pBlob->nByte > 0 ){` |
|    538136 |  736 | `			pNew = SyMemBackendDup(pBlob->pAllocator,pBlob->pBlob,pBlob->nByte);` |
|    538136 |  737 | `			if( pNew == 0 ){` |
|       ! 0 |  738 | `				return SXERR_MEM;` |
|         - |  739 | `			}` |
|    538136 |  740 | `			pBlob->pBlob = pNew;` |
|    538136 |  741 | `			pBlob->mByte = pBlob->nByte;` |
|    269069 |  742 | `		}else{` |
|       ! 0 |  743 | `			pBlob->pBlob = 0;` |
|       ! 0 |  744 | `			pBlob->mByte = 0;` |
|         - |  745 | `		}` |
|         - |  746 | `		/* Remove the read-only flag */` |
|    538136 |  747 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|    269067 |  748 | `	}` |
|   5940664 |  749 | `	if( SyBlobFreeSpace(pBlob) >= nByte ){` |
|   1169433 |  750 | `		return SXRET_OK;` |
|         - |  751 | `	}` |
|   4771233 |  752 | `	if( pBlob->mByte > 0 ){` |
|    627073 |  753 | `		nByte = nByte + pBlob->mByte * 2 + SXBLOB_MIN_GROWTH;` |
|   4457690 |  754 | `	}else if ( nByte < SXBLOB_MIN_GROWTH ){` |
|   3354590 |  755 | `		nByte = SXBLOB_MIN_GROWTH;` |
|   1677193 |  756 | `	}` |
|   4771233 |  757 | `	pNew = SyMemBackendRealloc(pBlob->pAllocator,pBlob->pBlob,nByte);` |
|   4771233 |  758 | `	if( pNew == 0 ){` |
|       ! 0 |  759 | `		return SXERR_MEM;` |
|         - |  760 | `	}` |
|   4771233 |  761 | `	pBlob->pBlob = pNew;` |
|   4771233 |  762 | `	pBlob->mByte = nByte;` |
|   4771233 |  763 | `	return SXRET_OK;` |
|   3467466 |  764 |  |
|   6981086 |  765 | `PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize)` |
|         2 |  766 |  |
|         - |  767 | `	sxu8 *zBlob;` |
|         - |  768 | `	sxi32 rc;` |
|   6981088 |  769 | `	if( nSize < 1 ){` |
|     46248 |  770 | `		return SXRET_OK;` |
|         - |  771 | `	}` |
|   6934842 |  772 | `	rc = BlobPrepareGrow(&(*pBlob),&nSize);` |
|   6934842 |  773 | `	if( SXRET_OK != rc ){` |
|       ! 0 |  774 | `		return rc;` |
|         - |  775 | `	}` |
|   6934842 |  776 | `	if( pData ){` |
|   6934810 |  777 | `		zBlob = (sxu8 *)pBlob->pBlob ;` |
|   6934810 |  778 | `		zBlob = &zBlob[pBlob->nByte];` |
|   6934810 |  779 | `		pBlob->nByte += nSize;` |
|  31100715 |  780 | `		SX_MACRO_FAST_MEMCPY(pData,zBlob,nSize);` |
|   3467448 |  781 | `	}` |
|   6934842 |  782 | `	return SXRET_OK;` |
|   3490589 |  783 |  |
|    538202 |  784 | `PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob)` |
|         2 |  785 |  |
|         - |  786 | `	sxi32 rc;` |
|         - |  787 | `	sxu32 n;` |
|    538204 |  788 | `	n = pBlob->nByte;` |
|    538204 |  789 | `	rc = SyBlobAppend(&(*pBlob),(const void *)"\0",sizeof(char));` |
|    538204 |  790 | `	if (rc == SXRET_OK ){` |
|    538204 |  791 | `		pBlob->nByte = n;` |
|    269123 |  792 | `	}` |
|    538204 |  793 | `	return rc;` |
|         2 |  794 |  |
|   3475052 |  795 | `PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest)` |
|         2 |  796 |  |
|   3475054 |  797 | `	sxi32 rc = SXRET_OK;` |
|         - |  798 | `#ifdef UNTRUST` |
|         - |  799 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|         - |  800 | `		return SXERR_EMPTY;` |
|         - |  801 | `	}` |
|         - |  802 | `#endif` |
|   3475054 |  803 | `	if( pSrc->nByte > 0 ){` |
|   3453834 |  804 | `		rc = SyBlobAppend(&(*pDest),pSrc->pBlob,pSrc->nByte);` |
|   1726916 |  805 | `	}` |
|   3475054 |  806 | `	return rc;` |
|         2 |  807 |  |
|         8 |  808 | `PH7_PRIVATE sxi32 SyBlobCmp(SyBlob *pLeft,SyBlob *pRight)` |
|         1 |  809 |  |
|         - |  810 | `	sxi32 rc;` |
|         - |  811 | `#ifdef UNTRUST` |
|         - |  812 | `	if( pLeft == 0 \|\| pRight == 0 ){` |
|         - |  813 | `		return pLeft ? 1 : -1;` |
|         - |  814 | `	}` |
|         - |  815 | `#endif` |
|         9 |  816 | `	if( pLeft->nByte != pRight->nByte ){` |
|         - |  817 | `		/* Length differ */` |
|       ! 0 |  818 | `		return pLeft->nByte - pRight->nByte;` |
|         - |  819 | `	}` |
|         9 |  820 | `	if( pLeft->nByte == 0 ){` |
|       ! 0 |  821 | `		return 0;` |
|         - |  822 | `	}` |
|         - |  823 | `	/* Perform a standard memcmp() operation */` |
|         9 |  824 | `	rc = SyMemcmp(pLeft->pBlob,pRight->pBlob,pLeft->nByte);` |
|         9 |  825 | `	return rc;` |
|         5 |  826 |  |
|   3664644 |  827 | `PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob)` |
|         2 |  828 |  |
|   3664646 |  829 | `	pBlob->nByte = 0;` |
|   3664646 |  830 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|      7776 |  831 | `		pBlob->pBlob = 0;` |
|      7776 |  832 | `		pBlob->mByte = 0;` |
|      7776 |  833 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|      3887 |  834 | `	}` |
|   3664646 |  835 | `	return SXRET_OK;` |
|         2 |  836 |  |
|   9867536 |  837 | `PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob)` |
|         2 |  838 |  |
|   9867538 |  839 | `	if( (pBlob->nFlags & (SXBLOB_STATIC\|SXBLOB_RDONLY)) == 0 && pBlob->mByte > 0 ){` |
|   4361322 |  840 | `		SyMemBackendFree(pBlob->pAllocator,pBlob->pBlob);` |
|   2180682 |  841 | `	}` |
|   9867538 |  842 | `	pBlob->pBlob = 0;` |
|   9867538 |  843 | `	pBlob->nByte = pBlob->mByte = 0;` |
|   9867538 |  844 | `	pBlob->nFlags = 0;` |
|   9867538 |  845 | `	return SXRET_OK;` |
|         2 |  846 |  |
|         - |  847 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    121180 |  848 | `PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft)` |
|         2 |  849 |  |
|    121182 |  850 | `	const char *zIn = (const char *)pBlob;` |
|         - |  851 | `	const char *zEnd;` |
|         - |  852 | `	sxi32 rc;` |
|    121182 |  853 | `	if( pLen > nLen ){` |
|      4632 |  854 | `		return SXERR_NOTFOUND;` |
|         - |  855 | `	}` |
|    116552 |  856 | `	zEnd = &zIn[nLen-pLen];` |
|    971426 |  857 | `	for(;;){` |
|   1942791 |  858 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   1913254 |  859 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   1872081 |  860 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   1846345 |  861 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|         2 |  862 | `	}` |
|     17930 |  863 | `	return SXERR_NOTFOUND;` |
|     60592 |  864 |  |
|         - |  865 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - |  866 |  |
