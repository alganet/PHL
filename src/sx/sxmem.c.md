# src/sx/sxmem.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 432/505 lines (85.54%)

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
|  13263192 |   18 | `static void * SyOSHeapAlloc(sxu32 nByte)` |
|         2 |   19 |  |
|         - |   20 | `	void *pNew;` |
|         - |   21 | `#if defined(__WINNT__)` |
|         2 |   22 | `	pNew = HeapAlloc(GetProcessHeap(),0,nByte);` |
|         - |   23 | `#else` |
|  13263192 |   24 | `	pNew = malloc((size_t)nByte);` |
|         - |   25 | `#endif` |
|  13263194 |   26 | `	return pNew;` |
|         2 |   27 |  |
|    858597 |   28 | `static void * SyOSHeapRealloc(void *pOld,sxu32 nByte)` |
|         2 |   29 |  |
|         - |   30 | `	void *pNew;` |
|         - |   31 | `#if defined(__WINNT__)` |
|         2 |   32 | `	pNew = HeapReAlloc(GetProcessHeap(),0,pOld,nByte);` |
|         - |   33 | `#else` |
|    858597 |   34 | `	pNew = realloc(pOld,(size_t)nByte);` |
|         - |   35 | `#endif` |
|    858599 |   36 | `	return pNew;` |
|         2 |   37 |  |
|  13250372 |   38 | `static void SyOSHeapFree(void *pPtr)` |
|         2 |   39 |  |
|         - |   40 | `#if defined(__WINNT__)` |
|         2 |   41 | `	HeapFree(GetProcessHeap(),0,pPtr);` |
|         - |   42 | `#else` |
|  13250372 |   43 | `	free(pPtr);` |
|         - |   44 | `#endif` |
|  13250374 |   45 |  |
|         - |   46 |  |
|         - |   47 |  |
|  23763668 |   48 | `PH7_PRIVATE void SyZero(void *pSrc,sxu32 nSize)` |
|         2 |   49 |  |
|  23763670 |   50 | `	register unsigned char *zSrc = (unsigned char *)pSrc;` |
|         - |   51 | `	unsigned char *zEnd;` |
|         - |   52 | `#if defined(UNTRUST)` |
|         - |   53 | `	if( zSrc == 0 \|\| nSize <= 0 ){` |
|         - |   54 | `		return ;` |
|         - |   55 | `	}` |
|         - |   56 | `#endif` |
|  23763670 |   57 | `	zEnd = &zSrc[nSize];` |
| 322564408 |   58 | `	for(;;){` |
| 645128096 |   59 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 621364606 |   60 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 621364556 |   61 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 621364448 |   62 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
|         2 |   63 | `	}` |
|  23763670 |   64 |  |
|  23450314 |   65 | `PH7_PRIVATE sxi32 SyMemcmp(const void *pB1,const void *pB2,sxu32 nSize)` |
|         2 |   66 |  |
|         - |   67 | `	sxi32 rc;` |
|  23450316 |   68 | `	if( nSize <= 0 ){` |
|        87 |   69 | `		return 0;` |
|         - |   70 | `	}` |
|  23450230 |   71 | `	if( pB1 == 0 \|\| pB2 == 0 ){` |
|       ! 0 |   72 | `		return pB1 != 0 ? 1 : (pB2 == 0 ? 0 : -1);` |
|         - |   73 | `	}` |
|  46175942 |   74 | `	SX_MACRO_FAST_CMP(pB1,pB2,nSize,rc);` |
|  23450230 |   75 | `	return rc;` |
|  11725755 |   76 |  |
|  10475502 |   77 | `PH7_PRIVATE sxu32 SyMemcpy(const void *pSrc,void *pDest,sxu32 nLen)` |
|         2 |   78 |  |
|         - |   79 | `#if defined(UNTRUST)` |
|         - |   80 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|         - |   81 | `		return 0;` |
|         - |   82 | `	}` |
|         - |   83 | `#endif` |
|  10475504 |   84 | `	if( pSrc == (const void *)pDest ){` |
|       ! 0 |   85 | `		return nLen;` |
|         - |   86 | `	}` |
|  85881189 |   87 | `	SX_MACRO_FAST_MEMCPY(pSrc,pDest,nLen);` |
|  10475504 |   88 | `	return nLen;` |
|   5237904 |   89 |  |
|  13263192 |   90 | `static void * MemOSAlloc(sxu32 nBytes)` |
|         2 |   91 |  |
|         - |   92 | `	sxu32 *pChunk;` |
|  13263194 |   93 | `	pChunk = (sxu32 *)SyOSHeapAlloc(nBytes + sizeof(sxu32));` |
|  13263194 |   94 | `	if( pChunk == 0 ){` |
|       ! 0 |   95 | `		return 0;` |
|         - |   96 | `	}` |
|  13263194 |   97 | `	pChunk[0] = nBytes;` |
|  13263194 |   98 | `	return (void *)&pChunk[1];` |
|   6631620 |   99 |  |
|    858597 |  100 | `static void * MemOSRealloc(void *pOld,sxu32 nBytes)` |
|         2 |  101 |  |
|         - |  102 | `	sxu32 *pOldChunk;` |
|         - |  103 | `	sxu32 *pChunk;` |
|    858599 |  104 | `	pOldChunk = (sxu32 *)(((char *)pOld)-sizeof(sxu32));` |
|    858599 |  105 | `	if( pOldChunk[0] >= nBytes ){` |
|       ! 0 |  106 | `		return pOld;` |
|         - |  107 | `	}` |
|    858599 |  108 | `	pChunk = (sxu32 *)SyOSHeapRealloc(pOldChunk,nBytes + sizeof(sxu32));` |
|    858599 |  109 | `	if( pChunk == 0 ){` |
|       ! 0 |  110 | `		return 0;` |
|         - |  111 | `	}` |
|    858599 |  112 | `	pChunk[0] = nBytes;` |
|    858599 |  113 | `	return (void *)&pChunk[1];` |
|    429288 |  114 |  |
|  13250372 |  115 | `static void MemOSFree(void *pBlock)` |
|         2 |  116 |  |
|         - |  117 | `	void *pChunk;` |
|  13250374 |  118 | `	pChunk = (void *)(((char *)pBlock)-sizeof(sxu32));` |
|  13250374 |  119 | `	SyOSHeapFree(pChunk);` |
|  13250374 |  120 |  |
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
|  13263192 |  137 | `static void * MemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         2 |  138 |  |
|         - |  139 | `	SyMemBlock *pBlock;` |
|  13263194 |  140 | `	sxi32 nRetry = 0;` |
|         - |  141 |  |
|         - |  142 | `	/* Append an extra block so we can tracks allocated chunks and avoid memory` |
|         - |  143 | `	 * leaks.` |
|         - |  144 | `	 */` |
|  13263194 |  145 | `	nByte += sizeof(SyMemBlock);` |
|   6631618 |  146 | `	for(;;){` |
|   6631620 |  147 | `		pBlock = (SyMemBlock *)pBackend->pMethods->xAlloc(nByte);` |
|  13263192 |  148 | `		if( pBlock != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY` |
|         2 |  149 | `			\|\| SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|   6631620 |  150 | `				break;` |
|         - |  151 | `		}` |
|       ! 0 |  152 | `		nRetry++;` |
|       ! 0 |  153 | `	}` |
|  13263194 |  154 | `	if( pBlock  == 0 ){` |
|       ! 0 |  155 | `		return 0;` |
|         - |  156 | `	}` |
|  13263194 |  157 | `	pBlock->pNext = pBlock->pPrev = 0;` |
|         - |  158 | `	/* Link to the list of already tracked blocks */` |
|  13263194 |  159 | `	MACRO_LD_PUSH(pBackend->pBlocks,pBlock);` |
|         - |  160 | `#if defined(UNTRUST)` |
|         - |  161 | `	pBlock->nGuard = SXMEM_BACKEND_MAGIC;` |
|         - |  162 | `#endif` |
|  13263194 |  163 | `	pBackend->nBlock++;` |
|  13263194 |  164 | `	return (void *)&pBlock[1];` |
|   6631620 |  165 |  |
|   4678598 |  166 | `PH7_PRIVATE void * SyMemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         2 |  167 |  |
|         - |  168 | `	void *pChunk;` |
|         - |  169 | `#if defined(UNTRUST)` |
|         - |  170 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  171 | `		return 0;` |
|         - |  172 | `	}` |
|         - |  173 | `#endif` |
|   4678600 |  174 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  175 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  176 | `	}` |
|   4678600 |  177 | `	pChunk = MemBackendAlloc(&(*pBackend),nByte);` |
|   4678600 |  178 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  179 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  180 | `	}` |
|   4678600 |  181 | `	return pChunk;` |
|         2 |  182 |  |
|   9386045 |  183 | `static void * MemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         2 |  184 |  |
|         - |  185 | `	SyMemBlock *pBlock,*pNew,*pPrev,*pNext;` |
|   9386047 |  186 | `	sxu32 nRetry = 0;` |
|         - |  187 |  |
|   9386047 |  188 | `	if( pOld == 0 ){` |
|   8527450 |  189 | `		return MemBackendAlloc(&(*pBackend),nByte);` |
|         - |  190 | `	}` |
|    858599 |  191 | `	pBlock = (SyMemBlock *)(((char *)pOld) - sizeof(SyMemBlock));` |
|         - |  192 | `#if defined(UNTRUST)` |
|         - |  193 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|         - |  194 | `		return 0;` |
|         - |  195 | `	}` |
|         - |  196 | `#endif` |
|    858599 |  197 | `	nByte += sizeof(SyMemBlock);` |
|    858599 |  198 | `	pPrev = pBlock->pPrev;` |
|    858599 |  199 | `	pNext = pBlock->pNext;` |
|    429286 |  200 | `	for(;;){` |
|    429288 |  201 | `		pNew = (SyMemBlock *)pBackend->pMethods->xRealloc(pBlock,nByte);` |
|    858599 |  202 | `		if( pNew != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY \|\|` |
|       ! 0 |  203 | `			SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|    429288 |  204 | `				break;` |
|         - |  205 | `		}` |
|       ! 0 |  206 | `		nRetry++;` |
|       ! 0 |  207 | `	}` |
|    858599 |  208 | `	if( pNew == 0 ){` |
|       ! 0 |  209 | `		return 0;` |
|         - |  210 | `	}` |
|    858599 |  211 | `	if( pNew != pBlock ){` |
|    755515 |  212 | `		if( pPrev == 0 ){` |
|    596317 |  213 | `			pBackend->pBlocks = pNew;` |
|    323674 |  214 | `		}else{` |
|    159200 |  215 | `			pPrev->pNext = pNew;` |
|         - |  216 | `		}` |
|    755515 |  217 | `		if( pNext ){` |
|    755501 |  218 | `			pNext->pPrev = pNew;` |
|    396407 |  219 | `		}` |
|         - |  220 | `#if defined(UNTRUST)` |
|         - |  221 | `		pNew->nGuard = SXMEM_BACKEND_MAGIC;` |
|         - |  222 | `#endif` |
|    396414 |  223 | `	}` |
|    858599 |  224 | `	return (void *)&pNew[1];` |
|   4693034 |  225 |  |
|   9386045 |  226 | `PH7_PRIVATE void * SyMemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         2 |  227 |  |
|         - |  228 | `	void *pChunk;` |
|         - |  229 | `#if defined(UNTRUST)` |
|         - |  230 | `	if( SXMEM_BACKEND_CORRUPT(pBackend)  ){` |
|         - |  231 | `		return 0;` |
|         - |  232 | `	}` |
|         - |  233 | `#endif` |
|   9386047 |  234 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  235 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  236 | `	}` |
|   9386047 |  237 | `	pChunk = MemBackendRealloc(&(*pBackend),pOld,nByte);` |
|   9386047 |  238 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  239 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  240 | `	}` |
|   9386047 |  241 | `	return pChunk;` |
|         2 |  242 |  |
|   9600598 |  243 | `static sxi32 MemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|         2 |  244 |  |
|         - |  245 | `	SyMemBlock *pBlock;` |
|   9600600 |  246 | `	pBlock = (SyMemBlock *)(((char *)pChunk) - sizeof(SyMemBlock));` |
|         - |  247 | `#if defined(UNTRUST)` |
|         - |  248 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|         - |  249 | `		return SXERR_CORRUPT;` |
|         - |  250 | `	}` |
|         - |  251 | `#endif` |
|         - |  252 | `	/* Unlink from the list of active blocks */` |
|   9600600 |  253 | `	if( pBackend->nBlock > 0 ){` |
|         - |  254 | `		/* Release the block */` |
|         - |  255 | `#if defined(UNTRUST)` |
|         - |  256 | `		/* Mark as stale block */` |
|         - |  257 | `		pBlock->nGuard = 0x635B;` |
|         - |  258 | `#endif` |
|   9600600 |  259 | `		MACRO_LD_REMOVE(pBackend->pBlocks,pBlock);` |
|   9600600 |  260 | `		pBackend->nBlock--;` |
|   9600600 |  261 | `		pBackend->pMethods->xFree(pBlock);` |
|   4800321 |  262 | `	}` |
|   9600600 |  263 | `	return SXRET_OK;` |
|         2 |  264 |  |
|   9600598 |  265 | `PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|         2 |  266 |  |
|         - |  267 | `	sxi32 rc;` |
|         - |  268 | `#if defined(UNTRUST)` |
|         - |  269 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  270 | `		return SXERR_CORRUPT;` |
|         - |  271 | `	}` |
|         - |  272 | `#endif` |
|   9600600 |  273 | `	if( pChunk == 0 ){` |
|       ! 0 |  274 | `		return SXRET_OK;` |
|         - |  275 | `	}` |
|   9600600 |  276 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  277 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  278 | `	}` |
|   9600600 |  279 | `	rc = MemBackendFree(&(*pBackend),pChunk);` |
|   9600600 |  280 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  281 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  282 | `	}` |
|   9600600 |  283 | `	return rc;` |
|   4800323 |  284 |  |
|         - |  285 | `#if defined(PH7_ENABLE_THREADS)` |
|      2994 |  286 | `PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods)` |
|         2 |  287 |  |
|         - |  288 | `	SyMutex *pMutex;` |
|         - |  289 | `#if defined(UNTRUST)` |
|         - |  290 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pMethods == 0 \|\| pMethods->xNew == 0){` |
|         - |  291 | `		return SXERR_CORRUPT;` |
|         - |  292 | `	}` |
|         - |  293 | `#endif` |
|      2996 |  294 | `	pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|      2996 |  295 | `	if( pMutex == 0 ){` |
|       ! 0 |  296 | `		return SXERR_OS;` |
|         - |  297 | `	}` |
|         - |  298 | `	/* Attach the mutex to the memory backend */` |
|      2996 |  299 | `	pBackend->pMutex = pMutex;` |
|      2996 |  300 | `	pBackend->pMutexMethods = pMethods;` |
|      2996 |  301 | `	return SXRET_OK;` |
|      1499 |  302 |  |
|      2994 |  303 | `PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend)` |
|         2 |  304 |  |
|         - |  305 | `#if defined(UNTRUST)` |
|         - |  306 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  307 | `		return SXERR_CORRUPT;` |
|         - |  308 | `	}` |
|         - |  309 | `#endif` |
|      2996 |  310 | `	if( pBackend->pMutex == 0 ){` |
|         - |  311 | `		/* There is no mutex subsystem at all */` |
|       ! 0 |  312 | `		return SXRET_OK;` |
|         - |  313 | `	}` |
|      2996 |  314 | `	SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|      2996 |  315 | `	pBackend->pMutexMethods = 0;` |
|      2996 |  316 | `	pBackend->pMutex = 0;` |
|      2996 |  317 | `	return SXRET_OK;` |
|      1499 |  318 |  |
|         - |  319 | `#endif` |
|         - |  320 | `/*` |
|         - |  321 | ` * Memory pool allocator` |
|         - |  322 | ` */` |
|         - |  323 | `#define SXMEM_POOL_MAGIC		0xDEAD` |
|         - |  324 | `#define SXMEM_POOL_MAXALLOC		(1<<(SXMEM_POOL_NBUCKETS+SXMEM_POOL_INCR))` |
|         - |  325 | `#define SXMEM_POOL_MINALLOC		(1<<(SXMEM_POOL_INCR))` |
|     57146 |  326 | `static sxi32 MemPoolBucketAlloc(SyMemBackend *pBackend,sxu32 nBucket)` |
|         2 |  327 |  |
|         - |  328 | `	char *zBucket,*zBucketEnd;` |
|         - |  329 | `	SyMemHeader *pHeader;` |
|         - |  330 | `	sxu32 nBucketSize;` |
|         - |  331 |  |
|         - |  332 | `	/* Allocate one big block first */` |
|     57148 |  333 | `	zBucket = (char *)MemBackendAlloc(&(*pBackend),SXMEM_POOL_MAXALLOC);` |
|     57148 |  334 | `	if( zBucket == 0 ){` |
|       ! 0 |  335 | `		return SXERR_MEM;` |
|         - |  336 | `	}` |
|     57148 |  337 | `	zBucketEnd = &zBucket[SXMEM_POOL_MAXALLOC];` |
|         - |  338 | `	/* Divide the big block into mini bucket pool */` |
|     57148 |  339 | `	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);` |
|     57148 |  340 | `	pBackend->apPool[nBucket] = pHeader = (SyMemHeader *)zBucket;` |
|   6484920 |  341 | `	for(;;){` |
|  12969842 |  342 | `		if( &zBucket[nBucketSize] >= zBucketEnd ){` |
|     57148 |  343 | `			break;` |
|         - |  344 | `		}` |
|  12912696 |  345 | `		pHeader->pNext = (SyMemHeader *)&zBucket[nBucketSize];` |
|         - |  346 | `		/* Advance the cursor to the next available chunk */` |
|  12912696 |  347 | `		pHeader = pHeader->pNext;` |
|  12912696 |  348 | `		zBucket += nBucketSize;` |
|         2 |  349 | `	}` |
|     57148 |  350 | `	pHeader->pNext = 0;` |
|         - |  351 |  |
|     57148 |  352 | `	return SXRET_OK;` |
|     28575 |  353 |  |
|  16063330 |  354 | `static void * MemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         2 |  355 |  |
|         - |  356 | `	SyMemHeader *pBucket,*pNext;` |
|         - |  357 | `	sxu32 nBucketSize;` |
|         - |  358 | `	sxu32 nBucket;` |
|         - |  359 |  |
|  16063332 |  360 | `	if( nByte + sizeof(SyMemHeader) >= SXMEM_POOL_MAXALLOC ){` |
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
|  16063332 |  371 | `	nBucket = 0;` |
|  16063332 |  372 | `	nBucketSize = SXMEM_POOL_MINALLOC;` |
|  80378718 |  373 | `	while( nByte + sizeof(SyMemHeader) > nBucketSize  ){` |
|  64315388 |  374 | `		nBucketSize <<= 1;` |
|  64315388 |  375 | `		nBucket++;` |
|         2 |  376 | `	}` |
|  16063332 |  377 | `	pBucket = pBackend->apPool[nBucket];` |
|  16063332 |  378 | `	if( pBucket == 0 ){` |
|         - |  379 | `		sxi32 rc;` |
|     57148 |  380 | `		rc = MemPoolBucketAlloc(&(*pBackend),nBucket);` |
|     57148 |  381 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  382 | `			return 0;` |
|         - |  383 | `		}` |
|     57148 |  384 | `		pBucket = pBackend->apPool[nBucket];` |
|     28573 |  385 | `	}` |
|         - |  386 | `	/* Remove from the free list */` |
|  16063332 |  387 | `	pNext = pBucket->pNext;` |
|  16063332 |  388 | `	pBackend->apPool[nBucket] = pNext;` |
|         - |  389 | `	/* Record bucket&magic number */` |
|  16063332 |  390 | `	pBucket->nBucket = (SXMEM_POOL_MAGIC << 16) \| nBucket;` |
|  16063332 |  391 | `	return (void *)&pBucket[1];` |
|   8031667 |  392 |  |
|  16063330 |  393 | `PH7_PRIVATE void * SyMemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         2 |  394 |  |
|         - |  395 | `	void *pChunk;` |
|         - |  396 | `#if defined(UNTRUST)` |
|         - |  397 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  398 | `		return 0;` |
|         - |  399 | `	}` |
|         - |  400 | `#endif` |
|  16063332 |  401 | `	if( pBackend->pMutexMethods ){` |
|      2996 |  402 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1497 |  403 | `	}` |
|  16063332 |  404 | `	pChunk = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|  16063332 |  405 | `	if( pBackend->pMutexMethods ){` |
|      2996 |  406 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1497 |  407 | `	}` |
|  16063332 |  408 | `	return pChunk;` |
|         2 |  409 |  |
|  10080050 |  410 | `static sxi32 MemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|         2 |  411 |  |
|         - |  412 | `	SyMemHeader *pHeader;` |
|         - |  413 | `	sxu32 nBucket;` |
|         - |  414 | `	/* Get the corresponding bucket */` |
|  10080052 |  415 | `	pHeader = (SyMemHeader *)(((char *)pChunk) - sizeof(SyMemHeader));` |
|         - |  416 | `	/* Sanity check to avoid misuse */` |
|  10080052 |  417 | `	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){` |
|         3 |  418 | `		return SXERR_CORRUPT;` |
|         - |  419 | `	}` |
|  10080050 |  420 | `	nBucket = pHeader->nBucket & 0xFFFF;` |
|  10080050 |  421 | `	if( nBucket == SXU16_HIGH ){` |
|         - |  422 | `		/* Free the big block */` |
|       ! 0 |  423 | `		MemBackendFree(&(*pBackend),pHeader);` |
|  10080050 |  424 | `	}else if( nBucket >= SXMEM_POOL_NBUCKETS + SXMEM_POOL_INCR ){` |
|         - |  425 | `		/* Corrupted or misused bucket index */` |
|       ! 0 |  426 | `		return SXERR_CORRUPT;` |
|       ! 0 |  427 | `	}else{` |
|         - |  428 | `		/* Return to the free list */` |
|  10080050 |  429 | `		pHeader->pNext = pBackend->apPool[nBucket];` |
|  10080050 |  430 | `		pBackend->apPool[nBucket] = pHeader;` |
|         - |  431 | `	}` |
|  10080050 |  432 | `	return SXRET_OK;` |
|   5040027 |  433 |  |
|  10080050 |  434 | `PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|         2 |  435 |  |
|         - |  436 | `	sxi32 rc;` |
|         - |  437 | `#if defined(UNTRUST)` |
|         - |  438 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pChunk == 0 ){` |
|         - |  439 | `		return SXERR_CORRUPT;` |
|         - |  440 | `	}` |
|         - |  441 | `#endif` |
|  10080052 |  442 | `	if( pBackend->pMutexMethods ){` |
|      2674 |  443 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1336 |  444 | `	}` |
|  10080052 |  445 | `	rc = MemBackendPoolFree(&(*pBackend),pChunk);` |
|  10080052 |  446 | `	if( pBackend->pMutexMethods ){` |
|      2674 |  447 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1336 |  448 | `	}` |
|  10080052 |  449 | `	return rc;` |
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
|      2994 |  508 | `PH7_PRIVATE sxi32 SyMemBackendInit(SyMemBackend *pBackend,ProcMemError xMemErr,void * pUserData)` |
|         2 |  509 |  |
|         - |  510 | `#if defined(UNTRUST)` |
|         - |  511 | `	if( pBackend == 0 ){` |
|         - |  512 | `		return SXERR_EMPTY;` |
|         - |  513 | `	}` |
|         - |  514 | `#endif` |
|         - |  515 | `	/* Zero the allocator first */` |
|      2996 |  516 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|      2996 |  517 | `	pBackend->xMemError = xMemErr;` |
|      2996 |  518 | `	pBackend->pUserData = pUserData;` |
|         - |  519 | `	/* Switch to the OS memory allocator */` |
|      2996 |  520 | `	pBackend->pMethods = &sOSAllocMethods;` |
|      2996 |  521 | `	if( pBackend->pMethods->xInit ){` |
|         - |  522 | `		/* Initialize the backend  */` |
|       ! 0 |  523 | `		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){` |
|       ! 0 |  524 | `			return SXERR_ABORT;` |
|         - |  525 | `		}` |
|       ! 0 |  526 | `	}` |
|         - |  527 | `#if defined(UNTRUST)` |
|         - |  528 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|         - |  529 | `#endif` |
|      2996 |  530 | `	return SXRET_OK;` |
|      1499 |  531 |  |
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
|      5986 |  560 | `PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend *pBackend,SyMemBackend *pParent)` |
|         2 |  561 |  |
|         - |  562 | `	sxu8 bInheritMutex;` |
|         - |  563 | `#if defined(UNTRUST)` |
|         - |  564 | `	if( pBackend == 0 \|\| SXMEM_BACKEND_CORRUPT(pParent) ){` |
|         - |  565 | `		return SXERR_CORRUPT;` |
|         - |  566 | `	}` |
|         - |  567 | `#endif` |
|         - |  568 | `	/* Zero the allocator first */` |
|      5988 |  569 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|      5988 |  570 | `	pBackend->pMethods  = pParent->pMethods;` |
|      5988 |  571 | `	pBackend->xMemError = pParent->xMemError;` |
|      5988 |  572 | `	pBackend->pUserData = pParent->pUserData;` |
|      5988 |  573 | `	bInheritMutex = pParent->pMutexMethods ? TRUE : FALSE;` |
|      5988 |  574 | `	if( bInheritMutex ){` |
|      2996 |  575 | `		pBackend->pMutexMethods = pParent->pMutexMethods;` |
|         - |  576 | `		/* Create a private mutex */` |
|      2996 |  577 | `		pBackend->pMutex = pBackend->pMutexMethods->xNew(SXMUTEX_TYPE_FAST);` |
|      2996 |  578 | `		if( pBackend->pMutex ==  0){` |
|       ! 0 |  579 | `			return SXERR_OS;` |
|         - |  580 | `		}` |
|      1497 |  581 | `	}` |
|         - |  582 | `#if defined(UNTRUST)` |
|         - |  583 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|         - |  584 | `#endif` |
|      5988 |  585 | `	return SXRET_OK;` |
|      2995 |  586 |  |
|      6284 |  587 | `static sxi32 MemBackendRelease(SyMemBackend *pBackend)` |
|         2 |  588 |  |
|         - |  589 | `	SyMemBlock *pBlock,*pNext;` |
|         - |  590 |  |
|      6286 |  591 | `	pBlock = pBackend->pBlocks;` |
|    458487 |  592 | `	for(;;){` |
|    916976 |  593 | `		if( pBackend->nBlock == 0 ){` |
|      1394 |  594 | `			break;` |
|         - |  595 | `		}` |
|    915584 |  596 | `		pNext  = pBlock->pNext;` |
|    915584 |  597 | `		pBackend->pMethods->xFree(pBlock);` |
|    915584 |  598 | `		pBlock = pNext;` |
|    915584 |  599 | `		pBackend->nBlock--;` |
|         - |  600 | `		/* LOOP ONE */` |
|    915584 |  601 | `		if( pBackend->nBlock == 0 ){` |
|      3638 |  602 | `			break;` |
|         - |  603 | `		}` |
|    911948 |  604 | `		pNext  = pBlock->pNext;` |
|    911948 |  605 | `		pBackend->pMethods->xFree(pBlock);` |
|    911948 |  606 | `		pBlock = pNext;` |
|    911948 |  607 | `		pBackend->nBlock--;` |
|         - |  608 | `		/* LOOP TWO */` |
|    911948 |  609 | `		if( pBackend->nBlock == 0 ){` |
|       391 |  610 | `			break;` |
|         - |  611 | `		}` |
|    911558 |  612 | `		pNext  = pBlock->pNext;` |
|    911558 |  613 | `		pBackend->pMethods->xFree(pBlock);` |
|    911558 |  614 | `		pBlock = pNext;` |
|    911558 |  615 | `		pBackend->nBlock--;` |
|         - |  616 | `		/* LOOP THREE */` |
|    911558 |  617 | `		if( pBackend->nBlock == 0 ){` |
|       867 |  618 | `			break;` |
|         - |  619 | `		}` |
|    910692 |  620 | `		pNext  = pBlock->pNext;` |
|    910692 |  621 | `		pBackend->pMethods->xFree(pBlock);` |
|    910692 |  622 | `		pBlock = pNext;` |
|    910692 |  623 | `		pBackend->nBlock--;` |
|         - |  624 | `		/* LOOP FOUR */` |
|         2 |  625 | `	}` |
|      6286 |  626 | `	if( pBackend->pMethods->xRelease ){` |
|       ! 0 |  627 | `		pBackend->pMethods->xRelease(pBackend->pMethods->pUserData);` |
|       ! 0 |  628 | `	}` |
|      6286 |  629 | `	pBackend->pMethods = 0;` |
|      6286 |  630 | `	pBackend->pBlocks  = 0;` |
|         - |  631 | `#if defined(UNTRUST)` |
|         - |  632 | `	pBackend->nMagic = 0x2626;` |
|         - |  633 | `#endif` |
|      6286 |  634 | `	return SXRET_OK;` |
|         2 |  635 |  |
|      6284 |  636 | `PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend *pBackend)` |
|         2 |  637 |  |
|         - |  638 | `#if defined(UNTRUST)` |
|         - |  639 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  640 | `		return SXERR_INVALID;` |
|         - |  641 | `	}` |
|         - |  642 | `#endif` |
|      6286 |  643 | `	if( pBackend->pMutexMethods ){` |
|       315 |  644 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       157 |  645 | `	}` |
|      6286 |  646 | `	(void)MemBackendRelease(&(*pBackend));` |
|      6286 |  647 | `	if( pBackend->pMutexMethods ){` |
|       315 |  648 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       315 |  649 | `		SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|       157 |  650 | `	}` |
|      6286 |  651 | `	return SXRET_OK;` |
|         2 |  652 |  |
|    620470 |  653 | `PH7_PRIVATE void * SyMemBackendDup(SyMemBackend *pBackend,const void *pSrc,sxu32 nSize)` |
|         2 |  654 |  |
|         - |  655 | `	void *pNew;` |
|         - |  656 | `#if defined(UNTRUST)` |
|         - |  657 | `	if( pSrc == 0 \|\| nSize <= 0 ){` |
|         - |  658 | `		return 0;` |
|         - |  659 | `	}` |
|         - |  660 | `#endif` |
|    620472 |  661 | `	pNew = SyMemBackendAlloc(&(*pBackend),nSize);` |
|    620472 |  662 | `	if( pNew ){` |
|    620472 |  663 | `		SyMemcpy(pSrc,pNew,nSize);` |
|    310235 |  664 | `	}` |
|    620472 |  665 | `	return pNew;` |
|         2 |  666 |  |
|   2342824 |  667 | `PH7_PRIVATE char * SyMemBackendStrDup(SyMemBackend *pBackend,const char *zSrc,sxu32 nSize)` |
|         2 |  668 |  |
|         - |  669 | `	char *zDest;` |
|   2342826 |  670 | `	zDest = (char *)SyMemBackendAlloc(&(*pBackend),nSize + 1);` |
|   2342826 |  671 | `	if( zDest ){` |
|   2342826 |  672 | `		Systrcpy(zDest,nSize+1,zSrc,nSize);` |
|   1171412 |  673 | `	}` |
|   2342826 |  674 | `	return zDest;` |
|         2 |  675 |  |
|    195798 |  676 | `PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize)` |
|         2 |  677 |  |
|         - |  678 | `#if defined(UNTRUST)` |
|         - |  679 | `	if( pBlob == 0 \|\| pBuffer == 0 \|\| nSize < 1 ){` |
|         - |  680 | `		return SXERR_EMPTY;` |
|         - |  681 | `	}` |
|         - |  682 | `#endif` |
|    195800 |  683 | `	pBlob->pBlob = pBuffer;` |
|    195800 |  684 | `	pBlob->mByte = nSize;` |
|    195800 |  685 | `	pBlob->nByte = 0;` |
|    195800 |  686 | `	pBlob->pAllocator = 0;` |
|    195800 |  687 | `	pBlob->nFlags = SXBLOB_LOCKED\|SXBLOB_STATIC;` |
|    195800 |  688 | `	return SXRET_OK;` |
|         2 |  689 |  |
|   7991036 |  690 | `PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator)` |
|         2 |  691 |  |
|         - |  692 | `#if defined(UNTRUST)` |
|         - |  693 | `	if( pBlob == 0  ){` |
|         - |  694 | `		return SXERR_EMPTY;` |
|         - |  695 | `	}` |
|         - |  696 | `#endif` |
|   7991038 |  697 | `	pBlob->pBlob = 0;` |
|   7991038 |  698 | `	pBlob->mByte = pBlob->nByte	= 0;` |
|   7991038 |  699 | `	pBlob->pAllocator = &(*pAllocator);` |
|   7991038 |  700 | `	pBlob->nFlags = 0;` |
|   7991038 |  701 | `	return SXRET_OK;` |
|         2 |  702 |  |
|   2984328 |  703 | `PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte)` |
|         2 |  704 |  |
|         - |  705 | `#if defined(UNTRUST)` |
|         - |  706 | `	if( pBlob == 0  ){` |
|         - |  707 | `		return SXERR_EMPTY;` |
|         - |  708 | `	}` |
|         - |  709 | `#endif` |
|   2984330 |  710 | `	pBlob->pBlob = (void *)pData;` |
|   2984330 |  711 | `	pBlob->nByte = nByte;` |
|   2984330 |  712 | `	pBlob->mByte = 0;` |
|   2984330 |  713 | `	pBlob->nFlags \|= SXBLOB_RDONLY;` |
|   2984330 |  714 | `	return SXRET_OK;` |
|         2 |  715 |  |
|         - |  716 | `#ifndef SXBLOB_MIN_GROWTH` |
|         - |  717 | `#define SXBLOB_MIN_GROWTH 16` |
|         - |  718 | `#endif` |
|   8144764 |  719 | `static sxi32 BlobPrepareGrow(SyBlob *pBlob,sxu32 *pByte)` |
|         2 |  720 |  |
|         - |  721 | `	sxu32 nByte;` |
|         - |  722 | `	void *pNew;` |
|   8144766 |  723 | `	nByte = *pByte;` |
|   8144766 |  724 | `	if( pBlob->nFlags & (SXBLOB_LOCKED\|SXBLOB_STATIC) ){` |
|   1563550 |  725 | `		if ( SyBlobFreeSpace(pBlob) < nByte ){` |
|       ! 0 |  726 | `			*pByte = SyBlobFreeSpace(pBlob);` |
|       ! 0 |  727 | `			if( (*pByte) == 0 ){` |
|       ! 0 |  728 | `				return SXERR_SHORT;` |
|         - |  729 | `			}` |
|       ! 0 |  730 | `		}` |
|   1563550 |  731 | `		return SXRET_OK;` |
|         - |  732 | `	}` |
|   6581218 |  733 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|         - |  734 | `		/* Make a copy of the read-only item */` |
|    620472 |  735 | `		if( pBlob->nByte > 0 ){` |
|    620472 |  736 | `			pNew = SyMemBackendDup(pBlob->pAllocator,pBlob->pBlob,pBlob->nByte);` |
|    620472 |  737 | `			if( pNew == 0 ){` |
|       ! 0 |  738 | `				return SXERR_MEM;` |
|         - |  739 | `			}` |
|    620472 |  740 | `			pBlob->pBlob = pNew;` |
|    620472 |  741 | `			pBlob->mByte = pBlob->nByte;` |
|    310237 |  742 | `		}else{` |
|       ! 0 |  743 | `			pBlob->pBlob = 0;` |
|       ! 0 |  744 | `			pBlob->mByte = 0;` |
|         - |  745 | `		}` |
|         - |  746 | `		/* Remove the read-only flag */` |
|    620472 |  747 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|    310235 |  748 | `	}` |
|   6581218 |  749 | `	if( SyBlobFreeSpace(pBlob) >= nByte ){` |
|   1336095 |  750 | `		return SXRET_OK;` |
|         - |  751 | `	}` |
|   5245125 |  752 | `	if( pBlob->mByte > 0 ){` |
|    724257 |  753 | `		nByte = nByte + pBlob->mByte * 2 + SXBLOB_MIN_GROWTH;` |
|   4882985 |  754 | `	}else if ( nByte < SXBLOB_MIN_GROWTH ){` |
|   3642763 |  755 | `		nByte = SXBLOB_MIN_GROWTH;` |
|   1821277 |  756 | `	}` |
|   5245125 |  757 | `	pNew = SyMemBackendRealloc(pBlob->pAllocator,pBlob->pBlob,nByte);` |
|   5245125 |  758 | `	if( pNew == 0 ){` |
|       ! 0 |  759 | `		return SXERR_MEM;` |
|         - |  760 | `	}` |
|   5245125 |  761 | `	pBlob->pBlob = pNew;` |
|   5245125 |  762 | `	pBlob->mByte = nByte;` |
|   5245125 |  763 | `	return SXRET_OK;` |
|   4072428 |  764 |  |
|   8198254 |  765 | `PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize)` |
|         2 |  766 |  |
|         - |  767 | `	sxu8 *zBlob;` |
|         - |  768 | `	sxi32 rc;` |
|   8198256 |  769 | `	if( nSize < 1 ){` |
|     53492 |  770 | `		return SXRET_OK;` |
|         - |  771 | `	}` |
|   8144766 |  772 | `	rc = BlobPrepareGrow(&(*pBlob),&nSize);` |
|   8144766 |  773 | `	if( SXRET_OK != rc ){` |
|       ! 0 |  774 | `		return rc;` |
|         - |  775 | `	}` |
|   8144766 |  776 | `	if( pData ){` |
|   8144734 |  777 | `		zBlob = (sxu8 *)pBlob->pBlob ;` |
|   8144734 |  778 | `		zBlob = &zBlob[pBlob->nByte];` |
|   8144734 |  779 | `		pBlob->nByte += nSize;` |
|  36255145 |  780 | `		SX_MACRO_FAST_MEMCPY(pData,zBlob,nSize);` |
|   4072410 |  781 | `	}` |
|   8144766 |  782 | `	return SXRET_OK;` |
|   4099173 |  783 |  |
|    624276 |  784 | `PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob)` |
|         2 |  785 |  |
|         - |  786 | `	sxi32 rc;` |
|         - |  787 | `	sxu32 n;` |
|    624278 |  788 | `	n = pBlob->nByte;` |
|    624278 |  789 | `	rc = SyBlobAppend(&(*pBlob),(const void *)"\0",sizeof(char));` |
|    624278 |  790 | `	if (rc == SXRET_OK ){` |
|    624278 |  791 | `		pBlob->nByte = n;` |
|    312160 |  792 | `	}` |
|    624278 |  793 | `	return rc;` |
|         2 |  794 |  |
|   3652710 |  795 | `PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest)` |
|         2 |  796 |  |
|   3652712 |  797 | `	sxi32 rc = SXRET_OK;` |
|         - |  798 | `#ifdef UNTRUST` |
|         - |  799 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|         - |  800 | `		return SXERR_EMPTY;` |
|         - |  801 | `	}` |
|         - |  802 | `#endif` |
|   3652712 |  803 | `	if( pSrc->nByte > 0 ){` |
|   3628152 |  804 | `		rc = SyBlobAppend(&(*pDest),pSrc->pBlob,pSrc->nByte);` |
|   1814075 |  805 | `	}` |
|   3652712 |  806 | `	return rc;` |
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
|   4237596 |  827 | `PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob)` |
|         2 |  828 |  |
|   4237598 |  829 | `	pBlob->nByte = 0;` |
|   4237598 |  830 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|      9016 |  831 | `		pBlob->pBlob = 0;` |
|      9016 |  832 | `		pBlob->mByte = 0;` |
|      9016 |  833 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|      4507 |  834 | `	}` |
|   4237598 |  835 | `	return SXRET_OK;` |
|         2 |  836 |  |
|  11110816 |  837 | `PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob)` |
|         2 |  838 |  |
|  11110818 |  839 | `	if( (pBlob->nFlags & (SXBLOB_STATIC\|SXBLOB_RDONLY)) == 0 && pBlob->mByte > 0 ){` |
|   4767484 |  840 | `		SyMemBackendFree(pBlob->pAllocator,pBlob->pBlob);` |
|   2383763 |  841 | `	}` |
|  11110818 |  842 | `	pBlob->pBlob = 0;` |
|  11110818 |  843 | `	pBlob->nByte = pBlob->mByte = 0;` |
|  11110818 |  844 | `	pBlob->nFlags = 0;` |
|  11110818 |  845 | `	return SXRET_OK;` |
|         2 |  846 |  |
|         - |  847 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    143164 |  848 | `PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft)` |
|         2 |  849 |  |
|    143166 |  850 | `	const char *zIn = (const char *)pBlob;` |
|         - |  851 | `	const char *zEnd;` |
|         - |  852 | `	sxi32 rc;` |
|    143166 |  853 | `	if( pLen > nLen ){` |
|      5474 |  854 | `		return SXERR_NOTFOUND;` |
|         - |  855 | `	}` |
|    137694 |  856 | `	zEnd = &zIn[nLen-pLen];` |
|   1147090 |  857 | `	for(;;){` |
|   2294121 |  858 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   2259534 |  859 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   2210629 |  860 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   2180404 |  861 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|         2 |  862 | `	}` |
|     21332 |  863 | `	return SXERR_NOTFOUND;` |
|     71584 |  864 |  |
|         - |  865 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - |  866 |  |
