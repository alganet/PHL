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
|   4086810 |   18 | `static void * SyOSHeapAlloc(sxu32 nByte)` |
|         2 |   19 |  |
|         - |   20 | `	void *pNew;` |
|         - |   21 | `#if defined(__WINNT__)` |
|         2 |   22 | `	pNew = HeapAlloc(GetProcessHeap(),0,nByte);` |
|         - |   23 | `#else` |
|   4086810 |   24 | `	pNew = malloc((size_t)nByte);` |
|         - |   25 | `#endif` |
|   4086812 |   26 | `	return pNew;` |
|         2 |   27 |  |
|    534139 |   28 | `static void * SyOSHeapRealloc(void *pOld,sxu32 nByte)` |
|         2 |   29 |  |
|         - |   30 | `	void *pNew;` |
|         - |   31 | `#if defined(__WINNT__)` |
|         2 |   32 | `	pNew = HeapReAlloc(GetProcessHeap(),0,pOld,nByte);` |
|         - |   33 | `#else` |
|    534139 |   34 | `	pNew = realloc(pOld,(size_t)nByte);` |
|         - |   35 | `#endif` |
|    534141 |   36 | `	return pNew;` |
|         2 |   37 |  |
|   4077592 |   38 | `static void SyOSHeapFree(void *pPtr)` |
|         2 |   39 |  |
|         - |   40 | `#if defined(__WINNT__)` |
|         2 |   41 | `	HeapFree(GetProcessHeap(),0,pPtr);` |
|         - |   42 | `#else` |
|   4077592 |   43 | `	free(pPtr);` |
|         - |   44 | `#endif` |
|   4077594 |   45 |  |
|         - |   46 |  |
|         - |   47 |  |
|   7495776 |   48 | `PH7_PRIVATE void SyZero(void *pSrc,sxu32 nSize)` |
|         2 |   49 |  |
|   7495778 |   50 | `	register unsigned char *zSrc = (unsigned char *)pSrc;` |
|         - |   51 | `	unsigned char *zEnd;` |
|         - |   52 | `#if defined(UNTRUST)` |
|         - |   53 | `	if( zSrc == 0 \|\| nSize <= 0 ){` |
|         - |   54 | `		return ;` |
|         - |   55 | `	}` |
|         - |   56 | `#endif` |
|   7495778 |   57 | `	zEnd = &zSrc[nSize];` |
|  89251236 |   58 | `	for(;;){` |
| 178500308 |   59 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 171004536 |   60 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 171004534 |   61 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 171004534 |   62 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
|         2 |   63 | `	}` |
|   7495778 |   64 |  |
|  10027908 |   65 | `PH7_PRIVATE sxi32 SyMemcmp(const void *pB1,const void *pB2,sxu32 nSize)` |
|         2 |   66 |  |
|         - |   67 | `	sxi32 rc;` |
|  10027910 |   68 | `	if( nSize <= 0 ){` |
|        92 |   69 | `		return 0;` |
|         - |   70 | `	}` |
|  10027820 |   71 | `	if( pB1 == 0 \|\| pB2 == 0 ){` |
|       ! 0 |   72 | `		return pB1 != 0 ? 1 : (pB2 == 0 ? 0 : -1);` |
|         - |   73 | `	}` |
|  21195049 |   74 | `	SX_MACRO_FAST_CMP(pB1,pB2,nSize,rc);` |
|  10027820 |   75 | `	return rc;` |
|   5014393 |   76 |  |
|   5235028 |   77 | `PH7_PRIVATE sxu32 SyMemcpy(const void *pSrc,void *pDest,sxu32 nLen)` |
|         2 |   78 |  |
|         - |   79 | `#if defined(UNTRUST)` |
|         - |   80 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|         - |   81 | `		return 0;` |
|         - |   82 | `	}` |
|         - |   83 | `#endif` |
|   5235030 |   84 | `	if( pSrc == (const void *)pDest ){` |
|       ! 0 |   85 | `		return nLen;` |
|         - |   86 | `	}` |
|  42812416 |   87 | `	SX_MACRO_FAST_MEMCPY(pSrc,pDest,nLen);` |
|   5235030 |   88 | `	return nLen;` |
|   2617705 |   89 |  |
|   4086810 |   90 | `static void * MemOSAlloc(sxu32 nBytes)` |
|         2 |   91 |  |
|         - |   92 | `	sxu32 *pChunk;` |
|   4086812 |   93 | `	pChunk = (sxu32 *)SyOSHeapAlloc(nBytes + sizeof(sxu32));` |
|   4086812 |   94 | `	if( pChunk == 0 ){` |
|       ! 0 |   95 | `		return 0;` |
|         - |   96 | `	}` |
|   4086812 |   97 | `	pChunk[0] = nBytes;` |
|   4086812 |   98 | `	return (void *)&pChunk[1];` |
|   2043429 |   99 |  |
|    534139 |  100 | `static void * MemOSRealloc(void *pOld,sxu32 nBytes)` |
|         2 |  101 |  |
|         - |  102 | `	sxu32 *pOldChunk;` |
|         - |  103 | `	sxu32 *pChunk;` |
|    534141 |  104 | `	pOldChunk = (sxu32 *)(((char *)pOld)-sizeof(sxu32));` |
|    534141 |  105 | `	if( pOldChunk[0] >= nBytes ){` |
|       ! 0 |  106 | `		return pOld;` |
|         - |  107 | `	}` |
|    534141 |  108 | `	pChunk = (sxu32 *)SyOSHeapRealloc(pOldChunk,nBytes + sizeof(sxu32));` |
|    534141 |  109 | `	if( pChunk == 0 ){` |
|       ! 0 |  110 | `		return 0;` |
|         - |  111 | `	}` |
|    534141 |  112 | `	pChunk[0] = nBytes;` |
|    534141 |  113 | `	return (void *)&pChunk[1];` |
|    267058 |  114 |  |
|   4077592 |  115 | `static void MemOSFree(void *pBlock)` |
|         2 |  116 |  |
|         - |  117 | `	void *pChunk;` |
|   4077594 |  118 | `	pChunk = (void *)(((char *)pBlock)-sizeof(sxu32));` |
|   4077594 |  119 | `	SyOSHeapFree(pChunk);` |
|   4077594 |  120 |  |
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
|   4086810 |  137 | `static void * MemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         2 |  138 |  |
|         - |  139 | `	SyMemBlock *pBlock;` |
|   4086812 |  140 | `	sxi32 nRetry = 0;` |
|         - |  141 |  |
|         - |  142 | `	/* Append an extra block so we can tracks allocated chunks and avoid memory` |
|         - |  143 | `	 * leaks.` |
|         - |  144 | `	 */` |
|   4086812 |  145 | `	nByte += sizeof(SyMemBlock);` |
|   2043427 |  146 | `	for(;;){` |
|   2043429 |  147 | `		pBlock = (SyMemBlock *)pBackend->pMethods->xAlloc(nByte);` |
|   4086810 |  148 | `		if( pBlock != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY` |
|         2 |  149 | `			\|\| SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|   2043429 |  150 | `				break;` |
|         - |  151 | `		}` |
|       ! 0 |  152 | `		nRetry++;` |
|       ! 0 |  153 | `	}` |
|   4086812 |  154 | `	if( pBlock  == 0 ){` |
|       ! 0 |  155 | `		return 0;` |
|         - |  156 | `	}` |
|   4086812 |  157 | `	pBlock->pNext = pBlock->pPrev = 0;` |
|         - |  158 | `	/* Link to the list of already tracked blocks */` |
|   4086812 |  159 | `	MACRO_LD_PUSH(pBackend->pBlocks,pBlock);` |
|         - |  160 | `#if defined(UNTRUST)` |
|         - |  161 | `	pBlock->nGuard = SXMEM_BACKEND_MAGIC;` |
|         - |  162 | `#endif` |
|   4086812 |  163 | `	pBackend->nBlock++;` |
|   4086812 |  164 | `	return (void *)&pBlock[1];` |
|   2043429 |  165 |  |
|   1842194 |  166 | `PH7_PRIVATE void * SyMemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         2 |  167 |  |
|         - |  168 | `	void *pChunk;` |
|         - |  169 | `#if defined(UNTRUST)` |
|         - |  170 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  171 | `		return 0;` |
|         - |  172 | `	}` |
|         - |  173 | `#endif` |
|   1842196 |  174 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  175 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  176 | `	}` |
|   1842196 |  177 | `	pChunk = MemBackendAlloc(&(*pBackend),nByte);` |
|   1842196 |  178 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  179 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  180 | `	}` |
|   1842196 |  181 | `	return pChunk;` |
|         2 |  182 |  |
|   2764177 |  183 | `static void * MemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         2 |  184 |  |
|         - |  185 | `	SyMemBlock *pBlock,*pNew,*pPrev,*pNext;` |
|   2764179 |  186 | `	sxu32 nRetry = 0;` |
|         - |  187 |  |
|   2764179 |  188 | `	if( pOld == 0 ){` |
|   2230040 |  189 | `		return MemBackendAlloc(&(*pBackend),nByte);` |
|         - |  190 | `	}` |
|    534141 |  191 | `	pBlock = (SyMemBlock *)(((char *)pOld) - sizeof(SyMemBlock));` |
|         - |  192 | `#if defined(UNTRUST)` |
|         - |  193 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|         - |  194 | `		return 0;` |
|         - |  195 | `	}` |
|         - |  196 | `#endif` |
|    534141 |  197 | `	nByte += sizeof(SyMemBlock);` |
|    534141 |  198 | `	pPrev = pBlock->pPrev;` |
|    534141 |  199 | `	pNext = pBlock->pNext;` |
|    267056 |  200 | `	for(;;){` |
|    267058 |  201 | `		pNew = (SyMemBlock *)pBackend->pMethods->xRealloc(pBlock,nByte);` |
|    534141 |  202 | `		if( pNew != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY \|\|` |
|       ! 0 |  203 | `			SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|    267058 |  204 | `				break;` |
|         - |  205 | `		}` |
|       ! 0 |  206 | `		nRetry++;` |
|       ! 0 |  207 | `	}` |
|    534141 |  208 | `	if( pNew == 0 ){` |
|       ! 0 |  209 | `		return 0;` |
|         - |  210 | `	}` |
|    534141 |  211 | `	if( pNew != pBlock ){` |
|    478478 |  212 | `		if( pPrev == 0 ){` |
|    386361 |  213 | `			pBackend->pBlocks = pNew;` |
|    212978 |  214 | `		}else{` |
|     92119 |  215 | `			pPrev->pNext = pNew;` |
|         - |  216 | `		}` |
|    478478 |  217 | `		if( pNext ){` |
|    478470 |  218 | `			pNext->pPrev = pNew;` |
|    259027 |  219 | `		}` |
|         - |  220 | `#if defined(UNTRUST)` |
|         - |  221 | `		pNew->nGuard = SXMEM_BACKEND_MAGIC;` |
|         - |  222 | `#endif` |
|    259031 |  223 | `	}` |
|    534141 |  224 | `	return (void *)&pNew[1];` |
|   1382099 |  225 |  |
|   2764177 |  226 | `PH7_PRIVATE void * SyMemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         2 |  227 |  |
|         - |  228 | `	void *pChunk;` |
|         - |  229 | `#if defined(UNTRUST)` |
|         - |  230 | `	if( SXMEM_BACKEND_CORRUPT(pBackend)  ){` |
|         - |  231 | `		return 0;` |
|         - |  232 | `	}` |
|         - |  233 | `#endif` |
|   2764179 |  234 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  235 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  236 | `	}` |
|   2764179 |  237 | `	pChunk = MemBackendRealloc(&(*pBackend),pOld,nByte);` |
|   2764179 |  238 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  239 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  240 | `	}` |
|   2764179 |  241 | `	return pChunk;` |
|         2 |  242 |  |
|   2823348 |  243 | `static sxi32 MemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|         2 |  244 |  |
|         - |  245 | `	SyMemBlock *pBlock;` |
|   2823350 |  246 | `	pBlock = (SyMemBlock *)(((char *)pChunk) - sizeof(SyMemBlock));` |
|         - |  247 | `#if defined(UNTRUST)` |
|         - |  248 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|         - |  249 | `		return SXERR_CORRUPT;` |
|         - |  250 | `	}` |
|         - |  251 | `#endif` |
|         - |  252 | `	/* Unlink from the list of active blocks */` |
|   2823350 |  253 | `	if( pBackend->nBlock > 0 ){` |
|         - |  254 | `		/* Release the block */` |
|         - |  255 | `#if defined(UNTRUST)` |
|         - |  256 | `		/* Mark as stale block */` |
|         - |  257 | `		pBlock->nGuard = 0x635B;` |
|         - |  258 | `#endif` |
|   2823350 |  259 | `		MACRO_LD_REMOVE(pBackend->pBlocks,pBlock);` |
|   2823350 |  260 | `		pBackend->nBlock--;` |
|   2823350 |  261 | `		pBackend->pMethods->xFree(pBlock);` |
|   1411696 |  262 | `	}` |
|   2823350 |  263 | `	return SXRET_OK;` |
|         2 |  264 |  |
|   2823348 |  265 | `PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|         2 |  266 |  |
|         - |  267 | `	sxi32 rc;` |
|         - |  268 | `#if defined(UNTRUST)` |
|         - |  269 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  270 | `		return SXERR_CORRUPT;` |
|         - |  271 | `	}` |
|         - |  272 | `#endif` |
|   2823350 |  273 | `	if( pChunk == 0 ){` |
|       ! 0 |  274 | `		return SXRET_OK;` |
|         - |  275 | `	}` |
|   2823350 |  276 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  277 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  278 | `	}` |
|   2823350 |  279 | `	rc = MemBackendFree(&(*pBackend),pChunk);` |
|   2823350 |  280 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  281 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  282 | `	}` |
|   2823350 |  283 | `	return rc;` |
|   1411698 |  284 |  |
|         - |  285 | `#if defined(PH7_ENABLE_THREADS)` |
|      1376 |  286 | `PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods)` |
|         2 |  287 |  |
|         - |  288 | `	SyMutex *pMutex;` |
|         - |  289 | `#if defined(UNTRUST)` |
|         - |  290 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pMethods == 0 \|\| pMethods->xNew == 0){` |
|         - |  291 | `		return SXERR_CORRUPT;` |
|         - |  292 | `	}` |
|         - |  293 | `#endif` |
|      1378 |  294 | `	pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|      1378 |  295 | `	if( pMutex == 0 ){` |
|       ! 0 |  296 | `		return SXERR_OS;` |
|         - |  297 | `	}` |
|         - |  298 | `	/* Attach the mutex to the memory backend */` |
|      1378 |  299 | `	pBackend->pMutex = pMutex;` |
|      1378 |  300 | `	pBackend->pMutexMethods = pMethods;` |
|      1378 |  301 | `	return SXRET_OK;` |
|       690 |  302 |  |
|      1376 |  303 | `PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend)` |
|         2 |  304 |  |
|         - |  305 | `#if defined(UNTRUST)` |
|         - |  306 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  307 | `		return SXERR_CORRUPT;` |
|         - |  308 | `	}` |
|         - |  309 | `#endif` |
|      1378 |  310 | `	if( pBackend->pMutex == 0 ){` |
|         - |  311 | `		/* There is no mutex subsystem at all */` |
|       ! 0 |  312 | `		return SXRET_OK;` |
|         - |  313 | `	}` |
|      1378 |  314 | `	SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1378 |  315 | `	pBackend->pMutexMethods = 0;` |
|      1378 |  316 | `	pBackend->pMutex = 0;` |
|      1378 |  317 | `	return SXRET_OK;` |
|       690 |  318 |  |
|         - |  319 | `#endif` |
|         - |  320 | `/*` |
|         - |  321 | ` * Memory pool allocator` |
|         - |  322 | ` */` |
|         - |  323 | `#define SXMEM_POOL_MAGIC		0xDEAD` |
|         - |  324 | `#define SXMEM_POOL_MAXALLOC		(1<<(SXMEM_POOL_NBUCKETS+SXMEM_POOL_INCR))` |
|         - |  325 | `#define SXMEM_POOL_MINALLOC		(1<<(SXMEM_POOL_INCR))` |
|     14578 |  326 | `static sxi32 MemPoolBucketAlloc(SyMemBackend *pBackend,sxu32 nBucket)` |
|         2 |  327 |  |
|         - |  328 | `	char *zBucket,*zBucketEnd;` |
|         - |  329 | `	SyMemHeader *pHeader;` |
|         - |  330 | `	sxu32 nBucketSize;` |
|         - |  331 |  |
|         - |  332 | `	/* Allocate one big block first */` |
|     14580 |  333 | `	zBucket = (char *)MemBackendAlloc(&(*pBackend),SXMEM_POOL_MAXALLOC);` |
|     14580 |  334 | `	if( zBucket == 0 ){` |
|       ! 0 |  335 | `		return SXERR_MEM;` |
|         - |  336 | `	}` |
|     14580 |  337 | `	zBucketEnd = &zBucket[SXMEM_POOL_MAXALLOC];` |
|         - |  338 | `	/* Divide the big block into mini bucket pool */` |
|     14580 |  339 | `	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);` |
|     14580 |  340 | `	pBackend->apPool[nBucket] = pHeader = (SyMemHeader *)zBucket;` |
|   1528000 |  341 | `	for(;;){` |
|   3056002 |  342 | `		if( &zBucket[nBucketSize] >= zBucketEnd ){` |
|     14580 |  343 | `			break;` |
|         - |  344 | `		}` |
|   3041424 |  345 | `		pHeader->pNext = (SyMemHeader *)&zBucket[nBucketSize];` |
|         - |  346 | `		/* Advance the cursor to the next available chunk */` |
|   3041424 |  347 | `		pHeader = pHeader->pNext;` |
|   3041424 |  348 | `		zBucket += nBucketSize;` |
|         2 |  349 | `	}` |
|     14580 |  350 | `	pHeader->pNext = 0;` |
|         - |  351 |  |
|     14580 |  352 | `	return SXRET_OK;` |
|      7291 |  353 |  |
|   4756486 |  354 | `static void * MemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         2 |  355 |  |
|         - |  356 | `	SyMemHeader *pBucket,*pNext;` |
|         - |  357 | `	sxu32 nBucketSize;` |
|         - |  358 | `	sxu32 nBucket;` |
|         - |  359 |  |
|   4756488 |  360 | `	if( nByte + sizeof(SyMemHeader) >= SXMEM_POOL_MAXALLOC ){` |
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
|   4756488 |  371 | `	nBucket = 0;` |
|   4756488 |  372 | `	nBucketSize = SXMEM_POOL_MINALLOC;` |
|  23648370 |  373 | `	while( nByte + sizeof(SyMemHeader) > nBucketSize  ){` |
|  18891884 |  374 | `		nBucketSize <<= 1;` |
|  18891884 |  375 | `		nBucket++;` |
|         2 |  376 | `	}` |
|   4756488 |  377 | `	pBucket = pBackend->apPool[nBucket];` |
|   4756488 |  378 | `	if( pBucket == 0 ){` |
|         - |  379 | `		sxi32 rc;` |
|     14580 |  380 | `		rc = MemPoolBucketAlloc(&(*pBackend),nBucket);` |
|     14580 |  381 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  382 | `			return 0;` |
|         - |  383 | `		}` |
|     14580 |  384 | `		pBucket = pBackend->apPool[nBucket];` |
|      7289 |  385 | `	}` |
|         - |  386 | `	/* Remove from the free list */` |
|   4756488 |  387 | `	pNext = pBucket->pNext;` |
|   4756488 |  388 | `	pBackend->apPool[nBucket] = pNext;` |
|         - |  389 | `	/* Record bucket&magic number */` |
|   4756488 |  390 | `	pBucket->nBucket = (SXMEM_POOL_MAGIC << 16) \| nBucket;` |
|   4756488 |  391 | `	return (void *)&pBucket[1];` |
|   2378245 |  392 |  |
|   4756486 |  393 | `PH7_PRIVATE void * SyMemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         2 |  394 |  |
|         - |  395 | `	void *pChunk;` |
|         - |  396 | `#if defined(UNTRUST)` |
|         - |  397 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  398 | `		return 0;` |
|         - |  399 | `	}` |
|         - |  400 | `#endif` |
|   4756488 |  401 | `	if( pBackend->pMutexMethods ){` |
|      1378 |  402 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       688 |  403 | `	}` |
|   4756488 |  404 | `	pChunk = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|   4756488 |  405 | `	if( pBackend->pMutexMethods ){` |
|      1378 |  406 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       688 |  407 | `	}` |
|   4756488 |  408 | `	return pChunk;` |
|         2 |  409 |  |
|   2649480 |  410 | `static sxi32 MemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|         2 |  411 |  |
|         - |  412 | `	SyMemHeader *pHeader;` |
|         - |  413 | `	sxu32 nBucket;` |
|         - |  414 | `	/* Get the corresponding bucket */` |
|   2649482 |  415 | `	pHeader = (SyMemHeader *)(((char *)pChunk) - sizeof(SyMemHeader));` |
|         - |  416 | `	/* Sanity check to avoid misuse */` |
|   2649482 |  417 | `	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){` |
|       ! 0 |  418 | `		return SXERR_CORRUPT;` |
|         - |  419 | `	}` |
|   2649482 |  420 | `	nBucket = pHeader->nBucket & 0xFFFF;` |
|   2649482 |  421 | `	if( nBucket == SXU16_HIGH ){` |
|         - |  422 | `		/* Free the big block */` |
|       ! 0 |  423 | `		MemBackendFree(&(*pBackend),pHeader);` |
|       ! 0 |  424 | `	}else{` |
|         - |  425 | `		/* Return to the free list */` |
|   2649482 |  426 | `		pHeader->pNext = pBackend->apPool[nBucket & 0x0f];` |
|   2649482 |  427 | `		pBackend->apPool[nBucket & 0x0f] = pHeader;` |
|         - |  428 | `	}` |
|   2649482 |  429 | `	return SXRET_OK;` |
|   1324742 |  430 |  |
|   2649480 |  431 | `PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|         2 |  432 |  |
|         - |  433 | `	sxi32 rc;` |
|         - |  434 | `#if defined(UNTRUST)` |
|         - |  435 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pChunk == 0 ){` |
|         - |  436 | `		return SXERR_CORRUPT;` |
|         - |  437 | `	}` |
|         - |  438 | `#endif` |
|   2649482 |  439 | `	if( pBackend->pMutexMethods ){` |
|      1112 |  440 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       555 |  441 | `	}` |
|   2649482 |  442 | `	rc = MemBackendPoolFree(&(*pBackend),pChunk);` |
|   2649482 |  443 | `	if( pBackend->pMutexMethods ){` |
|      1112 |  444 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       555 |  445 | `	}` |
|   2649482 |  446 | `	return rc;` |
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
|      1376 |  505 | `PH7_PRIVATE sxi32 SyMemBackendInit(SyMemBackend *pBackend,ProcMemError xMemErr,void * pUserData)` |
|         2 |  506 |  |
|         - |  507 | `#if defined(UNTRUST)` |
|         - |  508 | `	if( pBackend == 0 ){` |
|         - |  509 | `		return SXERR_EMPTY;` |
|         - |  510 | `	}` |
|         - |  511 | `#endif` |
|         - |  512 | `	/* Zero the allocator first */` |
|      1378 |  513 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|      1378 |  514 | `	pBackend->xMemError = xMemErr;` |
|      1378 |  515 | `	pBackend->pUserData = pUserData;` |
|         - |  516 | `	/* Switch to the OS memory allocator */` |
|      1378 |  517 | `	pBackend->pMethods = &sOSAllocMethods;` |
|      1378 |  518 | `	if( pBackend->pMethods->xInit ){` |
|         - |  519 | `		/* Initialize the backend  */` |
|       ! 0 |  520 | `		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){` |
|       ! 0 |  521 | `			return SXERR_ABORT;` |
|         - |  522 | `		}` |
|       ! 0 |  523 | `	}` |
|         - |  524 | `#if defined(UNTRUST)` |
|         - |  525 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|         - |  526 | `#endif` |
|      1378 |  527 | `	return SXRET_OK;` |
|       690 |  528 |  |
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
|      2752 |  557 | `PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend *pBackend,SyMemBackend *pParent)` |
|         2 |  558 |  |
|         - |  559 | `	sxu8 bInheritMutex;` |
|         - |  560 | `#if defined(UNTRUST)` |
|         - |  561 | `	if( pBackend == 0 \|\| SXMEM_BACKEND_CORRUPT(pParent) ){` |
|         - |  562 | `		return SXERR_CORRUPT;` |
|         - |  563 | `	}` |
|         - |  564 | `#endif` |
|         - |  565 | `	/* Zero the allocator first */` |
|      2754 |  566 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|      2754 |  567 | `	pBackend->pMethods  = pParent->pMethods;` |
|      2754 |  568 | `	pBackend->xMemError = pParent->xMemError;` |
|      2754 |  569 | `	pBackend->pUserData = pParent->pUserData;` |
|      2754 |  570 | `	bInheritMutex = pParent->pMutexMethods ? TRUE : FALSE;` |
|      2754 |  571 | `	if( bInheritMutex ){` |
|      1378 |  572 | `		pBackend->pMutexMethods = pParent->pMutexMethods;` |
|         - |  573 | `		/* Create a private mutex */` |
|      1378 |  574 | `		pBackend->pMutex = pBackend->pMutexMethods->xNew(SXMUTEX_TYPE_FAST);` |
|      1378 |  575 | `		if( pBackend->pMutex ==  0){` |
|       ! 0 |  576 | `			return SXERR_OS;` |
|         - |  577 | `		}` |
|       688 |  578 | `	}` |
|         - |  579 | `#if defined(UNTRUST)` |
|         - |  580 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|         - |  581 | `#endif` |
|      2754 |  582 | `	return SXRET_OK;` |
|      1378 |  583 |  |
|      2994 |  584 | `static sxi32 MemBackendRelease(SyMemBackend *pBackend)` |
|         2 |  585 |  |
|         - |  586 | `	SyMemBlock *pBlock,*pNext;` |
|         - |  587 |  |
|      2996 |  588 | `	pBlock = pBackend->pBlocks;` |
|    157885 |  589 | `	for(;;){` |
|    315772 |  590 | `		if( pBackend->nBlock == 0 ){` |
|       477 |  591 | `			break;` |
|         - |  592 | `		}` |
|    315296 |  593 | `		pNext  = pBlock->pNext;` |
|    315296 |  594 | `		pBackend->pMethods->xFree(pBlock);` |
|    315296 |  595 | `		pBlock = pNext;` |
|    315296 |  596 | `		pBackend->nBlock--;` |
|         - |  597 | `		/* LOOP ONE */` |
|    315296 |  598 | `		if( pBackend->nBlock == 0 ){` |
|      2098 |  599 | `			break;` |
|         - |  600 | `		}` |
|    313200 |  601 | `		pNext  = pBlock->pNext;` |
|    313200 |  602 | `		pBackend->pMethods->xFree(pBlock);` |
|    313200 |  603 | `		pBlock = pNext;` |
|    313200 |  604 | `		pBackend->nBlock--;` |
|         - |  605 | `		/* LOOP TWO */` |
|    313200 |  606 | `		if( pBackend->nBlock == 0 ){` |
|       223 |  607 | `			break;` |
|         - |  608 | `		}` |
|    312978 |  609 | `		pNext  = pBlock->pNext;` |
|    312978 |  610 | `		pBackend->pMethods->xFree(pBlock);` |
|    312978 |  611 | `		pBlock = pNext;` |
|    312978 |  612 | `		pBackend->nBlock--;` |
|         - |  613 | `		/* LOOP THREE */` |
|    312978 |  614 | `		if( pBackend->nBlock == 0 ){` |
|       201 |  615 | `			break;` |
|         - |  616 | `		}` |
|    312778 |  617 | `		pNext  = pBlock->pNext;` |
|    312778 |  618 | `		pBackend->pMethods->xFree(pBlock);` |
|    312778 |  619 | `		pBlock = pNext;` |
|    312778 |  620 | `		pBackend->nBlock--;` |
|         - |  621 | `		/* LOOP FOUR */` |
|         2 |  622 | `	}` |
|      2996 |  623 | `	if( pBackend->pMethods->xRelease ){` |
|       ! 0 |  624 | `		pBackend->pMethods->xRelease(pBackend->pMethods->pUserData);` |
|       ! 0 |  625 | `	}` |
|      2996 |  626 | `	pBackend->pMethods = 0;` |
|      2996 |  627 | `	pBackend->pBlocks  = 0;` |
|         - |  628 | `#if defined(UNTRUST)` |
|         - |  629 | `	pBackend->nMagic = 0x2626;` |
|         - |  630 | `#endif` |
|      2996 |  631 | `	return SXRET_OK;` |
|         2 |  632 |  |
|      2994 |  633 | `PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend *pBackend)` |
|         2 |  634 |  |
|         - |  635 | `#if defined(UNTRUST)` |
|         - |  636 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  637 | `		return SXERR_INVALID;` |
|         - |  638 | `	}` |
|         - |  639 | `#endif` |
|      2996 |  640 | `	if( pBackend->pMutexMethods ){` |
|       259 |  641 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       129 |  642 | `	}` |
|      2996 |  643 | `	(void)MemBackendRelease(&(*pBackend));` |
|      2996 |  644 | `	if( pBackend->pMutexMethods ){` |
|       259 |  645 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       259 |  646 | `		SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|       129 |  647 | `	}` |
|      2996 |  648 | `	return SXRET_OK;` |
|         2 |  649 |  |
|    417792 |  650 | `PH7_PRIVATE void * SyMemBackendDup(SyMemBackend *pBackend,const void *pSrc,sxu32 nSize)` |
|         2 |  651 |  |
|         - |  652 | `	void *pNew;` |
|         - |  653 | `#if defined(UNTRUST)` |
|         - |  654 | `	if( pSrc == 0 \|\| nSize <= 0 ){` |
|         - |  655 | `		return 0;` |
|         - |  656 | `	}` |
|         - |  657 | `#endif` |
|    417794 |  658 | `	pNew = SyMemBackendAlloc(&(*pBackend),nSize);` |
|    417794 |  659 | `	if( pNew ){` |
|    417794 |  660 | `		SyMemcpy(pSrc,pNew,nSize);` |
|    208896 |  661 | `	}` |
|    417794 |  662 | `	return pNew;` |
|         2 |  663 |  |
|    866892 |  664 | `PH7_PRIVATE char * SyMemBackendStrDup(SyMemBackend *pBackend,const char *zSrc,sxu32 nSize)` |
|         2 |  665 |  |
|         - |  666 | `	char *zDest;` |
|    866894 |  667 | `	zDest = (char *)SyMemBackendAlloc(&(*pBackend),nSize + 1);` |
|    866894 |  668 | `	if( zDest ){` |
|    866894 |  669 | `		Systrcpy(zDest,nSize+1,zSrc,nSize);` |
|    433446 |  670 | `	}` |
|    866894 |  671 | `	return zDest;` |
|         2 |  672 |  |
|     40282 |  673 | `PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize)` |
|         2 |  674 |  |
|         - |  675 | `#if defined(UNTRUST)` |
|         - |  676 | `	if( pBlob == 0 \|\| pBuffer == 0 \|\| nSize < 1 ){` |
|         - |  677 | `		return SXERR_EMPTY;` |
|         - |  678 | `	}` |
|         - |  679 | `#endif` |
|     40284 |  680 | `	pBlob->pBlob = pBuffer;` |
|     40284 |  681 | `	pBlob->mByte = nSize;` |
|     40284 |  682 | `	pBlob->nByte = 0;` |
|     40284 |  683 | `	pBlob->pAllocator = 0;` |
|     40284 |  684 | `	pBlob->nFlags = SXBLOB_LOCKED\|SXBLOB_STATIC;` |
|     40284 |  685 | `	return SXRET_OK;` |
|         2 |  686 |  |
|   2901514 |  687 | `PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator)` |
|         2 |  688 |  |
|         - |  689 | `#if defined(UNTRUST)` |
|         - |  690 | `	if( pBlob == 0  ){` |
|         - |  691 | `		return SXERR_EMPTY;` |
|         - |  692 | `	}` |
|         - |  693 | `#endif` |
|   2901516 |  694 | `	pBlob->pBlob = 0;` |
|   2901516 |  695 | `	pBlob->mByte = pBlob->nByte	= 0;` |
|   2901516 |  696 | `	pBlob->pAllocator = &(*pAllocator);` |
|   2901516 |  697 | `	pBlob->nFlags = 0;` |
|   2901516 |  698 | `	return SXRET_OK;` |
|         2 |  699 |  |
|   1837268 |  700 | `PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte)` |
|         2 |  701 |  |
|         - |  702 | `#if defined(UNTRUST)` |
|         - |  703 | `	if( pBlob == 0  ){` |
|         - |  704 | `		return SXERR_EMPTY;` |
|         - |  705 | `	}` |
|         - |  706 | `#endif` |
|   1837270 |  707 | `	pBlob->pBlob = (void *)pData;` |
|   1837270 |  708 | `	pBlob->nByte = nByte;` |
|   1837270 |  709 | `	pBlob->mByte = 0;` |
|   1837270 |  710 | `	pBlob->nFlags \|= SXBLOB_RDONLY;` |
|   1837270 |  711 | `	return SXRET_OK;` |
|         2 |  712 |  |
|         - |  713 | `#ifndef SXBLOB_MIN_GROWTH` |
|         - |  714 | `#define SXBLOB_MIN_GROWTH 16` |
|         - |  715 | `#endif` |
|   2996736 |  716 | `static sxi32 BlobPrepareGrow(SyBlob *pBlob,sxu32 *pByte)` |
|         2 |  717 |  |
|         - |  718 | `	sxu32 nByte;` |
|         - |  719 | `	void *pNew;` |
|   2996738 |  720 | `	nByte = *pByte;` |
|   2996738 |  721 | `	if( pBlob->nFlags & (SXBLOB_LOCKED\|SXBLOB_STATIC) ){` |
|    321564 |  722 | `		if ( SyBlobFreeSpace(pBlob) < nByte ){` |
|       ! 0 |  723 | `			*pByte = SyBlobFreeSpace(pBlob);` |
|       ! 0 |  724 | `			if( (*pByte) == 0 ){` |
|       ! 0 |  725 | `				return SXERR_SHORT;` |
|         - |  726 | `			}` |
|       ! 0 |  727 | `		}` |
|    321564 |  728 | `		return SXRET_OK;` |
|         - |  729 | `	}` |
|   2675176 |  730 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|         - |  731 | `		/* Make a copy of the read-only item */` |
|    417794 |  732 | `		if( pBlob->nByte > 0 ){` |
|    417794 |  733 | `			pNew = SyMemBackendDup(pBlob->pAllocator,pBlob->pBlob,pBlob->nByte);` |
|    417794 |  734 | `			if( pNew == 0 ){` |
|       ! 0 |  735 | `				return SXERR_MEM;` |
|         - |  736 | `			}` |
|    417794 |  737 | `			pBlob->pBlob = pNew;` |
|    417794 |  738 | `			pBlob->mByte = pBlob->nByte;` |
|    208898 |  739 | `		}else{` |
|       ! 0 |  740 | `			pBlob->pBlob = 0;` |
|       ! 0 |  741 | `			pBlob->mByte = 0;` |
|         - |  742 | `		}` |
|         - |  743 | `		/* Remove the read-only flag */` |
|    417794 |  744 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|    208896 |  745 | `	}` |
|   2675176 |  746 | `	if( SyBlobFreeSpace(pBlob) >= nByte ){` |
|    926963 |  747 | `		return SXRET_OK;` |
|         - |  748 | `	}` |
|   1748215 |  749 | `	if( pBlob->mByte > 0 ){` |
|    481569 |  750 | `		nByte = nByte + pBlob->mByte * 2 + SXBLOB_MIN_GROWTH;` |
|   1507418 |  751 | `	}else if ( nByte < SXBLOB_MIN_GROWTH ){` |
|    807684 |  752 | `		nByte = SXBLOB_MIN_GROWTH;` |
|    403748 |  753 | `	}` |
|   1748215 |  754 | `	pNew = SyMemBackendRealloc(pBlob->pAllocator,pBlob->pBlob,nByte);` |
|   1748215 |  755 | `	if( pNew == 0 ){` |
|       ! 0 |  756 | `		return SXERR_MEM;` |
|         - |  757 | `	}` |
|   1748215 |  758 | `	pBlob->pBlob = pNew;` |
|   1748215 |  759 | `	pBlob->mByte = nByte;` |
|   1748215 |  760 | `	return SXRET_OK;` |
|   1498414 |  761 |  |
|   3031504 |  762 | `PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize)` |
|         2 |  763 |  |
|         - |  764 | `	sxu8 *zBlob;` |
|         - |  765 | `	sxi32 rc;` |
|   3031506 |  766 | `	if( nSize < 1 ){` |
|     34770 |  767 | `		return SXRET_OK;` |
|         - |  768 | `	}` |
|   2996738 |  769 | `	rc = BlobPrepareGrow(&(*pBlob),&nSize);` |
|   2996738 |  770 | `	if( SXRET_OK != rc ){` |
|       ! 0 |  771 | `		return rc;` |
|         - |  772 | `	}` |
|   2996738 |  773 | `	if( pData ){` |
|   2996706 |  774 | `		zBlob = (sxu8 *)pBlob->pBlob ;` |
|   2996706 |  775 | `		zBlob = &zBlob[pBlob->nByte];` |
|   2996706 |  776 | `		pBlob->nByte += nSize;` |
|  18000524 |  777 | `		SX_MACRO_FAST_MEMCPY(pData,zBlob,nSize);` |
|   1498396 |  778 | `	}` |
|   2996738 |  779 | `	return SXRET_OK;` |
|   1515798 |  780 |  |
|    410194 |  781 | `PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob)` |
|         2 |  782 |  |
|         - |  783 | `	sxi32 rc;` |
|         - |  784 | `	sxu32 n;` |
|    410196 |  785 | `	n = pBlob->nByte;` |
|    410196 |  786 | `	rc = SyBlobAppend(&(*pBlob),(const void *)"\0",sizeof(char));` |
|    410196 |  787 | `	if (rc == SXRET_OK ){` |
|    410196 |  788 | `		pBlob->nByte = n;` |
|    205119 |  789 | `	}` |
|    410196 |  790 | `	return rc;` |
|         2 |  791 |  |
|    928620 |  792 | `PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest)` |
|         2 |  793 |  |
|    928622 |  794 | `	sxi32 rc = SXRET_OK;` |
|         - |  795 | `#ifdef UNTRUST` |
|         - |  796 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|         - |  797 | `		return SXERR_EMPTY;` |
|         - |  798 | `	}` |
|         - |  799 | `#endif` |
|    928622 |  800 | `	if( pSrc->nByte > 0 ){` |
|    928622 |  801 | `		rc = SyBlobAppend(&(*pDest),pSrc->pBlob,pSrc->nByte);` |
|    464310 |  802 | `	}` |
|    928622 |  803 | `	return rc;` |
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
|   1071760 |  824 | `PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob)` |
|         2 |  825 |  |
|   1071762 |  826 | `	pBlob->nByte = 0;` |
|   1071762 |  827 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|      4458 |  828 | `		pBlob->pBlob = 0;` |
|      4458 |  829 | `		pBlob->mByte = 0;` |
|      4458 |  830 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|      2228 |  831 | `	}` |
|   1071762 |  832 | `	return SXRET_OK;` |
|         2 |  833 |  |
|   5457822 |  834 | `PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob)` |
|         2 |  835 |  |
|   5457824 |  836 | `	if( (pBlob->nFlags & (SXBLOB_STATIC\|SXBLOB_RDONLY)) == 0 && pBlob->mByte > 0 ){` |
|   1546492 |  837 | `		SyMemBackendFree(pBlob->pAllocator,pBlob->pBlob);` |
|    773267 |  838 | `	}` |
|   5457824 |  839 | `	pBlob->pBlob = 0;` |
|   5457824 |  840 | `	pBlob->nByte = pBlob->mByte = 0;` |
|   5457824 |  841 | `	pBlob->nFlags = 0;` |
|   5457824 |  842 | `	return SXRET_OK;` |
|         2 |  843 |  |
|         - |  844 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     90352 |  845 | `PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft)` |
|         2 |  846 |  |
|     90354 |  847 | `	const char *zIn = (const char *)pBlob;` |
|         - |  848 | `	const char *zEnd;` |
|         - |  849 | `	sxi32 rc;` |
|     90354 |  850 | `	if( pLen > nLen ){` |
|      3262 |  851 | `		return SXERR_NOTFOUND;` |
|         - |  852 | `	}` |
|     87094 |  853 | `	zEnd = &zIn[nLen-pLen];` |
|    716040 |  854 | `	for(;;){` |
|   1432031 |  855 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   1409360 |  856 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   1379485 |  857 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   1359943 |  858 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|         2 |  859 | `	}` |
|     12682 |  860 | `	return SXERR_NOTFOUND;` |
|     45178 |  861 |  |
|         - |  862 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - |  863 |  |
