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
|  10554700 |   18 | `static void * SyOSHeapAlloc(sxu32 nByte)` |
|         2 |   19 |  |
|         - |   20 | `	void *pNew;` |
|         - |   21 | `#if defined(__WINNT__)` |
|         2 |   22 | `	pNew = HeapAlloc(GetProcessHeap(),0,nByte);` |
|         - |   23 | `#else` |
|  10554700 |   24 | `	pNew = malloc((size_t)nByte);` |
|         - |   25 | `#endif` |
|  10554702 |   26 | `	return pNew;` |
|         2 |   27 |  |
|    677990 |   28 | `static void * SyOSHeapRealloc(void *pOld,sxu32 nByte)` |
|         2 |   29 |  |
|         - |   30 | `	void *pNew;` |
|         - |   31 | `#if defined(__WINNT__)` |
|         2 |   32 | `	pNew = HeapReAlloc(GetProcessHeap(),0,pOld,nByte);` |
|         - |   33 | `#else` |
|    677990 |   34 | `	pNew = realloc(pOld,(size_t)nByte);` |
|         - |   35 | `#endif` |
|    677992 |   36 | `	return pNew;` |
|         2 |   37 |  |
|  10544394 |   38 | `static void SyOSHeapFree(void *pPtr)` |
|         2 |   39 |  |
|         - |   40 | `#if defined(__WINNT__)` |
|         2 |   41 | `	HeapFree(GetProcessHeap(),0,pPtr);` |
|         - |   42 | `#else` |
|  10544394 |   43 | `	free(pPtr);` |
|         - |   44 | `#endif` |
|  10544396 |   45 |  |
|         - |   46 |  |
|         - |   47 |  |
|  18169846 |   48 | `PH7_PRIVATE void SyZero(void *pSrc,sxu32 nSize)` |
|         2 |   49 |  |
|  18169848 |   50 | `	register unsigned char *zSrc = (unsigned char *)pSrc;` |
|         - |   51 | `	unsigned char *zEnd;` |
|         - |   52 | `#if defined(UNTRUST)` |
|         - |   53 | `	if( zSrc == 0 \|\| nSize <= 0 ){` |
|         - |   54 | `		return ;` |
|         - |   55 | `	}` |
|         - |   56 | `#endif` |
|  18169848 |   57 | `	zEnd = &zSrc[nSize];` |
| 239263333 |   58 | `	for(;;){` |
| 478522906 |   59 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 460353064 |   60 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 460353062 |   61 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 460353062 |   62 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
|         2 |   63 | `	}` |
|  18169848 |   64 |  |
|  15222828 |   65 | `PH7_PRIVATE sxi32 SyMemcmp(const void *pB1,const void *pB2,sxu32 nSize)` |
|         2 |   66 |  |
|         - |   67 | `	sxi32 rc;` |
|  15222830 |   68 | `	if( nSize <= 0 ){` |
|        86 |   69 | `		return 0;` |
|         - |   70 | `	}` |
|  15222746 |   71 | `	if( pB1 == 0 \|\| pB2 == 0 ){` |
|       ! 0 |   72 | `		return pB1 != 0 ? 1 : (pB2 == 0 ? 0 : -1);` |
|         - |   73 | `	}` |
|  30718031 |   74 | `	SX_MACRO_FAST_CMP(pB1,pB2,nSize,rc);` |
|  15222746 |   75 | `	return rc;` |
|   7611991 |   76 |  |
|   8927048 |   77 | `PH7_PRIVATE sxu32 SyMemcpy(const void *pSrc,void *pDest,sxu32 nLen)` |
|         2 |   78 |  |
|         - |   79 | `#if defined(UNTRUST)` |
|         - |   80 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|         - |   81 | `		return 0;` |
|         - |   82 | `	}` |
|         - |   83 | `#endif` |
|   8927050 |   84 | `	if( pSrc == (const void *)pDest ){` |
|       ! 0 |   85 | `		return nLen;` |
|         - |   86 | `	}` |
|  72888154 |   87 | `	SX_MACRO_FAST_MEMCPY(pSrc,pDest,nLen);` |
|   8927050 |   88 | `	return nLen;` |
|   4463756 |   89 |  |
|  10554700 |   90 | `static void * MemOSAlloc(sxu32 nBytes)` |
|         2 |   91 |  |
|         - |   92 | `	sxu32 *pChunk;` |
|  10554702 |   93 | `	pChunk = (sxu32 *)SyOSHeapAlloc(nBytes + sizeof(sxu32));` |
|  10554702 |   94 | `	if( pChunk == 0 ){` |
|       ! 0 |   95 | `		return 0;` |
|         - |   96 | `	}` |
|  10554702 |   97 | `	pChunk[0] = nBytes;` |
|  10554702 |   98 | `	return (void *)&pChunk[1];` |
|   5277374 |   99 |  |
|    677990 |  100 | `static void * MemOSRealloc(void *pOld,sxu32 nBytes)` |
|         2 |  101 |  |
|         - |  102 | `	sxu32 *pOldChunk;` |
|         - |  103 | `	sxu32 *pChunk;` |
|    677992 |  104 | `	pOldChunk = (sxu32 *)(((char *)pOld)-sizeof(sxu32));` |
|    677992 |  105 | `	if( pOldChunk[0] >= nBytes ){` |
|       ! 0 |  106 | `		return pOld;` |
|         - |  107 | `	}` |
|    677992 |  108 | `	pChunk = (sxu32 *)SyOSHeapRealloc(pOldChunk,nBytes + sizeof(sxu32));` |
|    677992 |  109 | `	if( pChunk == 0 ){` |
|       ! 0 |  110 | `		return 0;` |
|         - |  111 | `	}` |
|    677992 |  112 | `	pChunk[0] = nBytes;` |
|    677992 |  113 | `	return (void *)&pChunk[1];` |
|    338984 |  114 |  |
|  10544394 |  115 | `static void MemOSFree(void *pBlock)` |
|         2 |  116 |  |
|         - |  117 | `	void *pChunk;` |
|  10544396 |  118 | `	pChunk = (void *)(((char *)pBlock)-sizeof(sxu32));` |
|  10544396 |  119 | `	SyOSHeapFree(pChunk);` |
|  10544396 |  120 |  |
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
|  10554700 |  137 | `static void * MemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         2 |  138 |  |
|         - |  139 | `	SyMemBlock *pBlock;` |
|  10554702 |  140 | `	sxi32 nRetry = 0;` |
|         - |  141 |  |
|         - |  142 | `	/* Append an extra block so we can tracks allocated chunks and avoid memory` |
|         - |  143 | `	 * leaks.` |
|         - |  144 | `	 */` |
|  10554702 |  145 | `	nByte += sizeof(SyMemBlock);` |
|   5277372 |  146 | `	for(;;){` |
|   5277374 |  147 | `		pBlock = (SyMemBlock *)pBackend->pMethods->xAlloc(nByte);` |
|  10554700 |  148 | `		if( pBlock != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY` |
|         2 |  149 | `			\|\| SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|   5277374 |  150 | `				break;` |
|         - |  151 | `		}` |
|       ! 0 |  152 | `		nRetry++;` |
|       ! 0 |  153 | `	}` |
|  10554702 |  154 | `	if( pBlock  == 0 ){` |
|       ! 0 |  155 | `		return 0;` |
|         - |  156 | `	}` |
|  10554702 |  157 | `	pBlock->pNext = pBlock->pPrev = 0;` |
|         - |  158 | `	/* Link to the list of already tracked blocks */` |
|  10554702 |  159 | `	MACRO_LD_PUSH(pBackend->pBlocks,pBlock);` |
|         - |  160 | `#if defined(UNTRUST)` |
|         - |  161 | `	pBlock->nGuard = SXMEM_BACKEND_MAGIC;` |
|         - |  162 | `#endif` |
|  10554702 |  163 | `	pBackend->nBlock++;` |
|  10554702 |  164 | `	return (void *)&pBlock[1];` |
|   5277374 |  165 |  |
|   3035100 |  166 | `PH7_PRIVATE void * SyMemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         2 |  167 |  |
|         - |  168 | `	void *pChunk;` |
|         - |  169 | `#if defined(UNTRUST)` |
|         - |  170 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  171 | `		return 0;` |
|         - |  172 | `	}` |
|         - |  173 | `#endif` |
|   3035102 |  174 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  175 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  176 | `	}` |
|   3035102 |  177 | `	pChunk = MemBackendAlloc(&(*pBackend),nByte);` |
|   3035102 |  178 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  179 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  180 | `	}` |
|   3035102 |  181 | `	return pChunk;` |
|         2 |  182 |  |
|   8154288 |  183 | `static void * MemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         2 |  184 |  |
|         - |  185 | `	SyMemBlock *pBlock,*pNew,*pPrev,*pNext;` |
|   8154290 |  186 | `	sxu32 nRetry = 0;` |
|         - |  187 |  |
|   8154290 |  188 | `	if( pOld == 0 ){` |
|   7476300 |  189 | `		return MemBackendAlloc(&(*pBackend),nByte);` |
|         - |  190 | `	}` |
|    677992 |  191 | `	pBlock = (SyMemBlock *)(((char *)pOld) - sizeof(SyMemBlock));` |
|         - |  192 | `#if defined(UNTRUST)` |
|         - |  193 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|         - |  194 | `		return 0;` |
|         - |  195 | `	}` |
|         - |  196 | `#endif` |
|    677992 |  197 | `	nByte += sizeof(SyMemBlock);` |
|    677992 |  198 | `	pPrev = pBlock->pPrev;` |
|    677992 |  199 | `	pNext = pBlock->pNext;` |
|    338982 |  200 | `	for(;;){` |
|    338984 |  201 | `		pNew = (SyMemBlock *)pBackend->pMethods->xRealloc(pBlock,nByte);` |
|    677992 |  202 | `		if( pNew != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY \|\|` |
|       ! 0 |  203 | `			SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|    338984 |  204 | `				break;` |
|         - |  205 | `		}` |
|       ! 0 |  206 | `		nRetry++;` |
|       ! 0 |  207 | `	}` |
|    677992 |  208 | `	if( pNew == 0 ){` |
|       ! 0 |  209 | `		return 0;` |
|         - |  210 | `	}` |
|    677992 |  211 | `	if( pNew != pBlock ){` |
|    604189 |  212 | `		if( pPrev == 0 ){` |
|    476673 |  213 | `			pBackend->pBlocks = pNew;` |
|    260293 |  214 | `		}else{` |
|    127518 |  215 | `			pPrev->pNext = pNew;` |
|         - |  216 | `		}` |
|    604189 |  217 | `		if( pNext ){` |
|    604176 |  218 | `			pNext->pPrev = pNew;` |
|    324064 |  219 | `		}` |
|         - |  220 | `#if defined(UNTRUST)` |
|         - |  221 | `		pNew->nGuard = SXMEM_BACKEND_MAGIC;` |
|         - |  222 | `#endif` |
|    324070 |  223 | `	}` |
|    677992 |  224 | `	return (void *)&pNew[1];` |
|   4077155 |  225 |  |
|   8154288 |  226 | `PH7_PRIVATE void * SyMemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         2 |  227 |  |
|         - |  228 | `	void *pChunk;` |
|         - |  229 | `#if defined(UNTRUST)` |
|         - |  230 | `	if( SXMEM_BACKEND_CORRUPT(pBackend)  ){` |
|         - |  231 | `		return 0;` |
|         - |  232 | `	}` |
|         - |  233 | `#endif` |
|   8154290 |  234 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  235 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  236 | `	}` |
|   8154290 |  237 | `	pChunk = MemBackendRealloc(&(*pBackend),pOld,nByte);` |
|   8154290 |  238 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  239 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  240 | `	}` |
|   8154290 |  241 | `	return pChunk;` |
|         2 |  242 |  |
|   8267464 |  243 | `static sxi32 MemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|         2 |  244 |  |
|         - |  245 | `	SyMemBlock *pBlock;` |
|   8267466 |  246 | `	pBlock = (SyMemBlock *)(((char *)pChunk) - sizeof(SyMemBlock));` |
|         - |  247 | `#if defined(UNTRUST)` |
|         - |  248 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|         - |  249 | `		return SXERR_CORRUPT;` |
|         - |  250 | `	}` |
|         - |  251 | `#endif` |
|         - |  252 | `	/* Unlink from the list of active blocks */` |
|   8267466 |  253 | `	if( pBackend->nBlock > 0 ){` |
|         - |  254 | `		/* Release the block */` |
|         - |  255 | `#if defined(UNTRUST)` |
|         - |  256 | `		/* Mark as stale block */` |
|         - |  257 | `		pBlock->nGuard = 0x635B;` |
|         - |  258 | `#endif` |
|   8267466 |  259 | `		MACRO_LD_REMOVE(pBackend->pBlocks,pBlock);` |
|   8267466 |  260 | `		pBackend->nBlock--;` |
|   8267466 |  261 | `		pBackend->pMethods->xFree(pBlock);` |
|   4133754 |  262 | `	}` |
|   8267466 |  263 | `	return SXRET_OK;` |
|         2 |  264 |  |
|   8267464 |  265 | `PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|         2 |  266 |  |
|         - |  267 | `	sxi32 rc;` |
|         - |  268 | `#if defined(UNTRUST)` |
|         - |  269 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  270 | `		return SXERR_CORRUPT;` |
|         - |  271 | `	}` |
|         - |  272 | `#endif` |
|   8267466 |  273 | `	if( pChunk == 0 ){` |
|       ! 0 |  274 | `		return SXRET_OK;` |
|         - |  275 | `	}` |
|   8267466 |  276 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  277 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  278 | `	}` |
|   8267466 |  279 | `	rc = MemBackendFree(&(*pBackend),pChunk);` |
|   8267466 |  280 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  281 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  282 | `	}` |
|   8267466 |  283 | `	return rc;` |
|   4133756 |  284 |  |
|         - |  285 | `#if defined(PH7_ENABLE_THREADS)` |
|      2310 |  286 | `PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods)` |
|         2 |  287 |  |
|         - |  288 | `	SyMutex *pMutex;` |
|         - |  289 | `#if defined(UNTRUST)` |
|         - |  290 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pMethods == 0 \|\| pMethods->xNew == 0){` |
|         - |  291 | `		return SXERR_CORRUPT;` |
|         - |  292 | `	}` |
|         - |  293 | `#endif` |
|      2312 |  294 | `	pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|      2312 |  295 | `	if( pMutex == 0 ){` |
|       ! 0 |  296 | `		return SXERR_OS;` |
|         - |  297 | `	}` |
|         - |  298 | `	/* Attach the mutex to the memory backend */` |
|      2312 |  299 | `	pBackend->pMutex = pMutex;` |
|      2312 |  300 | `	pBackend->pMutexMethods = pMethods;` |
|      2312 |  301 | `	return SXRET_OK;` |
|      1157 |  302 |  |
|      2310 |  303 | `PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend)` |
|         2 |  304 |  |
|         - |  305 | `#if defined(UNTRUST)` |
|         - |  306 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  307 | `		return SXERR_CORRUPT;` |
|         - |  308 | `	}` |
|         - |  309 | `#endif` |
|      2312 |  310 | `	if( pBackend->pMutex == 0 ){` |
|         - |  311 | `		/* There is no mutex subsystem at all */` |
|       ! 0 |  312 | `		return SXRET_OK;` |
|         - |  313 | `	}` |
|      2312 |  314 | `	SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|      2312 |  315 | `	pBackend->pMutexMethods = 0;` |
|      2312 |  316 | `	pBackend->pMutex = 0;` |
|      2312 |  317 | `	return SXRET_OK;` |
|      1157 |  318 |  |
|         - |  319 | `#endif` |
|         - |  320 | `/*` |
|         - |  321 | ` * Memory pool allocator` |
|         - |  322 | ` */` |
|         - |  323 | `#define SXMEM_POOL_MAGIC		0xDEAD` |
|         - |  324 | `#define SXMEM_POOL_MAXALLOC		(1<<(SXMEM_POOL_NBUCKETS+SXMEM_POOL_INCR))` |
|         - |  325 | `#define SXMEM_POOL_MINALLOC		(1<<(SXMEM_POOL_INCR))` |
|     43302 |  326 | `static sxi32 MemPoolBucketAlloc(SyMemBackend *pBackend,sxu32 nBucket)` |
|         2 |  327 |  |
|         - |  328 | `	char *zBucket,*zBucketEnd;` |
|         - |  329 | `	SyMemHeader *pHeader;` |
|         - |  330 | `	sxu32 nBucketSize;` |
|         - |  331 |  |
|         - |  332 | `	/* Allocate one big block first */` |
|     43304 |  333 | `	zBucket = (char *)MemBackendAlloc(&(*pBackend),SXMEM_POOL_MAXALLOC);` |
|     43304 |  334 | `	if( zBucket == 0 ){` |
|       ! 0 |  335 | `		return SXERR_MEM;` |
|         - |  336 | `	}` |
|     43304 |  337 | `	zBucketEnd = &zBucket[SXMEM_POOL_MAXALLOC];` |
|         - |  338 | `	/* Divide the big block into mini bucket pool */` |
|     43304 |  339 | `	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);` |
|     43304 |  340 | `	pBackend->apPool[nBucket] = pHeader = (SyMemHeader *)zBucket;` |
|   5131608 |  341 | `	for(;;){` |
|  10263218 |  342 | `		if( &zBucket[nBucketSize] >= zBucketEnd ){` |
|     43304 |  343 | `			break;` |
|         - |  344 | `		}` |
|  10219916 |  345 | `		pHeader->pNext = (SyMemHeader *)&zBucket[nBucketSize];` |
|         - |  346 | `		/* Advance the cursor to the next available chunk */` |
|  10219916 |  347 | `		pHeader = pHeader->pNext;` |
|  10219916 |  348 | `		zBucket += nBucketSize;` |
|         2 |  349 | `	}` |
|     43304 |  350 | `	pHeader->pNext = 0;` |
|         - |  351 |  |
|     43304 |  352 | `	return SXRET_OK;` |
|     21653 |  353 |  |
|  12282348 |  354 | `static void * MemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         2 |  355 |  |
|         - |  356 | `	SyMemHeader *pBucket,*pNext;` |
|         - |  357 | `	sxu32 nBucketSize;` |
|         - |  358 | `	sxu32 nBucket;` |
|         - |  359 |  |
|  12282350 |  360 | `	if( nByte + sizeof(SyMemHeader) >= SXMEM_POOL_MAXALLOC ){` |
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
|  12282350 |  371 | `	nBucket = 0;` |
|  12282350 |  372 | `	nBucketSize = SXMEM_POOL_MINALLOC;` |
|  61148412 |  373 | `	while( nByte + sizeof(SyMemHeader) > nBucketSize  ){` |
|  48866064 |  374 | `		nBucketSize <<= 1;` |
|  48866064 |  375 | `		nBucket++;` |
|         2 |  376 | `	}` |
|  12282350 |  377 | `	pBucket = pBackend->apPool[nBucket];` |
|  12282350 |  378 | `	if( pBucket == 0 ){` |
|         - |  379 | `		sxi32 rc;` |
|     43304 |  380 | `		rc = MemPoolBucketAlloc(&(*pBackend),nBucket);` |
|     43304 |  381 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  382 | `			return 0;` |
|         - |  383 | `		}` |
|     43304 |  384 | `		pBucket = pBackend->apPool[nBucket];` |
|     21651 |  385 | `	}` |
|         - |  386 | `	/* Remove from the free list */` |
|  12282350 |  387 | `	pNext = pBucket->pNext;` |
|  12282350 |  388 | `	pBackend->apPool[nBucket] = pNext;` |
|         - |  389 | `	/* Record bucket&magic number */` |
|  12282350 |  390 | `	pBucket->nBucket = (SXMEM_POOL_MAGIC << 16) \| nBucket;` |
|  12282350 |  391 | `	return (void *)&pBucket[1];` |
|   6141176 |  392 |  |
|  12282348 |  393 | `PH7_PRIVATE void * SyMemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         2 |  394 |  |
|         - |  395 | `	void *pChunk;` |
|         - |  396 | `#if defined(UNTRUST)` |
|         - |  397 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  398 | `		return 0;` |
|         - |  399 | `	}` |
|         - |  400 | `#endif` |
|  12282350 |  401 | `	if( pBackend->pMutexMethods ){` |
|      2312 |  402 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1155 |  403 | `	}` |
|  12282350 |  404 | `	pChunk = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|  12282350 |  405 | `	if( pBackend->pMutexMethods ){` |
|      2312 |  406 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1155 |  407 | `	}` |
|  12282350 |  408 | `	return pChunk;` |
|         2 |  409 |  |
|   8390056 |  410 | `static sxi32 MemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|         2 |  411 |  |
|         - |  412 | `	SyMemHeader *pHeader;` |
|         - |  413 | `	sxu32 nBucket;` |
|         - |  414 | `	/* Get the corresponding bucket */` |
|   8390058 |  415 | `	pHeader = (SyMemHeader *)(((char *)pChunk) - sizeof(SyMemHeader));` |
|         - |  416 | `	/* Sanity check to avoid misuse */` |
|   8390058 |  417 | `	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){` |
|       ! 0 |  418 | `		return SXERR_CORRUPT;` |
|         - |  419 | `	}` |
|   8390058 |  420 | `	nBucket = pHeader->nBucket & 0xFFFF;` |
|   8390058 |  421 | `	if( nBucket == SXU16_HIGH ){` |
|         - |  422 | `		/* Free the big block */` |
|       ! 0 |  423 | `		MemBackendFree(&(*pBackend),pHeader);` |
|   8390058 |  424 | `	}else if( nBucket >= SXMEM_POOL_NBUCKETS + SXMEM_POOL_INCR ){` |
|         - |  425 | `		/* Corrupted or misused bucket index */` |
|       ! 0 |  426 | `		return SXERR_CORRUPT;` |
|       ! 0 |  427 | `	}else{` |
|         - |  428 | `		/* Return to the free list */` |
|   8390058 |  429 | `		pHeader->pNext = pBackend->apPool[nBucket];` |
|   8390058 |  430 | `		pBackend->apPool[nBucket] = pHeader;` |
|         - |  431 | `	}` |
|   8390058 |  432 | `	return SXRET_OK;` |
|   4195030 |  433 |  |
|   8390056 |  434 | `PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|         2 |  435 |  |
|         - |  436 | `	sxi32 rc;` |
|         - |  437 | `#if defined(UNTRUST)` |
|         - |  438 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pChunk == 0 ){` |
|         - |  439 | `		return SXERR_CORRUPT;` |
|         - |  440 | `	}` |
|         - |  441 | `#endif` |
|   8390058 |  442 | `	if( pBackend->pMutexMethods ){` |
|      2072 |  443 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1035 |  444 | `	}` |
|   8390058 |  445 | `	rc = MemBackendPoolFree(&(*pBackend),pChunk);` |
|   8390058 |  446 | `	if( pBackend->pMutexMethods ){` |
|      2072 |  447 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1035 |  448 | `	}` |
|   8390058 |  449 | `	return rc;` |
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
|      2310 |  508 | `PH7_PRIVATE sxi32 SyMemBackendInit(SyMemBackend *pBackend,ProcMemError xMemErr,void * pUserData)` |
|         2 |  509 |  |
|         - |  510 | `#if defined(UNTRUST)` |
|         - |  511 | `	if( pBackend == 0 ){` |
|         - |  512 | `		return SXERR_EMPTY;` |
|         - |  513 | `	}` |
|         - |  514 | `#endif` |
|         - |  515 | `	/* Zero the allocator first */` |
|      2312 |  516 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|      2312 |  517 | `	pBackend->xMemError = xMemErr;` |
|      2312 |  518 | `	pBackend->pUserData = pUserData;` |
|         - |  519 | `	/* Switch to the OS memory allocator */` |
|      2312 |  520 | `	pBackend->pMethods = &sOSAllocMethods;` |
|      2312 |  521 | `	if( pBackend->pMethods->xInit ){` |
|         - |  522 | `		/* Initialize the backend  */` |
|       ! 0 |  523 | `		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){` |
|       ! 0 |  524 | `			return SXERR_ABORT;` |
|         - |  525 | `		}` |
|       ! 0 |  526 | `	}` |
|         - |  527 | `#if defined(UNTRUST)` |
|         - |  528 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|         - |  529 | `#endif` |
|      2312 |  530 | `	return SXRET_OK;` |
|      1157 |  531 |  |
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
|      4620 |  560 | `PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend *pBackend,SyMemBackend *pParent)` |
|         2 |  561 |  |
|         - |  562 | `	sxu8 bInheritMutex;` |
|         - |  563 | `#if defined(UNTRUST)` |
|         - |  564 | `	if( pBackend == 0 \|\| SXMEM_BACKEND_CORRUPT(pParent) ){` |
|         - |  565 | `		return SXERR_CORRUPT;` |
|         - |  566 | `	}` |
|         - |  567 | `#endif` |
|         - |  568 | `	/* Zero the allocator first */` |
|      4622 |  569 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|      4622 |  570 | `	pBackend->pMethods  = pParent->pMethods;` |
|      4622 |  571 | `	pBackend->xMemError = pParent->xMemError;` |
|      4622 |  572 | `	pBackend->pUserData = pParent->pUserData;` |
|      4622 |  573 | `	bInheritMutex = pParent->pMutexMethods ? TRUE : FALSE;` |
|      4622 |  574 | `	if( bInheritMutex ){` |
|      2312 |  575 | `		pBackend->pMutexMethods = pParent->pMutexMethods;` |
|         - |  576 | `		/* Create a private mutex */` |
|      2312 |  577 | `		pBackend->pMutex = pBackend->pMutexMethods->xNew(SXMUTEX_TYPE_FAST);` |
|      2312 |  578 | `		if( pBackend->pMutex ==  0){` |
|       ! 0 |  579 | `			return SXERR_OS;` |
|         - |  580 | `		}` |
|      1155 |  581 | `	}` |
|         - |  582 | `#if defined(UNTRUST)` |
|         - |  583 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|         - |  584 | `#endif` |
|      4622 |  585 | `	return SXRET_OK;` |
|      2312 |  586 |  |
|      4836 |  587 | `static sxi32 MemBackendRelease(SyMemBackend *pBackend)` |
|         2 |  588 |  |
|         - |  589 | `	SyMemBlock *pBlock,*pNext;` |
|         - |  590 |  |
|      4838 |  591 | `	pBlock = pBackend->pBlocks;` |
|    286360 |  592 | `	for(;;){` |
|    572722 |  593 | `		if( pBackend->nBlock == 0 ){` |
|       585 |  594 | `			break;` |
|         - |  595 | `		}` |
|    572138 |  596 | `		pNext  = pBlock->pNext;` |
|    572138 |  597 | `		pBackend->pMethods->xFree(pBlock);` |
|    572138 |  598 | `		pBlock = pNext;` |
|    572138 |  599 | `		pBackend->nBlock--;` |
|         - |  600 | `		/* LOOP ONE */` |
|    572138 |  601 | `		if( pBackend->nBlock == 0 ){` |
|      3506 |  602 | `			break;` |
|         - |  603 | `		}` |
|    568634 |  604 | `		pNext  = pBlock->pNext;` |
|    568634 |  605 | `		pBackend->pMethods->xFree(pBlock);` |
|    568634 |  606 | `		pBlock = pNext;` |
|    568634 |  607 | `		pBackend->nBlock--;` |
|         - |  608 | `		/* LOOP TWO */` |
|    568634 |  609 | `		if( pBackend->nBlock == 0 ){` |
|       355 |  610 | `			break;` |
|         - |  611 | `		}` |
|    568280 |  612 | `		pNext  = pBlock->pNext;` |
|    568280 |  613 | `		pBackend->pMethods->xFree(pBlock);` |
|    568280 |  614 | `		pBlock = pNext;` |
|    568280 |  615 | `		pBackend->nBlock--;` |
|         - |  616 | `		/* LOOP THREE */` |
|    568280 |  617 | `		if( pBackend->nBlock == 0 ){` |
|       396 |  618 | `			break;` |
|         - |  619 | `		}` |
|    567886 |  620 | `		pNext  = pBlock->pNext;` |
|    567886 |  621 | `		pBackend->pMethods->xFree(pBlock);` |
|    567886 |  622 | `		pBlock = pNext;` |
|    567886 |  623 | `		pBackend->nBlock--;` |
|         - |  624 | `		/* LOOP FOUR */` |
|         2 |  625 | `	}` |
|      4838 |  626 | `	if( pBackend->pMethods->xRelease ){` |
|       ! 0 |  627 | `		pBackend->pMethods->xRelease(pBackend->pMethods->pUserData);` |
|       ! 0 |  628 | `	}` |
|      4838 |  629 | `	pBackend->pMethods = 0;` |
|      4838 |  630 | `	pBackend->pBlocks  = 0;` |
|         - |  631 | `#if defined(UNTRUST)` |
|         - |  632 | `	pBackend->nMagic = 0x2626;` |
|         - |  633 | `#endif` |
|      4838 |  634 | `	return SXRET_OK;` |
|         2 |  635 |  |
|      4836 |  636 | `PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend *pBackend)` |
|         2 |  637 |  |
|         - |  638 | `#if defined(UNTRUST)` |
|         - |  639 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  640 | `		return SXERR_INVALID;` |
|         - |  641 | `	}` |
|         - |  642 | `#endif` |
|      4838 |  643 | `	if( pBackend->pMutexMethods ){` |
|       233 |  644 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       116 |  645 | `	}` |
|      4838 |  646 | `	(void)MemBackendRelease(&(*pBackend));` |
|      4838 |  647 | `	if( pBackend->pMutexMethods ){` |
|       233 |  648 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       233 |  649 | `		SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|       116 |  650 | `	}` |
|      4838 |  651 | `	return SXRET_OK;` |
|         2 |  652 |  |
|    501148 |  653 | `PH7_PRIVATE void * SyMemBackendDup(SyMemBackend *pBackend,const void *pSrc,sxu32 nSize)` |
|         2 |  654 |  |
|         - |  655 | `	void *pNew;` |
|         - |  656 | `#if defined(UNTRUST)` |
|         - |  657 | `	if( pSrc == 0 \|\| nSize <= 0 ){` |
|         - |  658 | `		return 0;` |
|         - |  659 | `	}` |
|         - |  660 | `#endif` |
|    501150 |  661 | `	pNew = SyMemBackendAlloc(&(*pBackend),nSize);` |
|    501150 |  662 | `	if( pNew ){` |
|    501150 |  663 | `		SyMemcpy(pSrc,pNew,nSize);` |
|    250574 |  664 | `	}` |
|    501150 |  665 | `	return pNew;` |
|         2 |  666 |  |
|   1578698 |  667 | `PH7_PRIVATE char * SyMemBackendStrDup(SyMemBackend *pBackend,const char *zSrc,sxu32 nSize)` |
|         2 |  668 |  |
|         - |  669 | `	char *zDest;` |
|   1578700 |  670 | `	zDest = (char *)SyMemBackendAlloc(&(*pBackend),nSize + 1);` |
|   1578700 |  671 | `	if( zDest ){` |
|   1578700 |  672 | `		Systrcpy(zDest,nSize+1,zSrc,nSize);` |
|    789349 |  673 | `	}` |
|   1578700 |  674 | `	return zDest;` |
|         2 |  675 |  |
|     67540 |  676 | `PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize)` |
|         2 |  677 |  |
|         - |  678 | `#if defined(UNTRUST)` |
|         - |  679 | `	if( pBlob == 0 \|\| pBuffer == 0 \|\| nSize < 1 ){` |
|         - |  680 | `		return SXERR_EMPTY;` |
|         - |  681 | `	}` |
|         - |  682 | `#endif` |
|     67542 |  683 | `	pBlob->pBlob = pBuffer;` |
|     67542 |  684 | `	pBlob->mByte = nSize;` |
|     67542 |  685 | `	pBlob->nByte = 0;` |
|     67542 |  686 | `	pBlob->pAllocator = 0;` |
|     67542 |  687 | `	pBlob->nFlags = SXBLOB_LOCKED\|SXBLOB_STATIC;` |
|     67542 |  688 | `	return SXRET_OK;` |
|         2 |  689 |  |
|   6226894 |  690 | `PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator)` |
|         2 |  691 |  |
|         - |  692 | `#if defined(UNTRUST)` |
|         - |  693 | `	if( pBlob == 0  ){` |
|         - |  694 | `		return SXERR_EMPTY;` |
|         - |  695 | `	}` |
|         - |  696 | `#endif` |
|   6226896 |  697 | `	pBlob->pBlob = 0;` |
|   6226896 |  698 | `	pBlob->mByte = pBlob->nByte	= 0;` |
|   6226896 |  699 | `	pBlob->pAllocator = &(*pAllocator);` |
|   6226896 |  700 | `	pBlob->nFlags = 0;` |
|   6226896 |  701 | `	return SXRET_OK;` |
|         2 |  702 |  |
|   2382912 |  703 | `PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte)` |
|         2 |  704 |  |
|         - |  705 | `#if defined(UNTRUST)` |
|         - |  706 | `	if( pBlob == 0  ){` |
|         - |  707 | `		return SXERR_EMPTY;` |
|         - |  708 | `	}` |
|         - |  709 | `#endif` |
|   2382914 |  710 | `	pBlob->pBlob = (void *)pData;` |
|   2382914 |  711 | `	pBlob->nByte = nByte;` |
|   2382914 |  712 | `	pBlob->mByte = 0;` |
|   2382914 |  713 | `	pBlob->nFlags \|= SXBLOB_RDONLY;` |
|   2382914 |  714 | `	return SXRET_OK;` |
|         2 |  715 |  |
|         - |  716 | `#ifndef SXBLOB_MIN_GROWTH` |
|         - |  717 | `#define SXBLOB_MIN_GROWTH 16` |
|         - |  718 | `#endif` |
|   6157762 |  719 | `static sxi32 BlobPrepareGrow(SyBlob *pBlob,sxu32 *pByte)` |
|         2 |  720 |  |
|         - |  721 | `	sxu32 nByte;` |
|         - |  722 | `	void *pNew;` |
|   6157764 |  723 | `	nByte = *pByte;` |
|   6157764 |  724 | `	if( pBlob->nFlags & (SXBLOB_LOCKED\|SXBLOB_STATIC) ){` |
|    539164 |  725 | `		if ( SyBlobFreeSpace(pBlob) < nByte ){` |
|       ! 0 |  726 | `			*pByte = SyBlobFreeSpace(pBlob);` |
|       ! 0 |  727 | `			if( (*pByte) == 0 ){` |
|       ! 0 |  728 | `				return SXERR_SHORT;` |
|         - |  729 | `			}` |
|       ! 0 |  730 | `		}` |
|    539164 |  731 | `		return SXRET_OK;` |
|         - |  732 | `	}` |
|   5618602 |  733 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|         - |  734 | `		/* Make a copy of the read-only item */` |
|    501150 |  735 | `		if( pBlob->nByte > 0 ){` |
|    501150 |  736 | `			pNew = SyMemBackendDup(pBlob->pAllocator,pBlob->pBlob,pBlob->nByte);` |
|    501150 |  737 | `			if( pNew == 0 ){` |
|       ! 0 |  738 | `				return SXERR_MEM;` |
|         - |  739 | `			}` |
|    501150 |  740 | `			pBlob->pBlob = pNew;` |
|    501150 |  741 | `			pBlob->mByte = pBlob->nByte;` |
|    250576 |  742 | `		}else{` |
|       ! 0 |  743 | `			pBlob->pBlob = 0;` |
|       ! 0 |  744 | `			pBlob->mByte = 0;` |
|         - |  745 | `		}` |
|         - |  746 | `		/* Remove the read-only flag */` |
|    501150 |  747 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|    250574 |  748 | `	}` |
|   5618602 |  749 | `	if( SyBlobFreeSpace(pBlob) >= nByte ){` |
|   1096492 |  750 | `		return SXRET_OK;` |
|         - |  751 | `	}` |
|   4522112 |  752 | `	if( pBlob->mByte > 0 ){` |
|    583324 |  753 | `		nByte = nByte + pBlob->mByte * 2 + SXBLOB_MIN_GROWTH;` |
|   4230438 |  754 | `	}else if ( nByte < SXBLOB_MIN_GROWTH ){` |
|   3229429 |  755 | `		nByte = SXBLOB_MIN_GROWTH;` |
|   1614621 |  756 | `	}` |
|   4522112 |  757 | `	pNew = SyMemBackendRealloc(pBlob->pAllocator,pBlob->pBlob,nByte);` |
|   4522112 |  758 | `	if( pNew == 0 ){` |
|       ! 0 |  759 | `		return SXERR_MEM;` |
|         - |  760 | `	}` |
|   4522112 |  761 | `	pBlob->pBlob = pNew;` |
|   4522112 |  762 | `	pBlob->mByte = nByte;` |
|   4522112 |  763 | `	return SXRET_OK;` |
|   3078927 |  764 |  |
|   6200690 |  765 | `PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize)` |
|         2 |  766 |  |
|         - |  767 | `	sxu8 *zBlob;` |
|         - |  768 | `	sxi32 rc;` |
|   6200692 |  769 | `	if( nSize < 1 ){` |
|     42930 |  770 | `		return SXRET_OK;` |
|         - |  771 | `	}` |
|   6157764 |  772 | `	rc = BlobPrepareGrow(&(*pBlob),&nSize);` |
|   6157764 |  773 | `	if( SXRET_OK != rc ){` |
|       ! 0 |  774 | `		return rc;` |
|         - |  775 | `	}` |
|   6157764 |  776 | `	if( pData ){` |
|   6157732 |  777 | `		zBlob = (sxu8 *)pBlob->pBlob ;` |
|   6157732 |  778 | `		zBlob = &zBlob[pBlob->nByte];` |
|   6157732 |  779 | `		pBlob->nByte += nSize;` |
|  28099268 |  780 | `		SX_MACRO_FAST_MEMCPY(pData,zBlob,nSize);` |
|   3078909 |  781 | `	}` |
|   6157764 |  782 | `	return SXRET_OK;` |
|   3100391 |  783 |  |
|    501278 |  784 | `PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob)` |
|         2 |  785 |  |
|         - |  786 | `	sxi32 rc;` |
|         - |  787 | `	sxu32 n;` |
|    501280 |  788 | `	n = pBlob->nByte;` |
|    501280 |  789 | `	rc = SyBlobAppend(&(*pBlob),(const void *)"\0",sizeof(char));` |
|    501280 |  790 | `	if (rc == SXRET_OK ){` |
|    501280 |  791 | `		pBlob->nByte = n;` |
|    250661 |  792 | `	}` |
|    501280 |  793 | `	return rc;` |
|         2 |  794 |  |
|   3369572 |  795 | `PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest)` |
|         2 |  796 |  |
|   3369574 |  797 | `	sxi32 rc = SXRET_OK;` |
|         - |  798 | `#ifdef UNTRUST` |
|         - |  799 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|         - |  800 | `		return SXERR_EMPTY;` |
|         - |  801 | `	}` |
|         - |  802 | `#endif` |
|   3369574 |  803 | `	if( pSrc->nByte > 0 ){` |
|   3349858 |  804 | `		rc = SyBlobAppend(&(*pDest),pSrc->pBlob,pSrc->nByte);` |
|   1674928 |  805 | `	}` |
|   3369574 |  806 | `	return rc;` |
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
|   3546538 |  827 | `PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob)` |
|         2 |  828 |  |
|   3546540 |  829 | `	pBlob->nByte = 0;` |
|   3546540 |  830 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|      7194 |  831 | `		pBlob->pBlob = 0;` |
|      7194 |  832 | `		pBlob->mByte = 0;` |
|      7194 |  833 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|      3596 |  834 | `	}` |
|   3546540 |  835 | `	return SXRET_OK;` |
|         2 |  836 |  |
|   9359556 |  837 | `PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob)` |
|         2 |  838 |  |
|   9359558 |  839 | `	if( (pBlob->nFlags & (SXBLOB_STATIC\|SXBLOB_RDONLY)) == 0 && pBlob->mByte > 0 ){` |
|   4197178 |  840 | `		SyMemBackendFree(pBlob->pAllocator,pBlob->pBlob);` |
|   2098610 |  841 | `	}` |
|   9359558 |  842 | `	pBlob->pBlob = 0;` |
|   9359558 |  843 | `	pBlob->nByte = pBlob->mByte = 0;` |
|   9359558 |  844 | `	pBlob->nFlags = 0;` |
|   9359558 |  845 | `	return SXRET_OK;` |
|         2 |  846 |  |
|         - |  847 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    110260 |  848 | `PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft)` |
|         2 |  849 |  |
|    110262 |  850 | `	const char *zIn = (const char *)pBlob;` |
|         - |  851 | `	const char *zEnd;` |
|         - |  852 | `	sxi32 rc;` |
|    110262 |  853 | `	if( pLen > nLen ){` |
|      4288 |  854 | `		return SXERR_NOTFOUND;` |
|         - |  855 | `	}` |
|    105976 |  856 | `	zEnd = &zIn[nLen-pLen];` |
|    891693 |  857 | `	for(;;){` |
|   1783338 |  858 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   1756342 |  859 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   1719066 |  860 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   1695602 |  861 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|         2 |  862 | `	}` |
|     16548 |  863 | `	return SXERR_NOTFOUND;` |
|     55132 |  864 |  |
|         - |  865 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - |  866 |  |
