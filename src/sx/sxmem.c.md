# src/sx/sxmem.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 435/510 lines (85.29%)

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
|  15650371 |   18 | `static void * SyOSHeapAlloc(sxu32 nByte)` |
|         5 |   19 |  |
|         - |   20 | `	void *pNew;` |
|         - |   21 | `#if defined(__WINNT__)` |
|         5 |   22 | `	pNew = HeapAlloc(GetProcessHeap(),0,nByte);` |
|         - |   23 | `#else` |
|  15650371 |   24 | `	pNew = malloc((size_t)nByte);` |
|         - |   25 | `#endif` |
|  15650376 |   26 | `	return pNew;` |
|         5 |   27 |  |
|    961412 |   28 | `static void * SyOSHeapRealloc(void *pOld,sxu32 nByte)` |
|         5 |   29 |  |
|         - |   30 | `	void *pNew;` |
|         - |   31 | `#if defined(__WINNT__)` |
|         5 |   32 | `	pNew = HeapReAlloc(GetProcessHeap(),0,pOld,nByte);` |
|         - |   33 | `#else` |
|    961412 |   34 | `	pNew = realloc(pOld,(size_t)nByte);` |
|         - |   35 | `#endif` |
|    961417 |   36 | `	return pNew;` |
|         5 |   37 |  |
|  15647169 |   38 | `static void SyOSHeapFree(void *pPtr)` |
|         5 |   39 |  |
|         - |   40 | `#if defined(__WINNT__)` |
|         5 |   41 | `	HeapFree(GetProcessHeap(),0,pPtr);` |
|         - |   42 | `#else` |
|  15647169 |   43 | `	free(pPtr);` |
|         - |   44 | `#endif` |
|  15647174 |   45 |  |
|         - |   46 |  |
|         - |   47 |  |
|  28467349 |   48 | `PH7_PRIVATE void SyZero(void *pSrc,sxu32 nSize)` |
|         5 |   49 |  |
|  28467354 |   50 | `	register unsigned char *zSrc = (unsigned char *)pSrc;` |
|         - |   51 | `	unsigned char *zEnd;` |
|         - |   52 | `#if defined(UNTRUST)` |
|         - |   53 | `	if( zSrc == 0 \|\| nSize <= 0 ){` |
|         - |   54 | `		return ;` |
|         - |   55 | `	}` |
|         - |   56 | `#endif` |
|  28467354 |   57 | `	zEnd = &zSrc[nSize];` |
| 384071445 |   58 | `	for(;;){` |
| 768139418 |   59 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 739672309 |   60 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 739672223 |   61 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 739672101 |   62 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
|         5 |   63 | `	}` |
|  28467354 |   64 |  |
|  28714858 |   65 | `PH7_PRIVATE sxi32 SyMemcmp(const void *pB1,const void *pB2,sxu32 nSize)` |
|         5 |   66 |  |
|         - |   67 | `	sxi32 rc;` |
|  28714863 |   68 | `	if( nSize <= 0 ){` |
|        93 |   69 | `		return 0;` |
|         - |   70 | `	}` |
|  28714771 |   71 | `	if( pB1 == 0 \|\| pB2 == 0 ){` |
|       ! 0 |   72 | `		return pB1 != 0 ? 1 : (pB2 == 0 ? 0 : -1);` |
|         - |   73 | `	}` |
|  62329014 |   74 | `	SX_MACRO_FAST_CMP(pB1,pB2,nSize,rc);` |
|  28714771 |   75 | `	return rc;` |
|  14358205 |   76 |  |
|  11398171 |   77 | `PH7_PRIVATE sxu32 SyMemcpy(const void *pSrc,void *pDest,sxu32 nLen)` |
|         5 |   78 |  |
|         - |   79 | `#if defined(UNTRUST)` |
|         - |   80 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|         - |   81 | `		return 0;` |
|         - |   82 | `	}` |
|         - |   83 | `#endif` |
|  11398176 |   84 | `	if( pSrc == (const void *)pDest ){` |
|       ! 0 |   85 | `		return nLen;` |
|         - |   86 | `	}` |
|  93543868 |   87 | `	SX_MACRO_FAST_MEMCPY(pSrc,pDest,nLen);` |
|  11398176 |   88 | `	return nLen;` |
|   5699439 |   89 |  |
|  15650371 |   90 | `static void * MemOSAlloc(sxu32 nBytes)` |
|         5 |   91 |  |
|         - |   92 | `	sxu32 *pChunk;` |
|  15650376 |   93 | `	pChunk = (sxu32 *)SyOSHeapAlloc(nBytes + sizeof(sxu32));` |
|  15650376 |   94 | `	if( pChunk == 0 ){` |
|       ! 0 |   95 | `		return 0;` |
|         - |   96 | `	}` |
|  15650376 |   97 | `	pChunk[0] = nBytes;` |
|  15650376 |   98 | `	return (void *)&pChunk[1];` |
|   7825233 |   99 |  |
|    961412 |  100 | `static void * MemOSRealloc(void *pOld,sxu32 nBytes)` |
|         5 |  101 |  |
|         - |  102 | `	sxu32 *pOldChunk;` |
|         - |  103 | `	sxu32 *pChunk;` |
|    961417 |  104 | `	pOldChunk = (sxu32 *)(((char *)pOld)-sizeof(sxu32));` |
|    961417 |  105 | `	if( pOldChunk[0] >= nBytes ){` |
|       ! 0 |  106 | `		return pOld;` |
|         - |  107 | `	}` |
|    961417 |  108 | `	pChunk = (sxu32 *)SyOSHeapRealloc(pOldChunk,nBytes + sizeof(sxu32));` |
|    961417 |  109 | `	if( pChunk == 0 ){` |
|       ! 0 |  110 | `		return 0;` |
|         - |  111 | `	}` |
|    961417 |  112 | `	pChunk[0] = nBytes;` |
|    961417 |  113 | `	return (void *)&pChunk[1];` |
|    480708 |  114 |  |
|  15647169 |  115 | `static void MemOSFree(void *pBlock)` |
|         5 |  116 |  |
|         - |  117 | `	void *pChunk;` |
|  15647174 |  118 | `	pChunk = (void *)(((char *)pBlock)-sizeof(sxu32));` |
|  15647174 |  119 | `	SyOSHeapFree(pChunk);` |
|  15647174 |  120 |  |
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
|  15650371 |  137 | `static void * MemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         5 |  138 |  |
|         - |  139 | `	SyMemBlock *pBlock;` |
|  15650376 |  140 | `	sxi32 nRetry = 0;` |
|         - |  141 |  |
|         - |  142 | `	/* Append an extra block so we can tracks allocated chunks and avoid memory` |
|         - |  143 | `	 * leaks.` |
|         - |  144 | `	 */` |
|  15650376 |  145 | `	nByte += sizeof(SyMemBlock);` |
|         - |  146 | `	/* Enforce the optional per-allocation cap (0 = unlimited). A capped failure` |
|         - |  147 | `	 * returns NULL just like a genuine OS failure, driving the normal SXERR_MEM` |
|         - |  148 | `	 * propagation; the retry callback is intentionally skipped (hard limit). */` |
|  15650376 |  149 | `	if( pBackend->nMaxRequest && nByte > pBackend->nMaxRequest ){` |
|       ! 0 |  150 | `		return 0;` |
|         - |  151 | `	}` |
|   7825228 |  152 | `	for(;;){` |
|   7825233 |  153 | `		pBlock = (SyMemBlock *)pBackend->pMethods->xAlloc(nByte);` |
|  15650371 |  154 | `		if( pBlock != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY` |
|         5 |  155 | `			\|\| SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|   7825233 |  156 | `				break;` |
|         - |  157 | `		}` |
|       ! 0 |  158 | `		nRetry++;` |
|       ! 0 |  159 | `	}` |
|  15650376 |  160 | `	if( pBlock  == 0 ){` |
|       ! 0 |  161 | `		return 0;` |
|         - |  162 | `	}` |
|  15650376 |  163 | `	pBlock->pNext = pBlock->pPrev = 0;` |
|         - |  164 | `	/* Link to the list of already tracked blocks */` |
|  15650376 |  165 | `	MACRO_LD_PUSH(pBackend->pBlocks,pBlock);` |
|         - |  166 | `#if defined(UNTRUST)` |
|         - |  167 | `	pBlock->nGuard = SXMEM_BACKEND_MAGIC;` |
|         - |  168 | `#endif` |
|  15650376 |  169 | `	pBackend->nBlock++;` |
|  15650376 |  170 | `	return (void *)&pBlock[1];` |
|   7825233 |  171 |  |
|   6068622 |  172 | `PH7_PRIVATE void * SyMemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         5 |  173 |  |
|         - |  174 | `	void *pChunk;` |
|         - |  175 | `#if defined(UNTRUST)` |
|         - |  176 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  177 | `		return 0;` |
|         - |  178 | `	}` |
|         - |  179 | `#endif` |
|   6068627 |  180 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  181 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  182 | `	}` |
|   6068627 |  183 | `	pChunk = MemBackendAlloc(&(*pBackend),nByte);` |
|   6068627 |  184 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  185 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  186 | `	}` |
|   6068627 |  187 | `	return pChunk;` |
|         5 |  188 |  |
|  10474449 |  189 | `static void * MemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         5 |  190 |  |
|         - |  191 | `	SyMemBlock *pBlock,*pNew,*pPrev,*pNext;` |
|  10474454 |  192 | `	sxu32 nRetry = 0;` |
|         - |  193 |  |
|  10474454 |  194 | `	if( pOld == 0 ){` |
|   9513042 |  195 | `		return MemBackendAlloc(&(*pBackend),nByte);` |
|         - |  196 | `	}` |
|    961417 |  197 | `	pBlock = (SyMemBlock *)(((char *)pOld) - sizeof(SyMemBlock));` |
|         - |  198 | `#if defined(UNTRUST)` |
|         - |  199 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|         - |  200 | `		return 0;` |
|         - |  201 | `	}` |
|         - |  202 | `#endif` |
|    961417 |  203 | `	nByte += sizeof(SyMemBlock);` |
|         - |  204 | `	/* Enforce the optional per-allocation cap (0 = unlimited); see MemBackendAlloc. */` |
|    961417 |  205 | `	if( pBackend->nMaxRequest && nByte > pBackend->nMaxRequest ){` |
|       ! 0 |  206 | `		return 0;` |
|         - |  207 | `	}` |
|    961417 |  208 | `	pPrev = pBlock->pPrev;` |
|    961417 |  209 | `	pNext = pBlock->pNext;` |
|    480703 |  210 | `	for(;;){` |
|    480708 |  211 | `		pNew = (SyMemBlock *)pBackend->pMethods->xRealloc(pBlock,nByte);` |
|    961417 |  212 | `		if( pNew != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY \|\|` |
|       ! 0 |  213 | `			SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|    480708 |  214 | `				break;` |
|         - |  215 | `		}` |
|       ! 0 |  216 | `		nRetry++;` |
|       ! 0 |  217 | `	}` |
|    961417 |  218 | `	if( pNew == 0 ){` |
|       ! 0 |  219 | `		return 0;` |
|         - |  220 | `	}` |
|    961417 |  221 | `	if( pNew != pBlock ){` |
|    881050 |  222 | `		if( pPrev == 0 ){` |
|    668014 |  223 | `			pBackend->pBlocks = pNew;` |
|    363488 |  224 | `		}else{` |
|    213041 |  225 | `			pPrev->pNext = pNew;` |
|         - |  226 | `		}` |
|    881050 |  227 | `		if( pNext ){` |
|    881038 |  228 | `			pNext->pPrev = pNew;` |
|    478910 |  229 | `		}` |
|         - |  230 | `#if defined(UNTRUST)` |
|         - |  231 | `		pNew->nGuard = SXMEM_BACKEND_MAGIC;` |
|         - |  232 | `#endif` |
|    478917 |  233 | `	}` |
|    961417 |  234 | `	return (void *)&pNew[1];` |
|   5237269 |  235 |  |
|  10474449 |  236 | `PH7_PRIVATE void * SyMemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         5 |  237 |  |
|         - |  238 | `	void *pChunk;` |
|         - |  239 | `#if defined(UNTRUST)` |
|         - |  240 | `	if( SXMEM_BACKEND_CORRUPT(pBackend)  ){` |
|         - |  241 | `		return 0;` |
|         - |  242 | `	}` |
|         - |  243 | `#endif` |
|  10474454 |  244 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  245 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  246 | `	}` |
|  10474454 |  247 | `	pChunk = MemBackendRealloc(&(*pBackend),pOld,nByte);` |
|  10474454 |  248 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  249 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  250 | `	}` |
|  10474454 |  251 | `	return pChunk;` |
|         5 |  252 |  |
|  10552931 |  253 | `static sxi32 MemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|         5 |  254 |  |
|         - |  255 | `	SyMemBlock *pBlock;` |
|  10552936 |  256 | `	pBlock = (SyMemBlock *)(((char *)pChunk) - sizeof(SyMemBlock));` |
|         - |  257 | `#if defined(UNTRUST)` |
|         - |  258 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|         - |  259 | `		return SXERR_CORRUPT;` |
|         - |  260 | `	}` |
|         - |  261 | `#endif` |
|         - |  262 | `	/* Unlink from the list of active blocks */` |
|  10552936 |  263 | `	if( pBackend->nBlock > 0 ){` |
|         - |  264 | `		/* Release the block */` |
|         - |  265 | `#if defined(UNTRUST)` |
|         - |  266 | `		/* Mark as stale block */` |
|         - |  267 | `		pBlock->nGuard = 0x635B;` |
|         - |  268 | `#endif` |
|  10552936 |  269 | `		MACRO_LD_REMOVE(pBackend->pBlocks,pBlock);` |
|  10552936 |  270 | `		pBackend->nBlock--;` |
|  10552936 |  271 | `		pBackend->pMethods->xFree(pBlock);` |
|   5276508 |  272 | `	}` |
|  10552936 |  273 | `	return SXRET_OK;` |
|         5 |  274 |  |
|  10552931 |  275 | `PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|         5 |  276 |  |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|  10552936 |  283 | `	if( pChunk == 0 ){` |
|       ! 0 |  284 | `		return SXRET_OK;` |
|         - |  285 | `	}` |
|  10552936 |  286 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  287 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  288 | `	}` |
|  10552936 |  289 | `	rc = MemBackendFree(&(*pBackend),pChunk);` |
|  10552936 |  290 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  291 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  292 | `	}` |
|  10552936 |  293 | `	return rc;` |
|   5276513 |  294 |  |
|         - |  295 | `#if defined(PH7_ENABLE_THREADS)` |
|      3548 |  296 | `PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods)` |
|         5 |  297 |  |
|         - |  298 | `	SyMutex *pMutex;` |
|         - |  299 | `#if defined(UNTRUST)` |
|         - |  300 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pMethods == 0 \|\| pMethods->xNew == 0){` |
|         - |  301 | `		return SXERR_CORRUPT;` |
|         - |  302 | `	}` |
|         - |  303 | `#endif` |
|      3553 |  304 | `	pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|      3553 |  305 | `	if( pMutex == 0 ){` |
|       ! 0 |  306 | `		return SXERR_OS;` |
|         - |  307 | `	}` |
|         - |  308 | `	/* Attach the mutex to the memory backend */` |
|      3553 |  309 | `	pBackend->pMutex = pMutex;` |
|      3553 |  310 | `	pBackend->pMutexMethods = pMethods;` |
|      3553 |  311 | `	return SXRET_OK;` |
|      1779 |  312 |  |
|      3548 |  313 | `PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend)` |
|         5 |  314 |  |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  317 | `		return SXERR_CORRUPT;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|      3553 |  320 | `	if( pBackend->pMutex == 0 ){` |
|         - |  321 | `		/* There is no mutex subsystem at all */` |
|       ! 0 |  322 | `		return SXRET_OK;` |
|         - |  323 | `	}` |
|      3553 |  324 | `	SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|      3553 |  325 | `	pBackend->pMutexMethods = 0;` |
|      3553 |  326 | `	pBackend->pMutex = 0;` |
|      3553 |  327 | `	return SXRET_OK;` |
|      1779 |  328 |  |
|         - |  329 | `#endif` |
|         - |  330 | `/*` |
|         - |  331 | ` * Memory pool allocator` |
|         - |  332 | ` */` |
|         - |  333 | `#define SXMEM_POOL_MAGIC		0xDEAD` |
|         - |  334 | `#define SXMEM_POOL_MAXALLOC		(1<<(SXMEM_POOL_NBUCKETS+SXMEM_POOL_INCR))` |
|         - |  335 | `#define SXMEM_POOL_MINALLOC		(1<<(SXMEM_POOL_INCR))` |
|     68712 |  336 | `static sxi32 MemPoolBucketAlloc(SyMemBackend *pBackend,sxu32 nBucket)` |
|         5 |  337 |  |
|         - |  338 | `	char *zBucket,*zBucketEnd;` |
|         - |  339 | `	SyMemHeader *pHeader;` |
|         - |  340 | `	sxu32 nBucketSize;` |
|         - |  341 |  |
|         - |  342 | `	/* Allocate one big block first */` |
|     68717 |  343 | `	zBucket = (char *)MemBackendAlloc(&(*pBackend),SXMEM_POOL_MAXALLOC);` |
|     68717 |  344 | `	if( zBucket == 0 ){` |
|       ! 0 |  345 | `		return SXERR_MEM;` |
|         - |  346 | `	}` |
|     68717 |  347 | `	zBucketEnd = &zBucket[SXMEM_POOL_MAXALLOC];` |
|         - |  348 | `	/* Divide the big block into mini bucket pool */` |
|     68717 |  349 | `	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);` |
|     68717 |  350 | `	pBackend->apPool[nBucket] = pHeader = (SyMemHeader *)zBucket;` |
|   7815264 |  351 | `	for(;;){` |
|  15630533 |  352 | `		if( &zBucket[nBucketSize] >= zBucketEnd ){` |
|     68717 |  353 | `			break;` |
|         - |  354 | `		}` |
|  15561821 |  355 | `		pHeader->pNext = (SyMemHeader *)&zBucket[nBucketSize];` |
|         - |  356 | `		/* Advance the cursor to the next available chunk */` |
|  15561821 |  357 | `		pHeader = pHeader->pNext;` |
|  15561821 |  358 | `		zBucket += nBucketSize;` |
|         5 |  359 | `	}` |
|     68717 |  360 | `	pHeader->pNext = 0;` |
|         - |  361 |  |
|     68717 |  362 | `	return SXRET_OK;` |
|     34361 |  363 |  |
|  19466552 |  364 | `static void * MemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         5 |  365 |  |
|         - |  366 | `	SyMemHeader *pBucket,*pNext;` |
|         - |  367 | `	sxu32 nBucketSize;` |
|         - |  368 | `	sxu32 nBucket;` |
|         - |  369 |  |
|  19466557 |  370 | `	if( nByte + sizeof(SyMemHeader) >= SXMEM_POOL_MAXALLOC ){` |
|         - |  371 | `		/* Allocate a big chunk directly */` |
|       ! 0 |  372 | `		pBucket = (SyMemHeader *)MemBackendAlloc(&(*pBackend),nByte+sizeof(SyMemHeader));` |
|       ! 0 |  373 | `		if( pBucket == 0 ){` |
|       ! 0 |  374 | `			return 0;` |
|         - |  375 | `		}` |
|         - |  376 | `		/* Record as big block */` |
|       ! 0 |  377 | `		pBucket->nBucket = (sxu32)(SXMEM_POOL_MAGIC << 16) \| SXU16_HIGH;` |
|       ! 0 |  378 | `		return (void *)(pBucket+1);` |
|         - |  379 | `	}` |
|         - |  380 | `	/* Locate the appropriate bucket */` |
|  19466557 |  381 | `	nBucket = 0;` |
|  19466557 |  382 | `	nBucketSize = SXMEM_POOL_MINALLOC;` |
|  97511333 |  383 | `	while( nByte + sizeof(SyMemHeader) > nBucketSize  ){` |
|  78044781 |  384 | `		nBucketSize <<= 1;` |
|  78044781 |  385 | `		nBucket++;` |
|         5 |  386 | `	}` |
|  19466557 |  387 | `	pBucket = pBackend->apPool[nBucket];` |
|  19466557 |  388 | `	if( pBucket == 0 ){` |
|         - |  389 | `		sxi32 rc;` |
|     68717 |  390 | `		rc = MemPoolBucketAlloc(&(*pBackend),nBucket);` |
|     68717 |  391 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  392 | `			return 0;` |
|         - |  393 | `		}` |
|     68717 |  394 | `		pBucket = pBackend->apPool[nBucket];` |
|     34356 |  395 | `	}` |
|         - |  396 | `	/* Remove from the free list */` |
|  19466557 |  397 | `	pNext = pBucket->pNext;` |
|  19466557 |  398 | `	pBackend->apPool[nBucket] = pNext;` |
|         - |  399 | `	/* Record bucket&magic number */` |
|  19466557 |  400 | `	pBucket->nBucket = (SXMEM_POOL_MAGIC << 16) \| nBucket;` |
|  19466557 |  401 | `	return (void *)&pBucket[1];` |
|   9733281 |  402 |  |
|  19466552 |  403 | `PH7_PRIVATE void * SyMemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         5 |  404 |  |
|         - |  405 | `	void *pChunk;` |
|         - |  406 | `#if defined(UNTRUST)` |
|         - |  407 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  408 | `		return 0;` |
|         - |  409 | `	}` |
|         - |  410 | `#endif` |
|  19466557 |  411 | `	if( pBackend->pMutexMethods ){` |
|      3553 |  412 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1774 |  413 | `	}` |
|  19466557 |  414 | `	pChunk = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|  19466557 |  415 | `	if( pBackend->pMutexMethods ){` |
|      3553 |  416 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1774 |  417 | `	}` |
|  19466557 |  418 | `	return pChunk;` |
|         5 |  419 |  |
|  10949134 |  420 | `static sxi32 MemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|         5 |  421 |  |
|         - |  422 | `	SyMemHeader *pHeader;` |
|         - |  423 | `	sxu32 nBucket;` |
|         - |  424 | `	/* Get the corresponding bucket */` |
|  10949139 |  425 | `	pHeader = (SyMemHeader *)(((char *)pChunk) - sizeof(SyMemHeader));` |
|         - |  426 | `	/* Sanity check to avoid misuse */` |
|  10949139 |  427 | `	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){` |
|         3 |  428 | `		return SXERR_CORRUPT;` |
|         - |  429 | `	}` |
|  10949137 |  430 | `	nBucket = pHeader->nBucket & 0xFFFF;` |
|  10949137 |  431 | `	if( nBucket == SXU16_HIGH ){` |
|         - |  432 | `		/* Free the big block */` |
|       ! 0 |  433 | `		MemBackendFree(&(*pBackend),pHeader);` |
|  10949137 |  434 | `	}else if( nBucket >= SXMEM_POOL_NBUCKETS + SXMEM_POOL_INCR ){` |
|         - |  435 | `		/* Corrupted or misused bucket index */` |
|       ! 0 |  436 | `		return SXERR_CORRUPT;` |
|       ! 0 |  437 | `	}else{` |
|         - |  438 | `		/* Return to the free list */` |
|  10949137 |  439 | `		pHeader->pNext = pBackend->apPool[nBucket];` |
|  10949137 |  440 | `		pBackend->apPool[nBucket] = pHeader;` |
|         - |  441 | `	}` |
|  10949137 |  442 | `	return SXRET_OK;` |
|   5474572 |  443 |  |
|  10949134 |  444 | `PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|         5 |  445 |  |
|         - |  446 | `	sxi32 rc;` |
|         - |  447 | `#if defined(UNTRUST)` |
|         - |  448 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pChunk == 0 ){` |
|         - |  449 | `		return SXERR_CORRUPT;` |
|         - |  450 | `	}` |
|         - |  451 | `#endif` |
|  10949139 |  452 | `	if( pBackend->pMutexMethods ){` |
|      3207 |  453 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1601 |  454 | `	}` |
|  10949139 |  455 | `	rc = MemBackendPoolFree(&(*pBackend),pChunk);` |
|  10949139 |  456 | `	if( pBackend->pMutexMethods ){` |
|      3207 |  457 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1601 |  458 | `	}` |
|  10949139 |  459 | `	return rc;` |
|         5 |  460 |  |
|         - |  461 | `#if 0` |
|         - |  462 | `static void * MemBackendPoolRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         - |  463 |  |
|         - |  464 | `	sxu32 nBucket,nBucketSize;` |
|         - |  465 | `	SyMemHeader *pHeader;` |
|         - |  466 | `	void * pNew;` |
|         - |  467 |  |
|         - |  468 | `	if( pOld == 0 ){` |
|         - |  469 | `		/* Allocate a new pool */` |
|         - |  470 | `		pNew = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|         - |  471 | `		return pNew;` |
|         - |  472 | `	}` |
|         - |  473 | `	/* Get the corresponding bucket */` |
|         - |  474 | `	pHeader = (SyMemHeader *)(((char *)pOld) - sizeof(SyMemHeader));` |
|         - |  475 | `	/* Sanity check to avoid misuse */` |
|         - |  476 | `	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){` |
|         - |  477 | `		return 0;` |
|         - |  478 | `	}` |
|         - |  479 | `	nBucket = pHeader->nBucket & 0xFFFF;` |
|         - |  480 | `	if( nBucket == SXU16_HIGH ){` |
|         - |  481 | `		/* Big block */` |
|         - |  482 | `		return MemBackendRealloc(&(*pBackend),pHeader,nByte);` |
|         - |  483 | `	}` |
|         - |  484 | `	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);` |
|         - |  485 | `	if( nBucketSize >= nByte + sizeof(SyMemHeader) ){` |
|         - |  486 | `		/* The old bucket can honor the requested size */` |
|         - |  487 | `		return pOld;` |
|         - |  488 | `	}` |
|         - |  489 | `	/* Allocate a new pool */` |
|         - |  490 | `	pNew = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|         - |  491 | `	if( pNew == 0 ){` |
|         - |  492 | `		return 0;` |
|         - |  493 | `	}` |
|         - |  494 | `	/* Copy the old data into the new block */` |
|         - |  495 | `	SyMemcpy(pOld,pNew,nBucketSize);` |
|         - |  496 | `	/* Free the stale block */` |
|         - |  497 | `	MemBackendPoolFree(&(*pBackend),pOld);` |
|         - |  498 | `	return pNew;` |
|         - |  499 |  |
|         - |  500 | `PH7_PRIVATE void * SyMemBackendPoolRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         - |  501 |  |
|         - |  502 | `	void *pChunk;` |
|         - |  503 | `#if defined(UNTRUST)` |
|         - |  504 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  505 | `		return 0;` |
|         - |  506 | `	}` |
|         - |  507 | `#endif` |
|         - |  508 | `	if( pBackend->pMutexMethods ){` |
|         - |  509 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|         - |  510 | `	}` |
|         - |  511 | `	pChunk = MemBackendPoolRealloc(&(*pBackend),pOld,nByte);` |
|         - |  512 | `	if( pBackend->pMutexMethods ){` |
|         - |  513 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|         - |  514 | `	}` |
|         - |  515 | `	return pChunk;` |
|         - |  516 |  |
|         - |  517 | `#endif` |
|      3548 |  518 | `PH7_PRIVATE sxi32 SyMemBackendInit(SyMemBackend *pBackend,ProcMemError xMemErr,void * pUserData)` |
|         5 |  519 |  |
|         - |  520 | `#if defined(UNTRUST)` |
|         - |  521 | `	if( pBackend == 0 ){` |
|         - |  522 | `		return SXERR_EMPTY;` |
|         - |  523 | `	}` |
|         - |  524 | `#endif` |
|         - |  525 | `	/* Zero the allocator first */` |
|      3553 |  526 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|      3553 |  527 | `	pBackend->xMemError = xMemErr;` |
|      3553 |  528 | `	pBackend->pUserData = pUserData;` |
|         - |  529 | `	/* Switch to the OS memory allocator */` |
|      3553 |  530 | `	pBackend->pMethods = &sOSAllocMethods;` |
|      3553 |  531 | `	if( pBackend->pMethods->xInit ){` |
|         - |  532 | `		/* Initialize the backend  */` |
|       ! 0 |  533 | `		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){` |
|       ! 0 |  534 | `			return SXERR_ABORT;` |
|         - |  535 | `		}` |
|       ! 0 |  536 | `	}` |
|         - |  537 | `#if defined(UNTRUST)` |
|         - |  538 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|         - |  539 | `#endif` |
|      3553 |  540 | `	return SXRET_OK;` |
|      1779 |  541 |  |
|       ! 0 |  542 | `PH7_PRIVATE sxi32 SyMemBackendInitFromOthers(SyMemBackend *pBackend,const SyMemMethods *pMethods,ProcMemError xMemErr,void * pUserData)` |
|       ! 0 |  543 |  |
|         - |  544 | `#if defined(UNTRUST)` |
|         - |  545 | `	if( pBackend == 0 \|\| pMethods == 0){` |
|         - |  546 | `		return SXERR_EMPTY;` |
|         - |  547 | `	}` |
|         - |  548 | `#endif` |
|       ! 0 |  549 | `	if( pMethods->xAlloc == 0 \|\| pMethods->xRealloc == 0 \|\| pMethods->xFree == 0 \|\| pMethods->xChunkSize == 0 ){` |
|         - |  550 | `		/* mandatory methods are missing */` |
|       ! 0 |  551 | `		return SXERR_INVALID;` |
|         - |  552 | `	}` |
|         - |  553 | `	/* Zero the allocator first */` |
|       ! 0 |  554 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|       ! 0 |  555 | `	pBackend->xMemError = xMemErr;` |
|       ! 0 |  556 | `	pBackend->pUserData = pUserData;` |
|         - |  557 | `	/* Switch to the host application memory allocator */` |
|       ! 0 |  558 | `	pBackend->pMethods = pMethods;` |
|       ! 0 |  559 | `	if( pBackend->pMethods->xInit ){` |
|         - |  560 | `		/* Initialize the backend  */` |
|       ! 0 |  561 | `		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){` |
|       ! 0 |  562 | `			return SXERR_ABORT;` |
|         - |  563 | `		}` |
|       ! 0 |  564 | `	}` |
|         - |  565 | `#if defined(UNTRUST)` |
|         - |  566 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|         - |  567 | `#endif` |
|       ! 0 |  568 | `	return SXRET_OK;` |
|       ! 0 |  569 |  |
|      7092 |  570 | `PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend *pBackend,SyMemBackend *pParent)` |
|         5 |  571 |  |
|         - |  572 | `	sxu8 bInheritMutex;` |
|         - |  573 | `#if defined(UNTRUST)` |
|         - |  574 | `	if( pBackend == 0 \|\| SXMEM_BACKEND_CORRUPT(pParent) ){` |
|         - |  575 | `		return SXERR_CORRUPT;` |
|         - |  576 | `	}` |
|         - |  577 | `#endif` |
|         - |  578 | `	/* Zero the allocator first */` |
|      7097 |  579 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|      7097 |  580 | `	pBackend->pMethods  = pParent->pMethods;` |
|      7097 |  581 | `	pBackend->xMemError = pParent->xMemError;` |
|      7097 |  582 | `	pBackend->pUserData = pParent->pUserData;` |
|      7097 |  583 | `	pBackend->nMaxRequest = pParent->nMaxRequest;` |
|      7097 |  584 | `	bInheritMutex = pParent->pMutexMethods ? TRUE : FALSE;` |
|      7097 |  585 | `	if( bInheritMutex ){` |
|      3553 |  586 | `		pBackend->pMutexMethods = pParent->pMutexMethods;` |
|         - |  587 | `		/* Create a private mutex */` |
|      3553 |  588 | `		pBackend->pMutex = pBackend->pMutexMethods->xNew(SXMUTEX_TYPE_FAST);` |
|      3553 |  589 | `		if( pBackend->pMutex ==  0){` |
|       ! 0 |  590 | `			return SXERR_OS;` |
|         - |  591 | `		}` |
|      1774 |  592 | `	}` |
|         - |  593 | `#if defined(UNTRUST)` |
|         - |  594 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|         - |  595 | `#endif` |
|      7097 |  596 | `	return SXRET_OK;` |
|      3551 |  597 |  |
|      7438 |  598 | `static sxi32 MemBackendRelease(SyMemBackend *pBackend)` |
|         5 |  599 |  |
|         - |  600 | `	SyMemBlock *pBlock,*pNext;` |
|         - |  601 |  |
|      7443 |  602 | `	pBlock = pBackend->pBlocks;` |
|    639136 |  603 | `	for(;;){` |
|   1278277 |  604 | `		if( pBackend->nBlock == 0 ){` |
|       585 |  605 | `			break;` |
|         - |  606 | `		}` |
|   1277697 |  607 | `		pNext  = pBlock->pNext;` |
|   1277697 |  608 | `		pBackend->pMethods->xFree(pBlock);` |
|   1277697 |  609 | `		pBlock = pNext;` |
|   1277697 |  610 | `		pBackend->nBlock--;` |
|         - |  611 | `		/* LOOP ONE */` |
|   1277697 |  612 | `		if( pBackend->nBlock == 0 ){` |
|      4441 |  613 | `			break;` |
|         - |  614 | `		}` |
|   1273261 |  615 | `		pNext  = pBlock->pNext;` |
|   1273261 |  616 | `		pBackend->pMethods->xFree(pBlock);` |
|   1273261 |  617 | `		pBlock = pNext;` |
|   1273261 |  618 | `		pBackend->nBlock--;` |
|         - |  619 | `		/* LOOP TWO */` |
|   1273261 |  620 | `		if( pBackend->nBlock == 0 ){` |
|       804 |  621 | `			break;` |
|         - |  622 | `		}` |
|   1272461 |  623 | `		pNext  = pBlock->pNext;` |
|   1272461 |  624 | `		pBackend->pMethods->xFree(pBlock);` |
|   1272461 |  625 | `		pBlock = pNext;` |
|   1272461 |  626 | `		pBackend->nBlock--;` |
|         - |  627 | `		/* LOOP THREE */` |
|   1272461 |  628 | `		if( pBackend->nBlock == 0 ){` |
|      1626 |  629 | `			break;` |
|         - |  630 | `		}` |
|   1270839 |  631 | `		pNext  = pBlock->pNext;` |
|   1270839 |  632 | `		pBackend->pMethods->xFree(pBlock);` |
|   1270839 |  633 | `		pBlock = pNext;` |
|   1270839 |  634 | `		pBackend->nBlock--;` |
|         - |  635 | `		/* LOOP FOUR */` |
|         5 |  636 | `	}` |
|      7443 |  637 | `	if( pBackend->pMethods->xRelease ){` |
|       ! 0 |  638 | `		pBackend->pMethods->xRelease(pBackend->pMethods->pUserData);` |
|       ! 0 |  639 | `	}` |
|      7443 |  640 | `	pBackend->pMethods = 0;` |
|      7443 |  641 | `	pBackend->pBlocks  = 0;` |
|         - |  642 | `#if defined(UNTRUST)` |
|         - |  643 | `	pBackend->nMagic = 0x2626;` |
|         - |  644 | `#endif` |
|      7443 |  645 | `	return SXRET_OK;` |
|         5 |  646 |  |
|      7438 |  647 | `PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend *pBackend)` |
|         5 |  648 |  |
|         - |  649 | `#if defined(UNTRUST)` |
|         - |  650 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  651 | `		return SXERR_INVALID;` |
|         - |  652 | `	}` |
|         - |  653 | `#endif` |
|      7443 |  654 | `	if( pBackend->pMutexMethods ){` |
|       350 |  655 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       173 |  656 | `	}` |
|      7443 |  657 | `	(void)MemBackendRelease(&(*pBackend));` |
|      7443 |  658 | `	if( pBackend->pMutexMethods ){` |
|       350 |  659 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       350 |  660 | `		SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|       173 |  661 | `	}` |
|      7443 |  662 | `	return SXRET_OK;` |
|         5 |  663 |  |
|    685210 |  664 | `PH7_PRIVATE void * SyMemBackendDup(SyMemBackend *pBackend,const void *pSrc,sxu32 nSize)` |
|         5 |  665 |  |
|         - |  666 | `	void *pNew;` |
|         - |  667 | `#if defined(UNTRUST)` |
|         - |  668 | `	if( pSrc == 0 \|\| nSize <= 0 ){` |
|         - |  669 | `		return 0;` |
|         - |  670 | `	}` |
|         - |  671 | `#endif` |
|    685215 |  672 | `	pNew = SyMemBackendAlloc(&(*pBackend),nSize);` |
|    685215 |  673 | `	if( pNew ){` |
|    685215 |  674 | `		SyMemcpy(pSrc,pNew,nSize);` |
|    342605 |  675 | `	}` |
|    685215 |  676 | `	return pNew;` |
|         5 |  677 |  |
|   3091010 |  678 | `PH7_PRIVATE char * SyMemBackendStrDup(SyMemBackend *pBackend,const char *zSrc,sxu32 nSize)` |
|         5 |  679 |  |
|         - |  680 | `	char *zDest;` |
|   3091015 |  681 | `	zDest = (char *)SyMemBackendAlloc(&(*pBackend),nSize + 1);` |
|   3091015 |  682 | `	if( zDest ){` |
|   3091015 |  683 | `		Systrcpy(zDest,nSize+1,zSrc,nSize);` |
|   1545505 |  684 | `	}` |
|   3091015 |  685 | `	return zDest;` |
|         5 |  686 |  |
|    253740 |  687 | `PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize)` |
|         5 |  688 |  |
|         - |  689 | `#if defined(UNTRUST)` |
|         - |  690 | `	if( pBlob == 0 \|\| pBuffer == 0 \|\| nSize < 1 ){` |
|         - |  691 | `		return SXERR_EMPTY;` |
|         - |  692 | `	}` |
|         - |  693 | `#endif` |
|    253745 |  694 | `	pBlob->pBlob = pBuffer;` |
|    253745 |  695 | `	pBlob->mByte = nSize;` |
|    253745 |  696 | `	pBlob->nByte = 0;` |
|    253745 |  697 | `	pBlob->pAllocator = 0;` |
|    253745 |  698 | `	pBlob->nFlags = SXBLOB_LOCKED\|SXBLOB_STATIC;` |
|    253745 |  699 | `	return SXRET_OK;` |
|         5 |  700 |  |
|   9416249 |  701 | `PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator)` |
|         5 |  702 |  |
|         - |  703 | `#if defined(UNTRUST)` |
|         - |  704 | `	if( pBlob == 0  ){` |
|         - |  705 | `		return SXERR_EMPTY;` |
|         - |  706 | `	}` |
|         - |  707 | `#endif` |
|   9416254 |  708 | `	pBlob->pBlob = 0;` |
|   9416254 |  709 | `	pBlob->mByte = pBlob->nByte	= 0;` |
|   9416254 |  710 | `	pBlob->pAllocator = &(*pAllocator);` |
|   9416254 |  711 | `	pBlob->nFlags = 0;` |
|   9416254 |  712 | `	return SXRET_OK;` |
|         5 |  713 |  |
|   3338646 |  714 | `PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte)` |
|         5 |  715 |  |
|         - |  716 | `#if defined(UNTRUST)` |
|         - |  717 | `	if( pBlob == 0  ){` |
|         - |  718 | `		return SXERR_EMPTY;` |
|         - |  719 | `	}` |
|         - |  720 | `#endif` |
|   3338651 |  721 | `	pBlob->pBlob = (void *)pData;` |
|   3338651 |  722 | `	pBlob->nByte = nByte;` |
|   3338651 |  723 | `	pBlob->mByte = 0;` |
|   3338651 |  724 | `	pBlob->nFlags \|= SXBLOB_RDONLY;` |
|   3338651 |  725 | `	return SXRET_OK;` |
|         5 |  726 |  |
|         - |  727 | `#ifndef SXBLOB_MIN_GROWTH` |
|         - |  728 | `#define SXBLOB_MIN_GROWTH 16` |
|         - |  729 | `#endif` |
|   9415660 |  730 | `static sxi32 BlobPrepareGrow(SyBlob *pBlob,sxu32 *pByte)` |
|         5 |  731 |  |
|         - |  732 | `	sxu32 nByte;` |
|         - |  733 | `	void *pNew;` |
|   9415665 |  734 | `	nByte = *pByte;` |
|   9415665 |  735 | `	if( pBlob->nFlags & (SXBLOB_LOCKED\|SXBLOB_STATIC) ){` |
|   2025819 |  736 | `		if ( SyBlobFreeSpace(pBlob) < nByte ){` |
|       ! 0 |  737 | `			*pByte = SyBlobFreeSpace(pBlob);` |
|       ! 0 |  738 | `			if( (*pByte) == 0 ){` |
|       ! 0 |  739 | `				return SXERR_SHORT;` |
|         - |  740 | `			}` |
|       ! 0 |  741 | `		}` |
|   2025819 |  742 | `		return SXRET_OK;` |
|         - |  743 | `	}` |
|   7389851 |  744 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|         - |  745 | `		/* Make a copy of the read-only item */` |
|    685215 |  746 | `		if( pBlob->nByte > 0 ){` |
|    685215 |  747 | `			pNew = SyMemBackendDup(pBlob->pAllocator,pBlob->pBlob,pBlob->nByte);` |
|    685215 |  748 | `			if( pNew == 0 ){` |
|       ! 0 |  749 | `				return SXERR_MEM;` |
|         - |  750 | `			}` |
|    685215 |  751 | `			pBlob->pBlob = pNew;` |
|    685215 |  752 | `			pBlob->mByte = pBlob->nByte;` |
|    342610 |  753 | `		}else{` |
|       ! 0 |  754 | `			pBlob->pBlob = 0;` |
|       ! 0 |  755 | `			pBlob->mByte = 0;` |
|         - |  756 | `		}` |
|         - |  757 | `		/* Remove the read-only flag */` |
|    685215 |  758 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|    342605 |  759 | `	}` |
|   7389851 |  760 | `	if( SyBlobFreeSpace(pBlob) >= nByte ){` |
|   1491666 |  761 | `		return SXRET_OK;` |
|         - |  762 | `	}` |
|   5898190 |  763 | `	if( pBlob->mByte > 0 ){` |
|    802907 |  764 | `		nByte = nByte + pBlob->mByte * 2 + SXBLOB_MIN_GROWTH;` |
|   5496736 |  765 | `	}else if ( nByte < SXBLOB_MIN_GROWTH ){` |
|   3995448 |  766 | `		nByte = SXBLOB_MIN_GROWTH;` |
|   1997628 |  767 | `	}` |
|   5898190 |  768 | `	pNew = SyMemBackendRealloc(pBlob->pAllocator,pBlob->pBlob,nByte);` |
|   5898190 |  769 | `	if( pNew == 0 ){` |
|       ! 0 |  770 | `		return SXERR_MEM;` |
|         - |  771 | `	}` |
|   5898190 |  772 | `	pBlob->pBlob = pNew;` |
|   5898190 |  773 | `	pBlob->mByte = nByte;` |
|   5898190 |  774 | `	return SXRET_OK;` |
|   4707920 |  775 |  |
|   9478206 |  776 | `PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize)` |
|         5 |  777 |  |
|         - |  778 | `	sxu8 *zBlob;` |
|         - |  779 | `	sxi32 rc;` |
|   9478211 |  780 | `	if( nSize < 1 ){` |
|     62551 |  781 | `		return SXRET_OK;` |
|         - |  782 | `	}` |
|   9415665 |  783 | `	rc = BlobPrepareGrow(&(*pBlob),&nSize);` |
|   9415665 |  784 | `	if( SXRET_OK != rc ){` |
|       ! 0 |  785 | `		return rc;` |
|         - |  786 | `	}` |
|   9415665 |  787 | `	if( pData ){` |
|   9415633 |  788 | `		zBlob = (sxu8 *)pBlob->pBlob ;` |
|   9415633 |  789 | `		zBlob = &zBlob[pBlob->nByte];` |
|   9415633 |  790 | `		pBlob->nByte += nSize;` |
|  42224259 |  791 | `		SX_MACRO_FAST_MEMCPY(pData,zBlob,nSize);` |
|   4707899 |  792 | `	}` |
|   9415665 |  793 | `	return SXRET_OK;` |
|   4739193 |  794 |  |
|    695689 |  795 | `PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob)` |
|         5 |  796 |  |
|         - |  797 | `	sxi32 rc;` |
|         - |  798 | `	sxu32 n;` |
|    695694 |  799 | `	n = pBlob->nByte;` |
|    695694 |  800 | `	rc = SyBlobAppend(&(*pBlob),(const void *)"\0",sizeof(char));` |
|    695694 |  801 | `	if (rc == SXRET_OK ){` |
|    695694 |  802 | `		pBlob->nByte = n;` |
|    347887 |  803 | `	}` |
|    695694 |  804 | `	return rc;` |
|         5 |  805 |  |
|   3851012 |  806 | `PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest)` |
|         5 |  807 |  |
|   3851017 |  808 | `	sxi32 rc = SXRET_OK;` |
|         - |  809 | `#ifdef UNTRUST` |
|         - |  810 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|         - |  811 | `		return SXERR_EMPTY;` |
|         - |  812 | `	}` |
|         - |  813 | `#endif` |
|   3851017 |  814 | `	if( pSrc->nByte > 0 ){` |
|   3823721 |  815 | `		rc = SyBlobAppend(&(*pDest),pSrc->pBlob,pSrc->nByte);` |
|   1911858 |  816 | `	}` |
|   3851017 |  817 | `	return rc;` |
|         5 |  818 |  |
|         8 |  819 | `PH7_PRIVATE sxi32 SyBlobCmp(SyBlob *pLeft,SyBlob *pRight)` |
|         1 |  820 |  |
|         - |  821 | `	sxi32 rc;` |
|         - |  822 | `#ifdef UNTRUST` |
|         - |  823 | `	if( pLeft == 0 \|\| pRight == 0 ){` |
|         - |  824 | `		return pLeft ? 1 : -1;` |
|         - |  825 | `	}` |
|         - |  826 | `#endif` |
|         9 |  827 | `	if( pLeft->nByte != pRight->nByte ){` |
|         - |  828 | `		/* Length differ */` |
|       ! 0 |  829 | `		return pLeft->nByte - pRight->nByte;` |
|         - |  830 | `	}` |
|         9 |  831 | `	if( pLeft->nByte == 0 ){` |
|       ! 0 |  832 | `		return 0;` |
|         - |  833 | `	}` |
|         - |  834 | `	/* Perform a standard memcmp() operation */` |
|         9 |  835 | `	rc = SyMemcmp(pLeft->pBlob,pRight->pBlob,pLeft->nByte);` |
|         9 |  836 | `	return rc;` |
|         5 |  837 |  |
|   4534704 |  838 | `PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob)` |
|         5 |  839 |  |
|   4534709 |  840 | `	pBlob->nByte = 0;` |
|   4534709 |  841 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|      4841 |  842 | `		pBlob->pBlob = 0;` |
|      4841 |  843 | `		pBlob->mByte = 0;` |
|      4841 |  844 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|      2418 |  845 | `	}` |
|   4534709 |  846 | `	return SXRET_OK;` |
|         5 |  847 |  |
|  12329493 |  848 | `PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob)` |
|         5 |  849 |  |
|  12329498 |  850 | `	if( (pBlob->nFlags & (SXBLOB_STATIC\|SXBLOB_RDONLY)) == 0 && pBlob->mByte > 0 ){` |
|   5288616 |  851 | `		SyMemBackendFree(pBlob->pAllocator,pBlob->pBlob);` |
|   2644348 |  852 | `	}` |
|  12329498 |  853 | `	pBlob->pBlob = 0;` |
|  12329498 |  854 | `	pBlob->nByte = pBlob->mByte = 0;` |
|  12329498 |  855 | `	pBlob->nFlags = 0;` |
|  12329498 |  856 | `	return SXRET_OK;` |
|         5 |  857 |  |
|         - |  858 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    163662 |  859 | `PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft)` |
|         5 |  860 |  |
|    163667 |  861 | `	const char *zIn = (const char *)pBlob;` |
|         - |  862 | `	const char *zEnd;` |
|         - |  863 | `	sxi32 rc;` |
|    163667 |  864 | `	if( pLen > nLen ){` |
|      6131 |  865 | `		return SXERR_NOTFOUND;` |
|         - |  866 | `	}` |
|    157541 |  867 | `	zEnd = &zIn[nLen-pLen];` |
|   1343673 |  868 | `	for(;;){` |
|   2687266 |  869 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   2648087 |  870 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   2592197 |  871 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   2557538 |  872 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|         5 |  873 | `	}` |
|     23949 |  874 | `	return SXERR_NOTFOUND;` |
|     81836 |  875 |  |
|         - |  876 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - |  877 |  |
