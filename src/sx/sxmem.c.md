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
|  11818904 |   18 | `static void * SyOSHeapAlloc(sxu32 nByte)` |
|         2 |   19 |  |
|         - |   20 | `	void *pNew;` |
|         - |   21 | `#if defined(__WINNT__)` |
|         2 |   22 | `	pNew = HeapAlloc(GetProcessHeap(),0,nByte);` |
|         - |   23 | `#else` |
|  11818904 |   24 | `	pNew = malloc((size_t)nByte);` |
|         - |   25 | `#endif` |
|  11818906 |   26 | `	return pNew;` |
|         2 |   27 |  |
|    740682 |   28 | `static void * SyOSHeapRealloc(void *pOld,sxu32 nByte)` |
|         2 |   29 |  |
|         - |   30 | `	void *pNew;` |
|         - |   31 | `#if defined(__WINNT__)` |
|         2 |   32 | `	pNew = HeapReAlloc(GetProcessHeap(),0,pOld,nByte);` |
|         - |   33 | `#else` |
|    740682 |   34 | `	pNew = realloc(pOld,(size_t)nByte);` |
|         - |   35 | `#endif` |
|    740684 |   36 | `	return pNew;` |
|         2 |   37 |  |
|  11807094 |   38 | `static void SyOSHeapFree(void *pPtr)` |
|         2 |   39 |  |
|         - |   40 | `#if defined(__WINNT__)` |
|         2 |   41 | `	HeapFree(GetProcessHeap(),0,pPtr);` |
|         - |   42 | `#else` |
|  11807094 |   43 | `	free(pPtr);` |
|         - |   44 | `#endif` |
|  11807096 |   45 |  |
|         - |   46 |  |
|         - |   47 |  |
|  20644806 |   48 | `PH7_PRIVATE void SyZero(void *pSrc,sxu32 nSize)` |
|         2 |   49 |  |
|  20644808 |   50 | `	register unsigned char *zSrc = (unsigned char *)pSrc;` |
|         - |   51 | `	unsigned char *zEnd;` |
|         - |   52 | `#if defined(UNTRUST)` |
|         - |   53 | `	if( zSrc == 0 \|\| nSize <= 0 ){` |
|         - |   54 | `		return ;` |
|         - |   55 | `	}` |
|         - |   56 | `#endif` |
|  20644808 |   57 | `	zEnd = &zSrc[nSize];` |
| 272137214 |   58 | `	for(;;){` |
| 544272150 |   59 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 523627348 |   60 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 523627346 |   61 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 523627346 |   62 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
|         2 |   63 | `	}` |
|  20644808 |   64 |  |
|  18559346 |   65 | `PH7_PRIVATE sxi32 SyMemcmp(const void *pB1,const void *pB2,sxu32 nSize)` |
|         2 |   66 |  |
|         - |   67 | `	sxi32 rc;` |
|  18559348 |   68 | `	if( nSize <= 0 ){` |
|        86 |   69 | `		return 0;` |
|         - |   70 | `	}` |
|  18559264 |   71 | `	if( pB1 == 0 \|\| pB2 == 0 ){` |
|       ! 0 |   72 | `		return pB1 != 0 ? 1 : (pB2 == 0 ? 0 : -1);` |
|         - |   73 | `	}` |
|  37687215 |   74 | `	SX_MACRO_FAST_CMP(pB1,pB2,nSize,rc);` |
|  18559264 |   75 | `	return rc;` |
|   9280247 |   76 |  |
|   9492214 |   77 | `PH7_PRIVATE sxu32 SyMemcpy(const void *pSrc,void *pDest,sxu32 nLen)` |
|         2 |   78 |  |
|         - |   79 | `#if defined(UNTRUST)` |
|         - |   80 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|         - |   81 | `		return 0;` |
|         - |   82 | `	}` |
|         - |   83 | `#endif` |
|   9492216 |   84 | `	if( pSrc == (const void *)pDest ){` |
|       ! 0 |   85 | `		return nLen;` |
|         - |   86 | `	}` |
|  77568666 |   87 | `	SX_MACRO_FAST_MEMCPY(pSrc,pDest,nLen);` |
|   9492216 |   88 | `	return nLen;` |
|   4746301 |   89 |  |
|  11818904 |   90 | `static void * MemOSAlloc(sxu32 nBytes)` |
|         2 |   91 |  |
|         - |   92 | `	sxu32 *pChunk;` |
|  11818906 |   93 | `	pChunk = (sxu32 *)SyOSHeapAlloc(nBytes + sizeof(sxu32));` |
|  11818906 |   94 | `	if( pChunk == 0 ){` |
|       ! 0 |   95 | `		return 0;` |
|         - |   96 | `	}` |
|  11818906 |   97 | `	pChunk[0] = nBytes;` |
|  11818906 |   98 | `	return (void *)&pChunk[1];` |
|   5909476 |   99 |  |
|    740682 |  100 | `static void * MemOSRealloc(void *pOld,sxu32 nBytes)` |
|         2 |  101 |  |
|         - |  102 | `	sxu32 *pOldChunk;` |
|         - |  103 | `	sxu32 *pChunk;` |
|    740684 |  104 | `	pOldChunk = (sxu32 *)(((char *)pOld)-sizeof(sxu32));` |
|    740684 |  105 | `	if( pOldChunk[0] >= nBytes ){` |
|       ! 0 |  106 | `		return pOld;` |
|         - |  107 | `	}` |
|    740684 |  108 | `	pChunk = (sxu32 *)SyOSHeapRealloc(pOldChunk,nBytes + sizeof(sxu32));` |
|    740684 |  109 | `	if( pChunk == 0 ){` |
|       ! 0 |  110 | `		return 0;` |
|         - |  111 | `	}` |
|    740684 |  112 | `	pChunk[0] = nBytes;` |
|    740684 |  113 | `	return (void *)&pChunk[1];` |
|    370336 |  114 |  |
|  11807094 |  115 | `static void MemOSFree(void *pBlock)` |
|         2 |  116 |  |
|         - |  117 | `	void *pChunk;` |
|  11807096 |  118 | `	pChunk = (void *)(((char *)pBlock)-sizeof(sxu32));` |
|  11807096 |  119 | `	SyOSHeapFree(pChunk);` |
|  11807096 |  120 |  |
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
|  11818904 |  137 | `static void * MemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         2 |  138 |  |
|         - |  139 | `	SyMemBlock *pBlock;` |
|  11818906 |  140 | `	sxi32 nRetry = 0;` |
|         - |  141 |  |
|         - |  142 | `	/* Append an extra block so we can tracks allocated chunks and avoid memory` |
|         - |  143 | `	 * leaks.` |
|         - |  144 | `	 */` |
|  11818906 |  145 | `	nByte += sizeof(SyMemBlock);` |
|   5909474 |  146 | `	for(;;){` |
|   5909476 |  147 | `		pBlock = (SyMemBlock *)pBackend->pMethods->xAlloc(nByte);` |
|  11818904 |  148 | `		if( pBlock != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY` |
|         2 |  149 | `			\|\| SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|   5909476 |  150 | `				break;` |
|         - |  151 | `		}` |
|       ! 0 |  152 | `		nRetry++;` |
|       ! 0 |  153 | `	}` |
|  11818906 |  154 | `	if( pBlock  == 0 ){` |
|       ! 0 |  155 | `		return 0;` |
|         - |  156 | `	}` |
|  11818906 |  157 | `	pBlock->pNext = pBlock->pPrev = 0;` |
|         - |  158 | `	/* Link to the list of already tracked blocks */` |
|  11818906 |  159 | `	MACRO_LD_PUSH(pBackend->pBlocks,pBlock);` |
|         - |  160 | `#if defined(UNTRUST)` |
|         - |  161 | `	pBlock->nGuard = SXMEM_BACKEND_MAGIC;` |
|         - |  162 | `#endif` |
|  11818906 |  163 | `	pBackend->nBlock++;` |
|  11818906 |  164 | `	return (void *)&pBlock[1];` |
|   5909476 |  165 |  |
|   3823920 |  166 | `PH7_PRIVATE void * SyMemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         2 |  167 |  |
|         - |  168 | `	void *pChunk;` |
|         - |  169 | `#if defined(UNTRUST)` |
|         - |  170 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  171 | `		return 0;` |
|         - |  172 | `	}` |
|         - |  173 | `#endif` |
|   3823922 |  174 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  175 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  176 | `	}` |
|   3823922 |  177 | `	pChunk = MemBackendAlloc(&(*pBackend),nByte);` |
|   3823922 |  178 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  179 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  180 | `	}` |
|   3823922 |  181 | `	return pChunk;` |
|         2 |  182 |  |
|   8685436 |  183 | `static void * MemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         2 |  184 |  |
|         - |  185 | `	SyMemBlock *pBlock,*pNew,*pPrev,*pNext;` |
|   8685438 |  186 | `	sxu32 nRetry = 0;` |
|         - |  187 |  |
|   8685438 |  188 | `	if( pOld == 0 ){` |
|   7944756 |  189 | `		return MemBackendAlloc(&(*pBackend),nByte);` |
|         - |  190 | `	}` |
|    740684 |  191 | `	pBlock = (SyMemBlock *)(((char *)pOld) - sizeof(SyMemBlock));` |
|         - |  192 | `#if defined(UNTRUST)` |
|         - |  193 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|         - |  194 | `		return 0;` |
|         - |  195 | `	}` |
|         - |  196 | `#endif` |
|    740684 |  197 | `	nByte += sizeof(SyMemBlock);` |
|    740684 |  198 | `	pPrev = pBlock->pPrev;` |
|    740684 |  199 | `	pNext = pBlock->pNext;` |
|    370334 |  200 | `	for(;;){` |
|    370336 |  201 | `		pNew = (SyMemBlock *)pBackend->pMethods->xRealloc(pBlock,nByte);` |
|    740684 |  202 | `		if( pNew != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY \|\|` |
|       ! 0 |  203 | `			SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|    370336 |  204 | `				break;` |
|         - |  205 | `		}` |
|       ! 0 |  206 | `		nRetry++;` |
|       ! 0 |  207 | `	}` |
|    740684 |  208 | `	if( pNew == 0 ){` |
|       ! 0 |  209 | `		return 0;` |
|         - |  210 | `	}` |
|    740684 |  211 | `	if( pNew != pBlock ){` |
|    661428 |  212 | `		if( pPrev == 0 ){` |
|    518158 |  213 | `			pBackend->pBlocks = pNew;` |
|    282545 |  214 | `		}else{` |
|    143272 |  215 | `			pPrev->pNext = pNew;` |
|         - |  216 | `		}` |
|    661428 |  217 | `		if( pNext ){` |
|    661417 |  218 | `			pNext->pPrev = pNew;` |
|    353454 |  219 | `		}` |
|         - |  220 | `#if defined(UNTRUST)` |
|         - |  221 | `		pNew->nGuard = SXMEM_BACKEND_MAGIC;` |
|         - |  222 | `#endif` |
|    353460 |  223 | `	}` |
|    740684 |  224 | `	return (void *)&pNew[1];` |
|   4342735 |  225 |  |
|   8685436 |  226 | `PH7_PRIVATE void * SyMemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         2 |  227 |  |
|         - |  228 | `	void *pChunk;` |
|         - |  229 | `#if defined(UNTRUST)` |
|         - |  230 | `	if( SXMEM_BACKEND_CORRUPT(pBackend)  ){` |
|         - |  231 | `		return 0;` |
|         - |  232 | `	}` |
|         - |  233 | `#endif` |
|   8685438 |  234 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  235 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  236 | `	}` |
|   8685438 |  237 | `	pChunk = MemBackendRealloc(&(*pBackend),pOld,nByte);` |
|   8685438 |  238 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  239 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  240 | `	}` |
|   8685438 |  241 | `	return pChunk;` |
|         2 |  242 |  |
|   8846088 |  243 | `static sxi32 MemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|         2 |  244 |  |
|         - |  245 | `	SyMemBlock *pBlock;` |
|   8846090 |  246 | `	pBlock = (SyMemBlock *)(((char *)pChunk) - sizeof(SyMemBlock));` |
|         - |  247 | `#if defined(UNTRUST)` |
|         - |  248 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|         - |  249 | `		return SXERR_CORRUPT;` |
|         - |  250 | `	}` |
|         - |  251 | `#endif` |
|         - |  252 | `	/* Unlink from the list of active blocks */` |
|   8846090 |  253 | `	if( pBackend->nBlock > 0 ){` |
|         - |  254 | `		/* Release the block */` |
|         - |  255 | `#if defined(UNTRUST)` |
|         - |  256 | `		/* Mark as stale block */` |
|         - |  257 | `		pBlock->nGuard = 0x635B;` |
|         - |  258 | `#endif` |
|   8846090 |  259 | `		MACRO_LD_REMOVE(pBackend->pBlocks,pBlock);` |
|   8846090 |  260 | `		pBackend->nBlock--;` |
|   8846090 |  261 | `		pBackend->pMethods->xFree(pBlock);` |
|   4423066 |  262 | `	}` |
|   8846090 |  263 | `	return SXRET_OK;` |
|         2 |  264 |  |
|   8846088 |  265 | `PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|         2 |  266 |  |
|         - |  267 | `	sxi32 rc;` |
|         - |  268 | `#if defined(UNTRUST)` |
|         - |  269 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  270 | `		return SXERR_CORRUPT;` |
|         - |  271 | `	}` |
|         - |  272 | `#endif` |
|   8846090 |  273 | `	if( pChunk == 0 ){` |
|       ! 0 |  274 | `		return SXRET_OK;` |
|         - |  275 | `	}` |
|   8846090 |  276 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  277 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  278 | `	}` |
|   8846090 |  279 | `	rc = MemBackendFree(&(*pBackend),pChunk);` |
|   8846090 |  280 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  281 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  282 | `	}` |
|   8846090 |  283 | `	return rc;` |
|   4423068 |  284 |  |
|         - |  285 | `#if defined(PH7_ENABLE_THREADS)` |
|      2604 |  286 | `PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods)` |
|         2 |  287 |  |
|         - |  288 | `	SyMutex *pMutex;` |
|         - |  289 | `#if defined(UNTRUST)` |
|         - |  290 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pMethods == 0 \|\| pMethods->xNew == 0){` |
|         - |  291 | `		return SXERR_CORRUPT;` |
|         - |  292 | `	}` |
|         - |  293 | `#endif` |
|      2606 |  294 | `	pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|      2606 |  295 | `	if( pMutex == 0 ){` |
|       ! 0 |  296 | `		return SXERR_OS;` |
|         - |  297 | `	}` |
|         - |  298 | `	/* Attach the mutex to the memory backend */` |
|      2606 |  299 | `	pBackend->pMutex = pMutex;` |
|      2606 |  300 | `	pBackend->pMutexMethods = pMethods;` |
|      2606 |  301 | `	return SXRET_OK;` |
|      1304 |  302 |  |
|      2604 |  303 | `PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend)` |
|         2 |  304 |  |
|         - |  305 | `#if defined(UNTRUST)` |
|         - |  306 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  307 | `		return SXERR_CORRUPT;` |
|         - |  308 | `	}` |
|         - |  309 | `#endif` |
|      2606 |  310 | `	if( pBackend->pMutex == 0 ){` |
|         - |  311 | `		/* There is no mutex subsystem at all */` |
|       ! 0 |  312 | `		return SXRET_OK;` |
|         - |  313 | `	}` |
|      2606 |  314 | `	SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|      2606 |  315 | `	pBackend->pMutexMethods = 0;` |
|      2606 |  316 | `	pBackend->pMutex = 0;` |
|      2606 |  317 | `	return SXRET_OK;` |
|      1304 |  318 |  |
|         - |  319 | `#endif` |
|         - |  320 | `/*` |
|         - |  321 | ` * Memory pool allocator` |
|         - |  322 | ` */` |
|         - |  323 | `#define SXMEM_POOL_MAGIC		0xDEAD` |
|         - |  324 | `#define SXMEM_POOL_MAXALLOC		(1<<(SXMEM_POOL_NBUCKETS+SXMEM_POOL_INCR))` |
|         - |  325 | `#define SXMEM_POOL_MINALLOC		(1<<(SXMEM_POOL_INCR))` |
|     50230 |  326 | `static sxi32 MemPoolBucketAlloc(SyMemBackend *pBackend,sxu32 nBucket)` |
|         2 |  327 |  |
|         - |  328 | `	char *zBucket,*zBucketEnd;` |
|         - |  329 | `	SyMemHeader *pHeader;` |
|         - |  330 | `	sxu32 nBucketSize;` |
|         - |  331 |  |
|         - |  332 | `	/* Allocate one big block first */` |
|     50232 |  333 | `	zBucket = (char *)MemBackendAlloc(&(*pBackend),SXMEM_POOL_MAXALLOC);` |
|     50232 |  334 | `	if( zBucket == 0 ){` |
|       ! 0 |  335 | `		return SXERR_MEM;` |
|         - |  336 | `	}` |
|     50232 |  337 | `	zBucketEnd = &zBucket[SXMEM_POOL_MAXALLOC];` |
|         - |  338 | `	/* Divide the big block into mini bucket pool */` |
|     50232 |  339 | `	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);` |
|     50232 |  340 | `	pBackend->apPool[nBucket] = pHeader = (SyMemHeader *)zBucket;` |
|   5705568 |  341 | `	for(;;){` |
|  11411138 |  342 | `		if( &zBucket[nBucketSize] >= zBucketEnd ){` |
|     50232 |  343 | `			break;` |
|         - |  344 | `		}` |
|  11360908 |  345 | `		pHeader->pNext = (SyMemHeader *)&zBucket[nBucketSize];` |
|         - |  346 | `		/* Advance the cursor to the next available chunk */` |
|  11360908 |  347 | `		pHeader = pHeader->pNext;` |
|  11360908 |  348 | `		zBucket += nBucketSize;` |
|         2 |  349 | `	}` |
|     50232 |  350 | `	pHeader->pNext = 0;` |
|         - |  351 |  |
|     50232 |  352 | `	return SXRET_OK;` |
|     25117 |  353 |  |
|  14207420 |  354 | `static void * MemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         2 |  355 |  |
|         - |  356 | `	SyMemHeader *pBucket,*pNext;` |
|         - |  357 | `	sxu32 nBucketSize;` |
|         - |  358 | `	sxu32 nBucket;` |
|         - |  359 |  |
|  14207422 |  360 | `	if( nByte + sizeof(SyMemHeader) >= SXMEM_POOL_MAXALLOC ){` |
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
|  14207422 |  371 | `	nBucket = 0;` |
|  14207422 |  372 | `	nBucketSize = SXMEM_POOL_MINALLOC;` |
|  70910336 |  373 | `	while( nByte + sizeof(SyMemHeader) > nBucketSize  ){` |
|  56702916 |  374 | `		nBucketSize <<= 1;` |
|  56702916 |  375 | `		nBucket++;` |
|         2 |  376 | `	}` |
|  14207422 |  377 | `	pBucket = pBackend->apPool[nBucket];` |
|  14207422 |  378 | `	if( pBucket == 0 ){` |
|         - |  379 | `		sxi32 rc;` |
|     50232 |  380 | `		rc = MemPoolBucketAlloc(&(*pBackend),nBucket);` |
|     50232 |  381 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  382 | `			return 0;` |
|         - |  383 | `		}` |
|     50232 |  384 | `		pBucket = pBackend->apPool[nBucket];` |
|     25115 |  385 | `	}` |
|         - |  386 | `	/* Remove from the free list */` |
|  14207422 |  387 | `	pNext = pBucket->pNext;` |
|  14207422 |  388 | `	pBackend->apPool[nBucket] = pNext;` |
|         - |  389 | `	/* Record bucket&magic number */` |
|  14207422 |  390 | `	pBucket->nBucket = (SXMEM_POOL_MAGIC << 16) \| nBucket;` |
|  14207422 |  391 | `	return (void *)&pBucket[1];` |
|   7103712 |  392 |  |
|  14207420 |  393 | `PH7_PRIVATE void * SyMemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         2 |  394 |  |
|         - |  395 | `	void *pChunk;` |
|         - |  396 | `#if defined(UNTRUST)` |
|         - |  397 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  398 | `		return 0;` |
|         - |  399 | `	}` |
|         - |  400 | `#endif` |
|  14207422 |  401 | `	if( pBackend->pMutexMethods ){` |
|      2606 |  402 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1302 |  403 | `	}` |
|  14207422 |  404 | `	pChunk = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|  14207422 |  405 | `	if( pBackend->pMutexMethods ){` |
|      2606 |  406 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1302 |  407 | `	}` |
|  14207422 |  408 | `	return pChunk;` |
|         2 |  409 |  |
|   9129764 |  410 | `static sxi32 MemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|         2 |  411 |  |
|         - |  412 | `	SyMemHeader *pHeader;` |
|         - |  413 | `	sxu32 nBucket;` |
|         - |  414 | `	/* Get the corresponding bucket */` |
|   9129766 |  415 | `	pHeader = (SyMemHeader *)(((char *)pChunk) - sizeof(SyMemHeader));` |
|         - |  416 | `	/* Sanity check to avoid misuse */` |
|   9129766 |  417 | `	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){` |
|       ! 0 |  418 | `		return SXERR_CORRUPT;` |
|         - |  419 | `	}` |
|   9129766 |  420 | `	nBucket = pHeader->nBucket & 0xFFFF;` |
|   9129766 |  421 | `	if( nBucket == SXU16_HIGH ){` |
|         - |  422 | `		/* Free the big block */` |
|       ! 0 |  423 | `		MemBackendFree(&(*pBackend),pHeader);` |
|   9129766 |  424 | `	}else if( nBucket >= SXMEM_POOL_NBUCKETS + SXMEM_POOL_INCR ){` |
|         - |  425 | `		/* Corrupted or misused bucket index */` |
|       ! 0 |  426 | `		return SXERR_CORRUPT;` |
|       ! 0 |  427 | `	}else{` |
|         - |  428 | `		/* Return to the free list */` |
|   9129766 |  429 | `		pHeader->pNext = pBackend->apPool[nBucket];` |
|   9129766 |  430 | `		pBackend->apPool[nBucket] = pHeader;` |
|         - |  431 | `	}` |
|   9129766 |  432 | `	return SXRET_OK;` |
|   4564884 |  433 |  |
|   9129764 |  434 | `PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|         2 |  435 |  |
|         - |  436 | `	sxi32 rc;` |
|         - |  437 | `#if defined(UNTRUST)` |
|         - |  438 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pChunk == 0 ){` |
|         - |  439 | `		return SXERR_CORRUPT;` |
|         - |  440 | `	}` |
|         - |  441 | `#endif` |
|   9129766 |  442 | `	if( pBackend->pMutexMethods ){` |
|      2336 |  443 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1167 |  444 | `	}` |
|   9129766 |  445 | `	rc = MemBackendPoolFree(&(*pBackend),pChunk);` |
|   9129766 |  446 | `	if( pBackend->pMutexMethods ){` |
|      2336 |  447 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1167 |  448 | `	}` |
|   9129766 |  449 | `	return rc;` |
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
|      2604 |  508 | `PH7_PRIVATE sxi32 SyMemBackendInit(SyMemBackend *pBackend,ProcMemError xMemErr,void * pUserData)` |
|         2 |  509 |  |
|         - |  510 | `#if defined(UNTRUST)` |
|         - |  511 | `	if( pBackend == 0 ){` |
|         - |  512 | `		return SXERR_EMPTY;` |
|         - |  513 | `	}` |
|         - |  514 | `#endif` |
|         - |  515 | `	/* Zero the allocator first */` |
|      2606 |  516 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|      2606 |  517 | `	pBackend->xMemError = xMemErr;` |
|      2606 |  518 | `	pBackend->pUserData = pUserData;` |
|         - |  519 | `	/* Switch to the OS memory allocator */` |
|      2606 |  520 | `	pBackend->pMethods = &sOSAllocMethods;` |
|      2606 |  521 | `	if( pBackend->pMethods->xInit ){` |
|         - |  522 | `		/* Initialize the backend  */` |
|       ! 0 |  523 | `		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){` |
|       ! 0 |  524 | `			return SXERR_ABORT;` |
|         - |  525 | `		}` |
|       ! 0 |  526 | `	}` |
|         - |  527 | `#if defined(UNTRUST)` |
|         - |  528 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|         - |  529 | `#endif` |
|      2606 |  530 | `	return SXRET_OK;` |
|      1304 |  531 |  |
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
|      5206 |  560 | `PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend *pBackend,SyMemBackend *pParent)` |
|         2 |  561 |  |
|         - |  562 | `	sxu8 bInheritMutex;` |
|         - |  563 | `#if defined(UNTRUST)` |
|         - |  564 | `	if( pBackend == 0 \|\| SXMEM_BACKEND_CORRUPT(pParent) ){` |
|         - |  565 | `		return SXERR_CORRUPT;` |
|         - |  566 | `	}` |
|         - |  567 | `#endif` |
|         - |  568 | `	/* Zero the allocator first */` |
|      5208 |  569 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|      5208 |  570 | `	pBackend->pMethods  = pParent->pMethods;` |
|      5208 |  571 | `	pBackend->xMemError = pParent->xMemError;` |
|      5208 |  572 | `	pBackend->pUserData = pParent->pUserData;` |
|      5208 |  573 | `	bInheritMutex = pParent->pMutexMethods ? TRUE : FALSE;` |
|      5208 |  574 | `	if( bInheritMutex ){` |
|      2606 |  575 | `		pBackend->pMutexMethods = pParent->pMutexMethods;` |
|         - |  576 | `		/* Create a private mutex */` |
|      2606 |  577 | `		pBackend->pMutex = pBackend->pMutexMethods->xNew(SXMUTEX_TYPE_FAST);` |
|      2606 |  578 | `		if( pBackend->pMutex ==  0){` |
|       ! 0 |  579 | `			return SXERR_OS;` |
|         - |  580 | `		}` |
|      1302 |  581 | `	}` |
|         - |  582 | `#if defined(UNTRUST)` |
|         - |  583 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|         - |  584 | `#endif` |
|      5208 |  585 | `	return SXRET_OK;` |
|      2605 |  586 |  |
|      5452 |  587 | `static sxi32 MemBackendRelease(SyMemBackend *pBackend)` |
|         2 |  588 |  |
|         - |  589 | `	SyMemBlock *pBlock,*pNext;` |
|         - |  590 |  |
|      5454 |  591 | `	pBlock = pBackend->pBlocks;` |
|    372077 |  592 | `	for(;;){` |
|    744156 |  593 | `		if( pBackend->nBlock == 0 ){` |
|      1096 |  594 | `			break;` |
|         - |  595 | `		}` |
|    743062 |  596 | `		pNext  = pBlock->pNext;` |
|    743062 |  597 | `		pBackend->pMethods->xFree(pBlock);` |
|    743062 |  598 | `		pBlock = pNext;` |
|    743062 |  599 | `		pBackend->nBlock--;` |
|         - |  600 | `		/* LOOP ONE */` |
|    743062 |  601 | `		if( pBackend->nBlock == 0 ){` |
|      3280 |  602 | `			break;` |
|         - |  603 | `		}` |
|    739784 |  604 | `		pNext  = pBlock->pNext;` |
|    739784 |  605 | `		pBackend->pMethods->xFree(pBlock);` |
|    739784 |  606 | `		pBlock = pNext;` |
|    739784 |  607 | `		pBackend->nBlock--;` |
|         - |  608 | `		/* LOOP TWO */` |
|    739784 |  609 | `		if( pBackend->nBlock == 0 ){` |
|       321 |  610 | `			break;` |
|         - |  611 | `		}` |
|    739464 |  612 | `		pNext  = pBlock->pNext;` |
|    739464 |  613 | `		pBackend->pMethods->xFree(pBlock);` |
|    739464 |  614 | `		pBlock = pNext;` |
|    739464 |  615 | `		pBackend->nBlock--;` |
|         - |  616 | `		/* LOOP THREE */` |
|    739464 |  617 | `		if( pBackend->nBlock == 0 ){` |
|       761 |  618 | `			break;` |
|         - |  619 | `		}` |
|    738704 |  620 | `		pNext  = pBlock->pNext;` |
|    738704 |  621 | `		pBackend->pMethods->xFree(pBlock);` |
|    738704 |  622 | `		pBlock = pNext;` |
|    738704 |  623 | `		pBackend->nBlock--;` |
|         - |  624 | `		/* LOOP FOUR */` |
|         2 |  625 | `	}` |
|      5454 |  626 | `	if( pBackend->pMethods->xRelease ){` |
|       ! 0 |  627 | `		pBackend->pMethods->xRelease(pBackend->pMethods->pUserData);` |
|       ! 0 |  628 | `	}` |
|      5454 |  629 | `	pBackend->pMethods = 0;` |
|      5454 |  630 | `	pBackend->pBlocks  = 0;` |
|         - |  631 | `#if defined(UNTRUST)` |
|         - |  632 | `	pBackend->nMagic = 0x2626;` |
|         - |  633 | `#endif` |
|      5454 |  634 | `	return SXRET_OK;` |
|         2 |  635 |  |
|      5452 |  636 | `PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend *pBackend)` |
|         2 |  637 |  |
|         - |  638 | `#if defined(UNTRUST)` |
|         - |  639 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  640 | `		return SXERR_INVALID;` |
|         - |  641 | `	}` |
|         - |  642 | `#endif` |
|      5454 |  643 | `	if( pBackend->pMutexMethods ){` |
|       263 |  644 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       131 |  645 | `	}` |
|      5454 |  646 | `	(void)MemBackendRelease(&(*pBackend));` |
|      5454 |  647 | `	if( pBackend->pMutexMethods ){` |
|       263 |  648 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       263 |  649 | `		SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|       131 |  650 | `	}` |
|      5454 |  651 | `	return SXRET_OK;` |
|         2 |  652 |  |
|    544422 |  653 | `PH7_PRIVATE void * SyMemBackendDup(SyMemBackend *pBackend,const void *pSrc,sxu32 nSize)` |
|         2 |  654 |  |
|         - |  655 | `	void *pNew;` |
|         - |  656 | `#if defined(UNTRUST)` |
|         - |  657 | `	if( pSrc == 0 \|\| nSize <= 0 ){` |
|         - |  658 | `		return 0;` |
|         - |  659 | `	}` |
|         - |  660 | `#endif` |
|    544424 |  661 | `	pNew = SyMemBackendAlloc(&(*pBackend),nSize);` |
|    544424 |  662 | `	if( pNew ){` |
|    544424 |  663 | `		SyMemcpy(pSrc,pNew,nSize);` |
|    272211 |  664 | `	}` |
|    544424 |  665 | `	return pNew;` |
|         2 |  666 |  |
|   1948278 |  667 | `PH7_PRIVATE char * SyMemBackendStrDup(SyMemBackend *pBackend,const char *zSrc,sxu32 nSize)` |
|         2 |  668 |  |
|         - |  669 | `	char *zDest;` |
|   1948280 |  670 | `	zDest = (char *)SyMemBackendAlloc(&(*pBackend),nSize + 1);` |
|   1948280 |  671 | `	if( zDest ){` |
|   1948280 |  672 | `		Systrcpy(zDest,nSize+1,zSrc,nSize);` |
|    974139 |  673 | `	}` |
|   1948280 |  674 | `	return zDest;` |
|         2 |  675 |  |
|    125696 |  676 | `PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize)` |
|         2 |  677 |  |
|         - |  678 | `#if defined(UNTRUST)` |
|         - |  679 | `	if( pBlob == 0 \|\| pBuffer == 0 \|\| nSize < 1 ){` |
|         - |  680 | `		return SXERR_EMPTY;` |
|         - |  681 | `	}` |
|         - |  682 | `#endif` |
|    125698 |  683 | `	pBlob->pBlob = pBuffer;` |
|    125698 |  684 | `	pBlob->mByte = nSize;` |
|    125698 |  685 | `	pBlob->nByte = 0;` |
|    125698 |  686 | `	pBlob->pAllocator = 0;` |
|    125698 |  687 | `	pBlob->nFlags = SXBLOB_LOCKED\|SXBLOB_STATIC;` |
|    125698 |  688 | `	return SXRET_OK;` |
|         2 |  689 |  |
|   6723810 |  690 | `PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator)` |
|         2 |  691 |  |
|         - |  692 | `#if defined(UNTRUST)` |
|         - |  693 | `	if( pBlob == 0  ){` |
|         - |  694 | `		return SXERR_EMPTY;` |
|         - |  695 | `	}` |
|         - |  696 | `#endif` |
|   6723812 |  697 | `	pBlob->pBlob = 0;` |
|   6723812 |  698 | `	pBlob->mByte = pBlob->nByte	= 0;` |
|   6723812 |  699 | `	pBlob->pAllocator = &(*pAllocator);` |
|   6723812 |  700 | `	pBlob->nFlags = 0;` |
|   6723812 |  701 | `	return SXRET_OK;` |
|         2 |  702 |  |
|   2595686 |  703 | `PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte)` |
|         2 |  704 |  |
|         - |  705 | `#if defined(UNTRUST)` |
|         - |  706 | `	if( pBlob == 0  ){` |
|         - |  707 | `		return SXERR_EMPTY;` |
|         - |  708 | `	}` |
|         - |  709 | `#endif` |
|   2595688 |  710 | `	pBlob->pBlob = (void *)pData;` |
|   2595688 |  711 | `	pBlob->nByte = nByte;` |
|   2595688 |  712 | `	pBlob->mByte = 0;` |
|   2595688 |  713 | `	pBlob->nFlags \|= SXBLOB_RDONLY;` |
|   2595688 |  714 | `	return SXRET_OK;` |
|         2 |  715 |  |
|         - |  716 | `#ifndef SXBLOB_MIN_GROWTH` |
|         - |  717 | `#define SXBLOB_MIN_GROWTH 16` |
|         - |  718 | `#endif` |
|   7001226 |  719 | `static sxi32 BlobPrepareGrow(SyBlob *pBlob,sxu32 *pByte)` |
|         2 |  720 |  |
|         - |  721 | `	sxu32 nByte;` |
|         - |  722 | `	void *pNew;` |
|   7001228 |  723 | `	nByte = *pByte;` |
|   7001228 |  724 | `	if( pBlob->nFlags & (SXBLOB_LOCKED\|SXBLOB_STATIC) ){` |
|   1004180 |  725 | `		if ( SyBlobFreeSpace(pBlob) < nByte ){` |
|       ! 0 |  726 | `			*pByte = SyBlobFreeSpace(pBlob);` |
|       ! 0 |  727 | `			if( (*pByte) == 0 ){` |
|       ! 0 |  728 | `				return SXERR_SHORT;` |
|         - |  729 | `			}` |
|       ! 0 |  730 | `		}` |
|   1004180 |  731 | `		return SXRET_OK;` |
|         - |  732 | `	}` |
|   5997050 |  733 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|         - |  734 | `		/* Make a copy of the read-only item */` |
|    544424 |  735 | `		if( pBlob->nByte > 0 ){` |
|    544424 |  736 | `			pNew = SyMemBackendDup(pBlob->pAllocator,pBlob->pBlob,pBlob->nByte);` |
|    544424 |  737 | `			if( pNew == 0 ){` |
|       ! 0 |  738 | `				return SXERR_MEM;` |
|         - |  739 | `			}` |
|    544424 |  740 | `			pBlob->pBlob = pNew;` |
|    544424 |  741 | `			pBlob->mByte = pBlob->nByte;` |
|    272213 |  742 | `		}else{` |
|       ! 0 |  743 | `			pBlob->pBlob = 0;` |
|       ! 0 |  744 | `			pBlob->mByte = 0;` |
|         - |  745 | `		}` |
|         - |  746 | `		/* Remove the read-only flag */` |
|    544424 |  747 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|    272211 |  748 | `	}` |
|   5997050 |  749 | `	if( SyBlobFreeSpace(pBlob) >= nByte ){` |
|   1182178 |  750 | `		return SXRET_OK;` |
|         - |  751 | `	}` |
|   4814874 |  752 | `	if( pBlob->mByte > 0 ){` |
|    634532 |  753 | `		nByte = nByte + pBlob->mByte * 2 + SXBLOB_MIN_GROWTH;` |
|   4497602 |  754 | `	}else if ( nByte < SXBLOB_MIN_GROWTH ){` |
|   3375000 |  755 | `		nByte = SXBLOB_MIN_GROWTH;` |
|   1687398 |  756 | `	}` |
|   4814874 |  757 | `	pNew = SyMemBackendRealloc(pBlob->pAllocator,pBlob->pBlob,nByte);` |
|   4814874 |  758 | `	if( pNew == 0 ){` |
|       ! 0 |  759 | `		return SXERR_MEM;` |
|         - |  760 | `	}` |
|   4814874 |  761 | `	pBlob->pBlob = pNew;` |
|   4814874 |  762 | `	pBlob->mByte = nByte;` |
|   4814874 |  763 | `	return SXRET_OK;` |
|   3500659 |  764 |  |
|   7048146 |  765 | `PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize)` |
|         2 |  766 |  |
|         - |  767 | `	sxu8 *zBlob;` |
|         - |  768 | `	sxi32 rc;` |
|   7048148 |  769 | `	if( nSize < 1 ){` |
|     46922 |  770 | `		return SXRET_OK;` |
|         - |  771 | `	}` |
|   7001228 |  772 | `	rc = BlobPrepareGrow(&(*pBlob),&nSize);` |
|   7001228 |  773 | `	if( SXRET_OK != rc ){` |
|       ! 0 |  774 | `		return rc;` |
|         - |  775 | `	}` |
|   7001228 |  776 | `	if( pData ){` |
|   7001196 |  777 | `		zBlob = (sxu8 *)pBlob->pBlob ;` |
|   7001196 |  778 | `		zBlob = &zBlob[pBlob->nByte];` |
|   7001196 |  779 | `		pBlob->nByte += nSize;` |
|  31528897 |  780 | `		SX_MACRO_FAST_MEMCPY(pData,zBlob,nSize);` |
|   3500641 |  781 | `	}` |
|   7001228 |  782 | `	return SXRET_OK;` |
|   3524119 |  783 |  |
|    545214 |  784 | `PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob)` |
|         2 |  785 |  |
|         - |  786 | `	sxi32 rc;` |
|         - |  787 | `	sxu32 n;` |
|    545216 |  788 | `	n = pBlob->nByte;` |
|    545216 |  789 | `	rc = SyBlobAppend(&(*pBlob),(const void *)"\0",sizeof(char));` |
|    545216 |  790 | `	if (rc == SXRET_OK ){` |
|    545216 |  791 | `		pBlob->nByte = n;` |
|    272629 |  792 | `	}` |
|    545216 |  793 | `	return rc;` |
|         2 |  794 |  |
|   3495630 |  795 | `PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest)` |
|         2 |  796 |  |
|   3495632 |  797 | `	sxi32 rc = SXRET_OK;` |
|         - |  798 | `#ifdef UNTRUST` |
|         - |  799 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|         - |  800 | `		return SXERR_EMPTY;` |
|         - |  801 | `	}` |
|         - |  802 | `#endif` |
|   3495632 |  803 | `	if( pSrc->nByte > 0 ){` |
|   3474124 |  804 | `		rc = SyBlobAppend(&(*pDest),pSrc->pBlob,pSrc->nByte);` |
|   1737061 |  805 | `	}` |
|   3495632 |  806 | `	return rc;` |
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
|   3998396 |  827 | `PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob)` |
|         2 |  828 |  |
|   3998398 |  829 | `	pBlob->nByte = 0;` |
|   3998398 |  830 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|      7884 |  831 | `		pBlob->pBlob = 0;` |
|      7884 |  832 | `		pBlob->mByte = 0;` |
|      7884 |  833 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|      3941 |  834 | `	}` |
|   3998398 |  835 | `	return SXRET_OK;` |
|         2 |  836 |  |
|   9971226 |  837 | `PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob)` |
|         2 |  838 |  |
|   9971228 |  839 | `	if( (pBlob->nFlags & (SXBLOB_STATIC\|SXBLOB_RDONLY)) == 0 && pBlob->mByte > 0 ){` |
|   4400420 |  840 | `		SyMemBackendFree(pBlob->pAllocator,pBlob->pBlob);` |
|   2200231 |  841 | `	}` |
|   9971228 |  842 | `	pBlob->pBlob = 0;` |
|   9971228 |  843 | `	pBlob->nByte = pBlob->mByte = 0;` |
|   9971228 |  844 | `	pBlob->nFlags = 0;` |
|   9971228 |  845 | `	return SXRET_OK;` |
|         2 |  846 |  |
|         - |  847 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    122882 |  848 | `PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft)` |
|         2 |  849 |  |
|    122884 |  850 | `	const char *zIn = (const char *)pBlob;` |
|         - |  851 | `	const char *zEnd;` |
|         - |  852 | `	sxi32 rc;` |
|    122884 |  853 | `	if( pLen > nLen ){` |
|      4704 |  854 | `		return SXERR_NOTFOUND;` |
|         - |  855 | `	}` |
|    118182 |  856 | `	zEnd = &zIn[nLen-pLen];` |
|    983221 |  857 | `	for(;;){` |
|   1966381 |  858 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   1936362 |  859 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   1894610 |  860 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   1868582 |  861 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|         2 |  862 | `	}` |
|     18218 |  863 | `	return SXERR_NOTFOUND;` |
|     61443 |  864 |  |
|         - |  865 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - |  866 |  |
