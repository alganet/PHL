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
|   8986264 |   18 | `static void * SyOSHeapAlloc(sxu32 nByte)` |
|         2 |   19 |  |
|         - |   20 | `	void *pNew;` |
|         - |   21 | `#if defined(__WINNT__)` |
|         2 |   22 | `	pNew = HeapAlloc(GetProcessHeap(),0,nByte);` |
|         - |   23 | `#else` |
|   8986264 |   24 | `	pNew = malloc((size_t)nByte);` |
|         - |   25 | `#endif` |
|   8986266 |   26 | `	return pNew;` |
|         2 |   27 |  |
|    580210 |   28 | `static void * SyOSHeapRealloc(void *pOld,sxu32 nByte)` |
|         2 |   29 |  |
|         - |   30 | `	void *pNew;` |
|         - |   31 | `#if defined(__WINNT__)` |
|         2 |   32 | `	pNew = HeapReAlloc(GetProcessHeap(),0,pOld,nByte);` |
|         - |   33 | `#else` |
|    580210 |   34 | `	pNew = realloc(pOld,(size_t)nByte);` |
|         - |   35 | `#endif` |
|    580212 |   36 | `	return pNew;` |
|         2 |   37 |  |
|   8976792 |   38 | `static void SyOSHeapFree(void *pPtr)` |
|         2 |   39 |  |
|         - |   40 | `#if defined(__WINNT__)` |
|         2 |   41 | `	HeapFree(GetProcessHeap(),0,pPtr);` |
|         - |   42 | `#else` |
|   8976792 |   43 | `	free(pPtr);` |
|         - |   44 | `#endif` |
|   8976794 |   45 |  |
|         - |   46 |  |
|         - |   47 |  |
|  15044430 |   48 | `PH7_PRIVATE void SyZero(void *pSrc,sxu32 nSize)` |
|         2 |   49 |  |
|  15044432 |   50 | `	register unsigned char *zSrc = (unsigned char *)pSrc;` |
|         - |   51 | `	unsigned char *zEnd;` |
|         - |   52 | `#if defined(UNTRUST)` |
|         - |   53 | `	if( zSrc == 0 \|\| nSize <= 0 ){` |
|         - |   54 | `		return ;` |
|         - |   55 | `	}` |
|         - |   56 | `#endif` |
|  15044432 |   57 | `	zEnd = &zSrc[nSize];` |
| 201310168 |   58 | `	for(;;){` |
| 402616310 |   59 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 387571884 |   60 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 387571882 |   61 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 387571882 |   62 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
|         2 |   63 | `	}` |
|  15044432 |   64 |  |
|  11490126 |   65 | `PH7_PRIVATE sxi32 SyMemcmp(const void *pB1,const void *pB2,sxu32 nSize)` |
|         2 |   66 |  |
|         - |   67 | `	sxi32 rc;` |
|  11490128 |   68 | `	if( nSize <= 0 ){` |
|        86 |   69 | `		return 0;` |
|         - |   70 | `	}` |
|  11490044 |   71 | `	if( pB1 == 0 \|\| pB2 == 0 ){` |
|       ! 0 |   72 | `		return pB1 != 0 ? 1 : (pB2 == 0 ? 0 : -1);` |
|         - |   73 | `	}` |
|  24152653 |   74 | `	SX_MACRO_FAST_CMP(pB1,pB2,nSize,rc);` |
|  11490044 |   75 | `	return rc;` |
|   5745583 |   76 |  |
|   8058810 |   77 | `PH7_PRIVATE sxu32 SyMemcpy(const void *pSrc,void *pDest,sxu32 nLen)` |
|         2 |   78 |  |
|         - |   79 | `#if defined(UNTRUST)` |
|         - |   80 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|         - |   81 | `		return 0;` |
|         - |   82 | `	}` |
|         - |   83 | `#endif` |
|   8058812 |   84 | `	if( pSrc == (const void *)pDest ){` |
|       ! 0 |   85 | `		return nLen;` |
|         - |   86 | `	}` |
|  65604009 |   87 | `	SX_MACRO_FAST_MEMCPY(pSrc,pDest,nLen);` |
|   8058812 |   88 | `	return nLen;` |
|   4029645 |   89 |  |
|   8986264 |   90 | `static void * MemOSAlloc(sxu32 nBytes)` |
|         2 |   91 |  |
|         - |   92 | `	sxu32 *pChunk;` |
|   8986266 |   93 | `	pChunk = (sxu32 *)SyOSHeapAlloc(nBytes + sizeof(sxu32));` |
|   8986266 |   94 | `	if( pChunk == 0 ){` |
|       ! 0 |   95 | `		return 0;` |
|         - |   96 | `	}` |
|   8986266 |   97 | `	pChunk[0] = nBytes;` |
|   8986266 |   98 | `	return (void *)&pChunk[1];` |
|   4493156 |   99 |  |
|    580210 |  100 | `static void * MemOSRealloc(void *pOld,sxu32 nBytes)` |
|         2 |  101 |  |
|         - |  102 | `	sxu32 *pOldChunk;` |
|         - |  103 | `	sxu32 *pChunk;` |
|    580212 |  104 | `	pOldChunk = (sxu32 *)(((char *)pOld)-sizeof(sxu32));` |
|    580212 |  105 | `	if( pOldChunk[0] >= nBytes ){` |
|       ! 0 |  106 | `		return pOld;` |
|         - |  107 | `	}` |
|    580212 |  108 | `	pChunk = (sxu32 *)SyOSHeapRealloc(pOldChunk,nBytes + sizeof(sxu32));` |
|    580212 |  109 | `	if( pChunk == 0 ){` |
|       ! 0 |  110 | `		return 0;` |
|         - |  111 | `	}` |
|    580212 |  112 | `	pChunk[0] = nBytes;` |
|    580212 |  113 | `	return (void *)&pChunk[1];` |
|    290095 |  114 |  |
|   8976792 |  115 | `static void MemOSFree(void *pBlock)` |
|         2 |  116 |  |
|         - |  117 | `	void *pChunk;` |
|   8976794 |  118 | `	pChunk = (void *)(((char *)pBlock)-sizeof(sxu32));` |
|   8976794 |  119 | `	SyOSHeapFree(pChunk);` |
|   8976794 |  120 |  |
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
|   8986264 |  137 | `static void * MemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         2 |  138 |  |
|         - |  139 | `	SyMemBlock *pBlock;` |
|   8986266 |  140 | `	sxi32 nRetry = 0;` |
|         - |  141 |  |
|         - |  142 | `	/* Append an extra block so we can tracks allocated chunks and avoid memory` |
|         - |  143 | `	 * leaks.` |
|         - |  144 | `	 */` |
|   8986266 |  145 | `	nByte += sizeof(SyMemBlock);` |
|   4493154 |  146 | `	for(;;){` |
|   4493156 |  147 | `		pBlock = (SyMemBlock *)pBackend->pMethods->xAlloc(nByte);` |
|   8986264 |  148 | `		if( pBlock != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY` |
|         2 |  149 | `			\|\| SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|   4493156 |  150 | `				break;` |
|         - |  151 | `		}` |
|       ! 0 |  152 | `		nRetry++;` |
|       ! 0 |  153 | `	}` |
|   8986266 |  154 | `	if( pBlock  == 0 ){` |
|       ! 0 |  155 | `		return 0;` |
|         - |  156 | `	}` |
|   8986266 |  157 | `	pBlock->pNext = pBlock->pPrev = 0;` |
|         - |  158 | `	/* Link to the list of already tracked blocks */` |
|   8986266 |  159 | `	MACRO_LD_PUSH(pBackend->pBlocks,pBlock);` |
|         - |  160 | `#if defined(UNTRUST)` |
|         - |  161 | `	pBlock->nGuard = SXMEM_BACKEND_MAGIC;` |
|         - |  162 | `#endif` |
|   8986266 |  163 | `	pBackend->nBlock++;` |
|   8986266 |  164 | `	return (void *)&pBlock[1];` |
|   4493156 |  165 |  |
|   2155090 |  166 | `PH7_PRIVATE void * SyMemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         2 |  167 |  |
|         - |  168 | `	void *pChunk;` |
|         - |  169 | `#if defined(UNTRUST)` |
|         - |  170 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  171 | `		return 0;` |
|         - |  172 | `	}` |
|         - |  173 | `#endif` |
|   2155092 |  174 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  175 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  176 | `	}` |
|   2155092 |  177 | `	pChunk = MemBackendAlloc(&(*pBackend),nByte);` |
|   2155092 |  178 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  179 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  180 | `	}` |
|   2155092 |  181 | `	return pChunk;` |
|         2 |  182 |  |
|   7377822 |  183 | `static void * MemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         2 |  184 |  |
|         - |  185 | `	SyMemBlock *pBlock,*pNew,*pPrev,*pNext;` |
|   7377824 |  186 | `	sxu32 nRetry = 0;` |
|         - |  187 |  |
|   7377824 |  188 | `	if( pOld == 0 ){` |
|   6797614 |  189 | `		return MemBackendAlloc(&(*pBackend),nByte);` |
|         - |  190 | `	}` |
|    580212 |  191 | `	pBlock = (SyMemBlock *)(((char *)pOld) - sizeof(SyMemBlock));` |
|         - |  192 | `#if defined(UNTRUST)` |
|         - |  193 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|         - |  194 | `		return 0;` |
|         - |  195 | `	}` |
|         - |  196 | `#endif` |
|    580212 |  197 | `	nByte += sizeof(SyMemBlock);` |
|    580212 |  198 | `	pPrev = pBlock->pPrev;` |
|    580212 |  199 | `	pNext = pBlock->pNext;` |
|    290093 |  200 | `	for(;;){` |
|    290095 |  201 | `		pNew = (SyMemBlock *)pBackend->pMethods->xRealloc(pBlock,nByte);` |
|    580212 |  202 | `		if( pNew != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY \|\|` |
|       ! 0 |  203 | `			SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|    290095 |  204 | `				break;` |
|         - |  205 | `		}` |
|       ! 0 |  206 | `		nRetry++;` |
|       ! 0 |  207 | `	}` |
|    580212 |  208 | `	if( pNew == 0 ){` |
|       ! 0 |  209 | `		return 0;` |
|         - |  210 | `	}` |
|    580212 |  211 | `	if( pNew != pBlock ){` |
|    520704 |  212 | `		if( pPrev == 0 ){` |
|    416479 |  213 | `			pBackend->pBlocks = pNew;` |
|    228898 |  214 | `		}else{` |
|    104227 |  215 | `			pPrev->pNext = pNew;` |
|         - |  216 | `		}` |
|    520704 |  217 | `		if( pNext ){` |
|    520695 |  218 | `			pNext->pPrev = pNew;` |
|    280904 |  219 | `		}` |
|         - |  220 | `#if defined(UNTRUST)` |
|         - |  221 | `		pNew->nGuard = SXMEM_BACKEND_MAGIC;` |
|         - |  222 | `#endif` |
|    280909 |  223 | `	}` |
|    580212 |  224 | `	return (void *)&pNew[1];` |
|   3688923 |  225 |  |
|   7377822 |  226 | `PH7_PRIVATE void * SyMemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         2 |  227 |  |
|         - |  228 | `	void *pChunk;` |
|         - |  229 | `#if defined(UNTRUST)` |
|         - |  230 | `	if( SXMEM_BACKEND_CORRUPT(pBackend)  ){` |
|         - |  231 | `		return 0;` |
|         - |  232 | `	}` |
|         - |  233 | `#endif` |
|   7377824 |  234 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  235 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  236 | `	}` |
|   7377824 |  237 | `	pChunk = MemBackendRealloc(&(*pBackend),pOld,nByte);` |
|   7377824 |  238 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  239 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  240 | `	}` |
|   7377824 |  241 | `	return pChunk;` |
|         2 |  242 |  |
|   7447464 |  243 | `static sxi32 MemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|         2 |  244 |  |
|         - |  245 | `	SyMemBlock *pBlock;` |
|   7447466 |  246 | `	pBlock = (SyMemBlock *)(((char *)pChunk) - sizeof(SyMemBlock));` |
|         - |  247 | `#if defined(UNTRUST)` |
|         - |  248 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|         - |  249 | `		return SXERR_CORRUPT;` |
|         - |  250 | `	}` |
|         - |  251 | `#endif` |
|         - |  252 | `	/* Unlink from the list of active blocks */` |
|   7447466 |  253 | `	if( pBackend->nBlock > 0 ){` |
|         - |  254 | `		/* Release the block */` |
|         - |  255 | `#if defined(UNTRUST)` |
|         - |  256 | `		/* Mark as stale block */` |
|         - |  257 | `		pBlock->nGuard = 0x635B;` |
|         - |  258 | `#endif` |
|   7447466 |  259 | `		MACRO_LD_REMOVE(pBackend->pBlocks,pBlock);` |
|   7447466 |  260 | `		pBackend->nBlock--;` |
|   7447466 |  261 | `		pBackend->pMethods->xFree(pBlock);` |
|   3723754 |  262 | `	}` |
|   7447466 |  263 | `	return SXRET_OK;` |
|         2 |  264 |  |
|   7447464 |  265 | `PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|         2 |  266 |  |
|         - |  267 | `	sxi32 rc;` |
|         - |  268 | `#if defined(UNTRUST)` |
|         - |  269 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  270 | `		return SXERR_CORRUPT;` |
|         - |  271 | `	}` |
|         - |  272 | `#endif` |
|   7447466 |  273 | `	if( pChunk == 0 ){` |
|       ! 0 |  274 | `		return SXRET_OK;` |
|         - |  275 | `	}` |
|   7447466 |  276 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  277 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  278 | `	}` |
|   7447466 |  279 | `	rc = MemBackendFree(&(*pBackend),pChunk);` |
|   7447466 |  280 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  281 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  282 | `	}` |
|   7447466 |  283 | `	return rc;` |
|   3723756 |  284 |  |
|         - |  285 | `#if defined(PH7_ENABLE_THREADS)` |
|      1630 |  286 | `PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods)` |
|         2 |  287 |  |
|         - |  288 | `	SyMutex *pMutex;` |
|         - |  289 | `#if defined(UNTRUST)` |
|         - |  290 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pMethods == 0 \|\| pMethods->xNew == 0){` |
|         - |  291 | `		return SXERR_CORRUPT;` |
|         - |  292 | `	}` |
|         - |  293 | `#endif` |
|      1632 |  294 | `	pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|      1632 |  295 | `	if( pMutex == 0 ){` |
|       ! 0 |  296 | `		return SXERR_OS;` |
|         - |  297 | `	}` |
|         - |  298 | `	/* Attach the mutex to the memory backend */` |
|      1632 |  299 | `	pBackend->pMutex = pMutex;` |
|      1632 |  300 | `	pBackend->pMutexMethods = pMethods;` |
|      1632 |  301 | `	return SXRET_OK;` |
|       817 |  302 |  |
|      1630 |  303 | `PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend)` |
|         2 |  304 |  |
|         - |  305 | `#if defined(UNTRUST)` |
|         - |  306 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  307 | `		return SXERR_CORRUPT;` |
|         - |  308 | `	}` |
|         - |  309 | `#endif` |
|      1632 |  310 | `	if( pBackend->pMutex == 0 ){` |
|         - |  311 | `		/* There is no mutex subsystem at all */` |
|       ! 0 |  312 | `		return SXRET_OK;` |
|         - |  313 | `	}` |
|      1632 |  314 | `	SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1632 |  315 | `	pBackend->pMutexMethods = 0;` |
|      1632 |  316 | `	pBackend->pMutex = 0;` |
|      1632 |  317 | `	return SXRET_OK;` |
|       817 |  318 |  |
|         - |  319 | `#endif` |
|         - |  320 | `/*` |
|         - |  321 | ` * Memory pool allocator` |
|         - |  322 | ` */` |
|         - |  323 | `#define SXMEM_POOL_MAGIC		0xDEAD` |
|         - |  324 | `#define SXMEM_POOL_MAXALLOC		(1<<(SXMEM_POOL_NBUCKETS+SXMEM_POOL_INCR))` |
|         - |  325 | `#define SXMEM_POOL_MINALLOC		(1<<(SXMEM_POOL_INCR))` |
|     33562 |  326 | `static sxi32 MemPoolBucketAlloc(SyMemBackend *pBackend,sxu32 nBucket)` |
|         2 |  327 |  |
|         - |  328 | `	char *zBucket,*zBucketEnd;` |
|         - |  329 | `	SyMemHeader *pHeader;` |
|         - |  330 | `	sxu32 nBucketSize;` |
|         - |  331 |  |
|         - |  332 | `	/* Allocate one big block first */` |
|     33564 |  333 | `	zBucket = (char *)MemBackendAlloc(&(*pBackend),SXMEM_POOL_MAXALLOC);` |
|     33564 |  334 | `	if( zBucket == 0 ){` |
|       ! 0 |  335 | `		return SXERR_MEM;` |
|         - |  336 | `	}` |
|     33564 |  337 | `	zBucketEnd = &zBucket[SXMEM_POOL_MAXALLOC];` |
|         - |  338 | `	/* Divide the big block into mini bucket pool */` |
|     33564 |  339 | `	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);` |
|     33564 |  340 | `	pBackend->apPool[nBucket] = pHeader = (SyMemHeader *)zBucket;` |
|   3943864 |  341 | `	for(;;){` |
|   7887730 |  342 | `		if( &zBucket[nBucketSize] >= zBucketEnd ){` |
|     33564 |  343 | `			break;` |
|         - |  344 | `		}` |
|   7854168 |  345 | `		pHeader->pNext = (SyMemHeader *)&zBucket[nBucketSize];` |
|         - |  346 | `		/* Advance the cursor to the next available chunk */` |
|   7854168 |  347 | `		pHeader = pHeader->pNext;` |
|   7854168 |  348 | `		zBucket += nBucketSize;` |
|         2 |  349 | `	}` |
|     33564 |  350 | `	pHeader->pNext = 0;` |
|         - |  351 |  |
|     33564 |  352 | `	return SXRET_OK;` |
|     16783 |  353 |  |
|   9841812 |  354 | `static void * MemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         2 |  355 |  |
|         - |  356 | `	SyMemHeader *pBucket,*pNext;` |
|         - |  357 | `	sxu32 nBucketSize;` |
|         - |  358 | `	sxu32 nBucket;` |
|         - |  359 |  |
|   9841814 |  360 | `	if( nByte + sizeof(SyMemHeader) >= SXMEM_POOL_MAXALLOC ){` |
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
|   9841814 |  371 | `	nBucket = 0;` |
|   9841814 |  372 | `	nBucketSize = SXMEM_POOL_MINALLOC;` |
|  49040018 |  373 | `	while( nByte + sizeof(SyMemHeader) > nBucketSize  ){` |
|  39198206 |  374 | `		nBucketSize <<= 1;` |
|  39198206 |  375 | `		nBucket++;` |
|         2 |  376 | `	}` |
|   9841814 |  377 | `	pBucket = pBackend->apPool[nBucket];` |
|   9841814 |  378 | `	if( pBucket == 0 ){` |
|         - |  379 | `		sxi32 rc;` |
|     33564 |  380 | `		rc = MemPoolBucketAlloc(&(*pBackend),nBucket);` |
|     33564 |  381 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  382 | `			return 0;` |
|         - |  383 | `		}` |
|     33564 |  384 | `		pBucket = pBackend->apPool[nBucket];` |
|     16781 |  385 | `	}` |
|         - |  386 | `	/* Remove from the free list */` |
|   9841814 |  387 | `	pNext = pBucket->pNext;` |
|   9841814 |  388 | `	pBackend->apPool[nBucket] = pNext;` |
|         - |  389 | `	/* Record bucket&magic number */` |
|   9841814 |  390 | `	pBucket->nBucket = (SXMEM_POOL_MAGIC << 16) \| nBucket;` |
|   9841814 |  391 | `	return (void *)&pBucket[1];` |
|   4920908 |  392 |  |
|   9841812 |  393 | `PH7_PRIVATE void * SyMemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         2 |  394 |  |
|         - |  395 | `	void *pChunk;` |
|         - |  396 | `#if defined(UNTRUST)` |
|         - |  397 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  398 | `		return 0;` |
|         - |  399 | `	}` |
|         - |  400 | `#endif` |
|   9841814 |  401 | `	if( pBackend->pMutexMethods ){` |
|      1632 |  402 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       815 |  403 | `	}` |
|   9841814 |  404 | `	pChunk = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|   9841814 |  405 | `	if( pBackend->pMutexMethods ){` |
|      1632 |  406 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       815 |  407 | `	}` |
|   9841814 |  408 | `	return pChunk;` |
|         2 |  409 |  |
|   7287954 |  410 | `static sxi32 MemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|         2 |  411 |  |
|         - |  412 | `	SyMemHeader *pHeader;` |
|         - |  413 | `	sxu32 nBucket;` |
|         - |  414 | `	/* Get the corresponding bucket */` |
|   7287956 |  415 | `	pHeader = (SyMemHeader *)(((char *)pChunk) - sizeof(SyMemHeader));` |
|         - |  416 | `	/* Sanity check to avoid misuse */` |
|   7287956 |  417 | `	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){` |
|       ! 0 |  418 | `		return SXERR_CORRUPT;` |
|         - |  419 | `	}` |
|   7287956 |  420 | `	nBucket = pHeader->nBucket & 0xFFFF;` |
|   7287956 |  421 | `	if( nBucket == SXU16_HIGH ){` |
|         - |  422 | `		/* Free the big block */` |
|       ! 0 |  423 | `		MemBackendFree(&(*pBackend),pHeader);` |
|       ! 0 |  424 | `	}else{` |
|         - |  425 | `		/* Return to the free list */` |
|   7287956 |  426 | `		pHeader->pNext = pBackend->apPool[nBucket & 0x0f];` |
|   7287956 |  427 | `		pBackend->apPool[nBucket & 0x0f] = pHeader;` |
|         - |  428 | `	}` |
|   7287956 |  429 | `	return SXRET_OK;` |
|   3643979 |  430 |  |
|   7287954 |  431 | `PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|         2 |  432 |  |
|         - |  433 | `	sxi32 rc;` |
|         - |  434 | `#if defined(UNTRUST)` |
|         - |  435 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pChunk == 0 ){` |
|         - |  436 | `		return SXERR_CORRUPT;` |
|         - |  437 | `	}` |
|         - |  438 | `#endif` |
|   7287956 |  439 | `	if( pBackend->pMutexMethods ){` |
|      1366 |  440 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       682 |  441 | `	}` |
|   7287956 |  442 | `	rc = MemBackendPoolFree(&(*pBackend),pChunk);` |
|   7287956 |  443 | `	if( pBackend->pMutexMethods ){` |
|      1366 |  444 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       682 |  445 | `	}` |
|   7287956 |  446 | `	return rc;` |
|         2 |  447 |  |
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
|      1630 |  505 | `PH7_PRIVATE sxi32 SyMemBackendInit(SyMemBackend *pBackend,ProcMemError xMemErr,void * pUserData)` |
|         2 |  506 |  |
|         - |  507 | `#if defined(UNTRUST)` |
|         - |  508 | `	if( pBackend == 0 ){` |
|         - |  509 | `		return SXERR_EMPTY;` |
|         - |  510 | `	}` |
|         - |  511 | `#endif` |
|         - |  512 | `	/* Zero the allocator first */` |
|      1632 |  513 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|      1632 |  514 | `	pBackend->xMemError = xMemErr;` |
|      1632 |  515 | `	pBackend->pUserData = pUserData;` |
|         - |  516 | `	/* Switch to the OS memory allocator */` |
|      1632 |  517 | `	pBackend->pMethods = &sOSAllocMethods;` |
|      1632 |  518 | `	if( pBackend->pMethods->xInit ){` |
|         - |  519 | `		/* Initialize the backend  */` |
|       ! 0 |  520 | `		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){` |
|       ! 0 |  521 | `			return SXERR_ABORT;` |
|         - |  522 | `		}` |
|       ! 0 |  523 | `	}` |
|         - |  524 | `#if defined(UNTRUST)` |
|         - |  525 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|         - |  526 | `#endif` |
|      1632 |  527 | `	return SXRET_OK;` |
|       817 |  528 |  |
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
|      3260 |  557 | `PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend *pBackend,SyMemBackend *pParent)` |
|         2 |  558 |  |
|         - |  559 | `	sxu8 bInheritMutex;` |
|         - |  560 | `#if defined(UNTRUST)` |
|         - |  561 | `	if( pBackend == 0 \|\| SXMEM_BACKEND_CORRUPT(pParent) ){` |
|         - |  562 | `		return SXERR_CORRUPT;` |
|         - |  563 | `	}` |
|         - |  564 | `#endif` |
|         - |  565 | `	/* Zero the allocator first */` |
|      3262 |  566 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|      3262 |  567 | `	pBackend->pMethods  = pParent->pMethods;` |
|      3262 |  568 | `	pBackend->xMemError = pParent->xMemError;` |
|      3262 |  569 | `	pBackend->pUserData = pParent->pUserData;` |
|      3262 |  570 | `	bInheritMutex = pParent->pMutexMethods ? TRUE : FALSE;` |
|      3262 |  571 | `	if( bInheritMutex ){` |
|      1632 |  572 | `		pBackend->pMutexMethods = pParent->pMutexMethods;` |
|         - |  573 | `		/* Create a private mutex */` |
|      1632 |  574 | `		pBackend->pMutex = pBackend->pMutexMethods->xNew(SXMUTEX_TYPE_FAST);` |
|      1632 |  575 | `		if( pBackend->pMutex ==  0){` |
|       ! 0 |  576 | `			return SXERR_OS;` |
|         - |  577 | `		}` |
|       815 |  578 | `	}` |
|         - |  579 | `#if defined(UNTRUST)` |
|         - |  580 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|         - |  581 | `#endif` |
|      3262 |  582 | `	return SXRET_OK;` |
|      1632 |  583 |  |
|      3502 |  584 | `static sxi32 MemBackendRelease(SyMemBackend *pBackend)` |
|         2 |  585 |  |
|         - |  586 | `	SyMemBlock *pBlock,*pNext;` |
|         - |  587 |  |
|      3504 |  588 | `	pBlock = pBackend->pBlocks;` |
|    192441 |  589 | `	for(;;){` |
|    384884 |  590 | `		if( pBackend->nBlock == 0 ){` |
|       489 |  591 | `			break;` |
|         - |  592 | `		}` |
|    384396 |  593 | `		pNext  = pBlock->pNext;` |
|    384396 |  594 | `		pBackend->pMethods->xFree(pBlock);` |
|    384396 |  595 | `		pBlock = pNext;` |
|    384396 |  596 | `		pBackend->nBlock--;` |
|         - |  597 | `		/* LOOP ONE */` |
|    384396 |  598 | `		if( pBackend->nBlock == 0 ){` |
|      2480 |  599 | `			break;` |
|         - |  600 | `		}` |
|    381918 |  601 | `		pNext  = pBlock->pNext;` |
|    381918 |  602 | `		pBackend->pMethods->xFree(pBlock);` |
|    381918 |  603 | `		pBlock = pNext;` |
|    381918 |  604 | `		pBackend->nBlock--;` |
|         - |  605 | `		/* LOOP TWO */` |
|    381918 |  606 | `		if( pBackend->nBlock == 0 ){` |
|       280 |  607 | `			break;` |
|         - |  608 | `		}` |
|    381640 |  609 | `		pNext  = pBlock->pNext;` |
|    381640 |  610 | `		pBackend->pMethods->xFree(pBlock);` |
|    381640 |  611 | `		pBlock = pNext;` |
|    381640 |  612 | `		pBackend->nBlock--;` |
|         - |  613 | `		/* LOOP THREE */` |
|    381640 |  614 | `		if( pBackend->nBlock == 0 ){` |
|       259 |  615 | `			break;` |
|         - |  616 | `		}` |
|    381382 |  617 | `		pNext  = pBlock->pNext;` |
|    381382 |  618 | `		pBackend->pMethods->xFree(pBlock);` |
|    381382 |  619 | `		pBlock = pNext;` |
|    381382 |  620 | `		pBackend->nBlock--;` |
|         - |  621 | `		/* LOOP FOUR */` |
|         2 |  622 | `	}` |
|      3504 |  623 | `	if( pBackend->pMethods->xRelease ){` |
|       ! 0 |  624 | `		pBackend->pMethods->xRelease(pBackend->pMethods->pUserData);` |
|       ! 0 |  625 | `	}` |
|      3504 |  626 | `	pBackend->pMethods = 0;` |
|      3504 |  627 | `	pBackend->pBlocks  = 0;` |
|         - |  628 | `#if defined(UNTRUST)` |
|         - |  629 | `	pBackend->nMagic = 0x2626;` |
|         - |  630 | `#endif` |
|      3504 |  631 | `	return SXRET_OK;` |
|         2 |  632 |  |
|      3502 |  633 | `PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend *pBackend)` |
|         2 |  634 |  |
|         - |  635 | `#if defined(UNTRUST)` |
|         - |  636 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  637 | `		return SXERR_INVALID;` |
|         - |  638 | `	}` |
|         - |  639 | `#endif` |
|      3504 |  640 | `	if( pBackend->pMutexMethods ){` |
|       259 |  641 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       129 |  642 | `	}` |
|      3504 |  643 | `	(void)MemBackendRelease(&(*pBackend));` |
|      3504 |  644 | `	if( pBackend->pMutexMethods ){` |
|       259 |  645 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       259 |  646 | `		SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|       129 |  647 | `	}` |
|      3504 |  648 | `	return SXRET_OK;` |
|         2 |  649 |  |
|    447990 |  650 | `PH7_PRIVATE void * SyMemBackendDup(SyMemBackend *pBackend,const void *pSrc,sxu32 nSize)` |
|         2 |  651 |  |
|         - |  652 | `	void *pNew;` |
|         - |  653 | `#if defined(UNTRUST)` |
|         - |  654 | `	if( pSrc == 0 \|\| nSize <= 0 ){` |
|         - |  655 | `		return 0;` |
|         - |  656 | `	}` |
|         - |  657 | `#endif` |
|    447992 |  658 | `	pNew = SyMemBackendAlloc(&(*pBackend),nSize);` |
|    447992 |  659 | `	if( pNew ){` |
|    447992 |  660 | `		SyMemcpy(pSrc,pNew,nSize);` |
|    223995 |  661 | `	}` |
|    447992 |  662 | `	return pNew;` |
|         2 |  663 |  |
|   1055016 |  664 | `PH7_PRIVATE char * SyMemBackendStrDup(SyMemBackend *pBackend,const char *zSrc,sxu32 nSize)` |
|         2 |  665 |  |
|         - |  666 | `	char *zDest;` |
|   1055018 |  667 | `	zDest = (char *)SyMemBackendAlloc(&(*pBackend),nSize + 1);` |
|   1055018 |  668 | `	if( zDest ){` |
|   1055018 |  669 | `		Systrcpy(zDest,nSize+1,zSrc,nSize);` |
|    527508 |  670 | `	}` |
|   1055018 |  671 | `	return zDest;` |
|         2 |  672 |  |
|     47658 |  673 | `PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize)` |
|         2 |  674 |  |
|         - |  675 | `#if defined(UNTRUST)` |
|         - |  676 | `	if( pBlob == 0 \|\| pBuffer == 0 \|\| nSize < 1 ){` |
|         - |  677 | `		return SXERR_EMPTY;` |
|         - |  678 | `	}` |
|         - |  679 | `#endif` |
|     47660 |  680 | `	pBlob->pBlob = pBuffer;` |
|     47660 |  681 | `	pBlob->mByte = nSize;` |
|     47660 |  682 | `	pBlob->nByte = 0;` |
|     47660 |  683 | `	pBlob->pAllocator = 0;` |
|     47660 |  684 | `	pBlob->nFlags = SXBLOB_LOCKED\|SXBLOB_STATIC;` |
|     47660 |  685 | `	return SXRET_OK;` |
|         2 |  686 |  |
|   5398530 |  687 | `PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator)` |
|         2 |  688 |  |
|         - |  689 | `#if defined(UNTRUST)` |
|         - |  690 | `	if( pBlob == 0  ){` |
|         - |  691 | `		return SXERR_EMPTY;` |
|         - |  692 | `	}` |
|         - |  693 | `#endif` |
|   5398532 |  694 | `	pBlob->pBlob = 0;` |
|   5398532 |  695 | `	pBlob->mByte = pBlob->nByte	= 0;` |
|   5398532 |  696 | `	pBlob->pAllocator = &(*pAllocator);` |
|   5398532 |  697 | `	pBlob->nFlags = 0;` |
|   5398532 |  698 | `	return SXRET_OK;` |
|         2 |  699 |  |
|   2074780 |  700 | `PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte)` |
|         2 |  701 |  |
|         - |  702 | `#if defined(UNTRUST)` |
|         - |  703 | `	if( pBlob == 0  ){` |
|         - |  704 | `		return SXERR_EMPTY;` |
|         - |  705 | `	}` |
|         - |  706 | `#endif` |
|   2074782 |  707 | `	pBlob->pBlob = (void *)pData;` |
|   2074782 |  708 | `	pBlob->nByte = nByte;` |
|   2074782 |  709 | `	pBlob->mByte = 0;` |
|   2074782 |  710 | `	pBlob->nFlags \|= SXBLOB_RDONLY;` |
|   2074782 |  711 | `	return SXRET_OK;` |
|         2 |  712 |  |
|         - |  713 | `#ifndef SXBLOB_MIN_GROWTH` |
|         - |  714 | `#define SXBLOB_MIN_GROWTH 16` |
|         - |  715 | `#endif` |
|   5477048 |  716 | `static sxi32 BlobPrepareGrow(SyBlob *pBlob,sxu32 *pByte)` |
|         2 |  717 |  |
|         - |  718 | `	sxu32 nByte;` |
|         - |  719 | `	void *pNew;` |
|   5477050 |  720 | `	nByte = *pByte;` |
|   5477050 |  721 | `	if( pBlob->nFlags & (SXBLOB_LOCKED\|SXBLOB_STATIC) ){` |
|    380532 |  722 | `		if ( SyBlobFreeSpace(pBlob) < nByte ){` |
|       ! 0 |  723 | `			*pByte = SyBlobFreeSpace(pBlob);` |
|       ! 0 |  724 | `			if( (*pByte) == 0 ){` |
|       ! 0 |  725 | `				return SXERR_SHORT;` |
|         - |  726 | `			}` |
|       ! 0 |  727 | `		}` |
|    380532 |  728 | `		return SXRET_OK;` |
|         - |  729 | `	}` |
|   5096520 |  730 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|         - |  731 | `		/* Make a copy of the read-only item */` |
|    447992 |  732 | `		if( pBlob->nByte > 0 ){` |
|    447992 |  733 | `			pNew = SyMemBackendDup(pBlob->pAllocator,pBlob->pBlob,pBlob->nByte);` |
|    447992 |  734 | `			if( pNew == 0 ){` |
|       ! 0 |  735 | `				return SXERR_MEM;` |
|         - |  736 | `			}` |
|    447992 |  737 | `			pBlob->pBlob = pNew;` |
|    447992 |  738 | `			pBlob->mByte = pBlob->nByte;` |
|    223997 |  739 | `		}else{` |
|       ! 0 |  740 | `			pBlob->pBlob = 0;` |
|       ! 0 |  741 | `			pBlob->mByte = 0;` |
|         - |  742 | `		}` |
|         - |  743 | `		/* Remove the read-only flag */` |
|    447992 |  744 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|    223995 |  745 | `	}` |
|   5096520 |  746 | `	if( SyBlobFreeSpace(pBlob) >= nByte ){` |
|    998398 |  747 | `		return SXRET_OK;` |
|         - |  748 | `	}` |
|   4098124 |  749 | `	if( pBlob->mByte > 0 ){` |
|    518442 |  750 | `		nByte = nByte + pBlob->mByte * 2 + SXBLOB_MIN_GROWTH;` |
|   3838892 |  751 | `	}else if ( nByte < SXBLOB_MIN_GROWTH ){` |
|   3023003 |  752 | `		nByte = SXBLOB_MIN_GROWTH;` |
|   1511409 |  753 | `	}` |
|   4098124 |  754 | `	pNew = SyMemBackendRealloc(pBlob->pAllocator,pBlob->pBlob,nByte);` |
|   4098124 |  755 | `	if( pNew == 0 ){` |
|       ! 0 |  756 | `		return SXERR_MEM;` |
|         - |  757 | `	}` |
|   4098124 |  758 | `	pBlob->pBlob = pNew;` |
|   4098124 |  759 | `	pBlob->mByte = nByte;` |
|   4098124 |  760 | `	return SXRET_OK;` |
|   2738570 |  761 |  |
|   5514772 |  762 | `PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize)` |
|         2 |  763 |  |
|         - |  764 | `	sxu8 *zBlob;` |
|         - |  765 | `	sxi32 rc;` |
|   5514774 |  766 | `	if( nSize < 1 ){` |
|     37726 |  767 | `		return SXRET_OK;` |
|         - |  768 | `	}` |
|   5477050 |  769 | `	rc = BlobPrepareGrow(&(*pBlob),&nSize);` |
|   5477050 |  770 | `	if( SXRET_OK != rc ){` |
|       ! 0 |  771 | `		return rc;` |
|         - |  772 | `	}` |
|   5477050 |  773 | `	if( pData ){` |
|   5477018 |  774 | `		zBlob = (sxu8 *)pBlob->pBlob ;` |
|   5477018 |  775 | `		zBlob = &zBlob[pBlob->nByte];` |
|   5477018 |  776 | `		pBlob->nByte += nSize;` |
|  23167988 |  777 | `		SX_MACRO_FAST_MEMCPY(pData,zBlob,nSize);` |
|   2738552 |  778 | `	}` |
|   5477050 |  779 | `	return SXRET_OK;` |
|   2757432 |  780 |  |
|    454500 |  781 | `PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob)` |
|         2 |  782 |  |
|         - |  783 | `	sxi32 rc;` |
|         - |  784 | `	sxu32 n;` |
|    454502 |  785 | `	n = pBlob->nByte;` |
|    454502 |  786 | `	rc = SyBlobAppend(&(*pBlob),(const void *)"\0",sizeof(char));` |
|    454502 |  787 | `	if (rc == SXRET_OK ){` |
|    454502 |  788 | `		pBlob->nByte = n;` |
|    227272 |  789 | `	}` |
|    454502 |  790 | `	return rc;` |
|         2 |  791 |  |
|   3157390 |  792 | `PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest)` |
|         2 |  793 |  |
|   3157392 |  794 | `	sxi32 rc = SXRET_OK;` |
|         - |  795 | `#ifdef UNTRUST` |
|         - |  796 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|         - |  797 | `		return SXERR_EMPTY;` |
|         - |  798 | `	}` |
|         - |  799 | `#endif` |
|   3157392 |  800 | `	if( pSrc->nByte > 0 ){` |
|   3157392 |  801 | `		rc = SyBlobAppend(&(*pDest),pSrc->pBlob,pSrc->nByte);` |
|   1578695 |  802 | `	}` |
|   3157392 |  803 | `	return rc;` |
|         2 |  804 |  |
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
|   3308960 |  824 | `PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob)` |
|         2 |  825 |  |
|   3308962 |  826 | `	pBlob->nByte = 0;` |
|   3308962 |  827 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|      5344 |  828 | `		pBlob->pBlob = 0;` |
|      5344 |  829 | `		pBlob->mByte = 0;` |
|      5344 |  830 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|      2671 |  831 | `	}` |
|   3308962 |  832 | `	return SXRET_OK;` |
|         2 |  833 |  |
|   8374222 |  834 | `PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob)` |
|         2 |  835 |  |
|   8374224 |  836 | `	if( (pBlob->nFlags & (SXBLOB_STATIC\|SXBLOB_RDONLY)) == 0 && pBlob->mByte > 0 ){` |
|   3867206 |  837 | `		SyMemBackendFree(pBlob->pAllocator,pBlob->pBlob);` |
|   1933624 |  838 | `	}` |
|   8374224 |  839 | `	pBlob->pBlob = 0;` |
|   8374224 |  840 | `	pBlob->nByte = pBlob->mByte = 0;` |
|   8374224 |  841 | `	pBlob->nFlags = 0;` |
|   8374224 |  842 | `	return SXRET_OK;` |
|         2 |  843 |  |
|         - |  844 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     97526 |  845 | `PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft)` |
|         2 |  846 |  |
|     97528 |  847 | `	const char *zIn = (const char *)pBlob;` |
|         - |  848 | `	const char *zEnd;` |
|         - |  849 | `	sxi32 rc;` |
|     97528 |  850 | `	if( pLen > nLen ){` |
|      3642 |  851 | `		return SXERR_NOTFOUND;` |
|         - |  852 | `	}` |
|     93888 |  853 | `	zEnd = &zIn[nLen-pLen];` |
|    780524 |  854 | `	for(;;){` |
|   1561000 |  855 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   1536820 |  856 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   1504268 |  857 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   1483242 |  858 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|         2 |  859 | `	}` |
|     14134 |  860 | `	return SXERR_NOTFOUND;` |
|     48765 |  861 |  |
|         - |  862 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - |  863 |  |
