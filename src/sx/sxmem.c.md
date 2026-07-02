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
|  16326317 |   18 | `static void * SyOSHeapAlloc(sxu32 nByte)` |
|         5 |   19 | `{` |
|         - |   20 | `	void *pNew;` |
|         - |   21 | `#if defined(__WINNT__)` |
|         5 |   22 | `	pNew = HeapAlloc(GetProcessHeap(),0,nByte);` |
|         - |   23 | `#else` |
|  16326317 |   24 | `	pNew = malloc((size_t)nByte);` |
|         - |   25 | `#endif` |
|  16326322 |   26 | `	return pNew;` |
|         5 |   27 | `}` |
|    990998 |   28 | `static void * SyOSHeapRealloc(void *pOld,sxu32 nByte)` |
|         5 |   29 | `{` |
|         - |   30 | `	void *pNew;` |
|         - |   31 | `#if defined(__WINNT__)` |
|         5 |   32 | `	pNew = HeapReAlloc(GetProcessHeap(),0,pOld,nByte);` |
|         - |   33 | `#else` |
|    990998 |   34 | `	pNew = realloc(pOld,(size_t)nByte);` |
|         - |   35 | `#endif` |
|    991003 |   36 | `	return pNew;` |
|         5 |   37 | `}` |
|  16323003 |   38 | `static void SyOSHeapFree(void *pPtr)` |
|         5 |   39 | `{` |
|         - |   40 | `#if defined(__WINNT__)` |
|         5 |   41 | `	HeapFree(GetProcessHeap(),0,pPtr);` |
|         - |   42 | `#else` |
|  16323003 |   43 | `	free(pPtr);` |
|         - |   44 | `#endif` |
|  16323008 |   45 | `}` |
|         - |   46 |  |
|         - |   47 |  |
|  30100963 |   48 | `PH7_PRIVATE void SyZero(void *pSrc,sxu32 nSize)` |
|         5 |   49 | `{` |
|  30100968 |   50 | `	register unsigned char *zSrc = (unsigned char *)pSrc;` |
|         - |   51 | `	unsigned char *zEnd;` |
|         - |   52 | `#if defined(UNTRUST)` |
|         - |   53 | `	if( zSrc == 0 \|\| nSize <= 0 ){` |
|         - |   54 | `		return ;` |
|         - |   55 | `	}` |
|         - |   56 | `#endif` |
|  30100968 |   57 | `	zEnd = &zSrc[nSize];` |
| 405120836 |   58 | `	for(;;){` |
| 810236680 |   59 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 780135961 |   60 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 780135871 |   61 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 780135749 |   62 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
|         5 |   63 | `	}` |
|  30100968 |   64 | `}` |
|  30945074 |   65 | `PH7_PRIVATE sxi32 SyMemcmp(const void *pB1,const void *pB2,sxu32 nSize)` |
|         5 |   66 | `{` |
|         - |   67 | `	sxi32 rc;` |
|  30945079 |   68 | `	if( nSize <= 0 ){` |
|        91 |   69 | `		return 0;` |
|         - |   70 | `	}` |
|  30944989 |   71 | `	if( pB1 == 0 \|\| pB2 == 0 ){` |
|       ! 0 |   72 | `		return pB1 != 0 ? 1 : (pB2 == 0 ? 0 : -1);` |
|         - |   73 | `	}` |
|  66315728 |   74 | `	SX_MACRO_FAST_CMP(pB1,pB2,nSize,rc);` |
|  30944989 |   75 | `	return rc;` |
|  15473338 |   76 | `}` |
|  11649677 |   77 | `PH7_PRIVATE sxu32 SyMemcpy(const void *pSrc,void *pDest,sxu32 nLen)` |
|         5 |   78 | `{` |
|         - |   79 | `#if defined(UNTRUST)` |
|         - |   80 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|         - |   81 | `		return 0;` |
|         - |   82 | `	}` |
|         - |   83 | `#endif` |
|  11649682 |   84 | `	if( pSrc == (const void *)pDest ){` |
|       ! 0 |   85 | `		return nLen;` |
|         - |   86 | `	}` |
|  95859940 |   87 | `	SX_MACRO_FAST_MEMCPY(pSrc,pDest,nLen);` |
|  11649682 |   88 | `	return nLen;` |
|   5825237 |   89 | `}` |
|  16326317 |   90 | `static void * MemOSAlloc(sxu32 nBytes)` |
|         5 |   91 | `{` |
|         - |   92 | `	sxu32 *pChunk;` |
|  16326322 |   93 | `	pChunk = (sxu32 *)SyOSHeapAlloc(nBytes + sizeof(sxu32));` |
|  16326322 |   94 | `	if( pChunk == 0 ){` |
|       ! 0 |   95 | `		return 0;` |
|         - |   96 | `	}` |
|  16326322 |   97 | `	pChunk[0] = nBytes;` |
|  16326322 |   98 | `	return (void *)&pChunk[1];` |
|   8163207 |   99 | `}` |
|    990998 |  100 | `static void * MemOSRealloc(void *pOld,sxu32 nBytes)` |
|         5 |  101 | `{` |
|         - |  102 | `	sxu32 *pOldChunk;` |
|         - |  103 | `	sxu32 *pChunk;` |
|    991003 |  104 | `	pOldChunk = (sxu32 *)(((char *)pOld)-sizeof(sxu32));` |
|    991003 |  105 | `	if( pOldChunk[0] >= nBytes ){` |
|       ! 0 |  106 | `		return pOld;` |
|         - |  107 | `	}` |
|    991003 |  108 | `	pChunk = (sxu32 *)SyOSHeapRealloc(pOldChunk,nBytes + sizeof(sxu32));` |
|    991003 |  109 | `	if( pChunk == 0 ){` |
|       ! 0 |  110 | `		return 0;` |
|         - |  111 | `	}` |
|    991003 |  112 | `	pChunk[0] = nBytes;` |
|    991003 |  113 | `	return (void *)&pChunk[1];` |
|    495500 |  114 | `}` |
|  16323003 |  115 | `static void MemOSFree(void *pBlock)` |
|         5 |  116 | `{` |
|         - |  117 | `	void *pChunk;` |
|  16323008 |  118 | `	pChunk = (void *)(((char *)pBlock)-sizeof(sxu32));` |
|  16323008 |  119 | `	SyOSHeapFree(pChunk);` |
|  16323008 |  120 | `}` |
|       ! 0 |  121 | `static sxu32 MemOSChunkSize(void *pBlock)` |
|       ! 0 |  122 | `{` |
|         - |  123 | `	sxu32 *pChunk;` |
|       ! 0 |  124 | `	pChunk = (sxu32 *)(((char *)pBlock)-sizeof(sxu32));` |
|       ! 0 |  125 | `	return pChunk[0];` |
|       ! 0 |  126 | `}` |
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
|  16326317 |  137 | `static void * MemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         5 |  138 | `{` |
|         - |  139 | `	SyMemBlock *pBlock;` |
|  16326322 |  140 | `	sxi32 nRetry = 0;` |
|         - |  141 |  |
|         - |  142 | `	/* Append an extra block so we can tracks allocated chunks and avoid memory` |
|         - |  143 | `	 * leaks.` |
|         - |  144 | `	 */` |
|  16326322 |  145 | `	nByte += sizeof(SyMemBlock);` |
|         - |  146 | `	/* Enforce the optional per-allocation cap (0 = unlimited). A capped failure` |
|         - |  147 | `	 * returns NULL just like a genuine OS failure, driving the normal SXERR_MEM` |
|         - |  148 | `	 * propagation; the retry callback is intentionally skipped (hard limit). */` |
|  16326322 |  149 | `	if( pBackend->nMaxRequest && nByte > pBackend->nMaxRequest ){` |
|       ! 0 |  150 | `		return 0;` |
|         - |  151 | `	}` |
|   8163202 |  152 | `	for(;;){` |
|   8163207 |  153 | `		pBlock = (SyMemBlock *)pBackend->pMethods->xAlloc(nByte);` |
|  16326317 |  154 | `		if( pBlock != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY` |
|         5 |  155 | `			\|\| SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|   8163207 |  156 | `				break;` |
|         - |  157 | `		}` |
|       ! 0 |  158 | `		nRetry++;` |
|       ! 0 |  159 | `	}` |
|  16326322 |  160 | `	if( pBlock  == 0 ){` |
|       ! 0 |  161 | `		return 0;` |
|         - |  162 | `	}` |
|  16326322 |  163 | `	pBlock->pNext = pBlock->pPrev = 0;` |
|         - |  164 | `	/* Link to the list of already tracked blocks */` |
|  16326322 |  165 | `	MACRO_LD_PUSH(pBackend->pBlocks,pBlock);` |
|         - |  166 | `#if defined(UNTRUST)` |
|         - |  167 | `	pBlock->nGuard = SXMEM_BACKEND_MAGIC;` |
|         - |  168 | `#endif` |
|  16326322 |  169 | `	pBackend->nBlock++;` |
|  16326322 |  170 | `	return (void *)&pBlock[1];` |
|   8163207 |  171 | `}` |
|   6492454 |  172 | `PH7_PRIVATE void * SyMemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         5 |  173 | `{` |
|         - |  174 | `	void *pChunk;` |
|         - |  175 | `#if defined(UNTRUST)` |
|         - |  176 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  177 | `		return 0;` |
|         - |  178 | `	}` |
|         - |  179 | `#endif` |
|   6492459 |  180 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  181 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  182 | `	}` |
|   6492459 |  183 | `	pChunk = MemBackendAlloc(&(*pBackend),nByte);` |
|   6492459 |  184 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  185 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  186 | `	}` |
|   6492459 |  187 | `	return pChunk;` |
|         5 |  188 | `}` |
|  10750805 |  189 | `static void * MemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         5 |  190 | `{` |
|         - |  191 | `	SyMemBlock *pBlock,*pNew,*pPrev,*pNext;` |
|  10750810 |  192 | `	sxu32 nRetry = 0;` |
|         - |  193 |  |
|  10750810 |  194 | `	if( pOld == 0 ){` |
|   9759812 |  195 | `		return MemBackendAlloc(&(*pBackend),nByte);` |
|         - |  196 | `	}` |
|    991003 |  197 | `	pBlock = (SyMemBlock *)(((char *)pOld) - sizeof(SyMemBlock));` |
|         - |  198 | `#if defined(UNTRUST)` |
|         - |  199 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|         - |  200 | `		return 0;` |
|         - |  201 | `	}` |
|         - |  202 | `#endif` |
|    991003 |  203 | `	nByte += sizeof(SyMemBlock);` |
|         - |  204 | `	/* Enforce the optional per-allocation cap (0 = unlimited); see MemBackendAlloc. */` |
|    991003 |  205 | `	if( pBackend->nMaxRequest && nByte > pBackend->nMaxRequest ){` |
|       ! 0 |  206 | `		return 0;` |
|         - |  207 | `	}` |
|    991003 |  208 | `	pPrev = pBlock->pPrev;` |
|    991003 |  209 | `	pNext = pBlock->pNext;` |
|    495495 |  210 | `	for(;;){` |
|    495500 |  211 | `		pNew = (SyMemBlock *)pBackend->pMethods->xRealloc(pBlock,nByte);` |
|    991003 |  212 | `		if( pNew != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY \|\|` |
|       ! 0 |  213 | `			SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|    495500 |  214 | `				break;` |
|         - |  215 | `		}` |
|       ! 0 |  216 | `		nRetry++;` |
|       ! 0 |  217 | `	}` |
|    991003 |  218 | `	if( pNew == 0 ){` |
|       ! 0 |  219 | `		return 0;` |
|         - |  220 | `	}` |
|    991003 |  221 | `	if( pNew != pBlock ){` |
|    908669 |  222 | `		if( pPrev == 0 ){` |
|    686903 |  223 | `			pBackend->pBlocks = pNew;` |
|    374511 |  224 | `		}else{` |
|    221771 |  225 | `			pPrev->pNext = pNew;` |
|         - |  226 | `		}` |
|    908669 |  227 | `		if( pNext ){` |
|    908655 |  228 | `			pNext->pPrev = pNew;` |
|    493637 |  229 | `		}` |
|         - |  230 | `#if defined(UNTRUST)` |
|         - |  231 | `		pNew->nGuard = SXMEM_BACKEND_MAGIC;` |
|         - |  232 | `#endif` |
|    493644 |  233 | `	}` |
|    991003 |  234 | `	return (void *)&pNew[1];` |
|   5375447 |  235 | `}` |
|  10750805 |  236 | `PH7_PRIVATE void * SyMemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         5 |  237 | `{` |
|         - |  238 | `	void *pChunk;` |
|         - |  239 | `#if defined(UNTRUST)` |
|         - |  240 | `	if( SXMEM_BACKEND_CORRUPT(pBackend)  ){` |
|         - |  241 | `		return 0;` |
|         - |  242 | `	}` |
|         - |  243 | `#endif` |
|  10750810 |  244 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  245 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  246 | `	}` |
|  10750810 |  247 | `	pChunk = MemBackendRealloc(&(*pBackend),pOld,nByte);` |
|  10750810 |  248 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  249 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  250 | `	}` |
|  10750810 |  251 | `	return pChunk;` |
|         5 |  252 | `}` |
|  10851009 |  253 | `static sxi32 MemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|         5 |  254 | `{` |
|         - |  255 | `	SyMemBlock *pBlock;` |
|  10851014 |  256 | `	pBlock = (SyMemBlock *)(((char *)pChunk) - sizeof(SyMemBlock));` |
|         - |  257 | `#if defined(UNTRUST)` |
|         - |  258 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|         - |  259 | `		return SXERR_CORRUPT;` |
|         - |  260 | `	}` |
|         - |  261 | `#endif` |
|         - |  262 | `	/* Unlink from the list of active blocks */` |
|  10851014 |  263 | `	if( pBackend->nBlock > 0 ){` |
|         - |  264 | `		/* Release the block */` |
|         - |  265 | `#if defined(UNTRUST)` |
|         - |  266 | `		/* Mark as stale block */` |
|         - |  267 | `		pBlock->nGuard = 0x635B;` |
|         - |  268 | `#endif` |
|  10851014 |  269 | `		MACRO_LD_REMOVE(pBackend->pBlocks,pBlock);` |
|  10851014 |  270 | `		pBackend->nBlock--;` |
|  10851014 |  271 | `		pBackend->pMethods->xFree(pBlock);` |
|   5425548 |  272 | `	}` |
|  10851014 |  273 | `	return SXRET_OK;` |
|         5 |  274 | `}` |
|  10851009 |  275 | `PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|         5 |  276 | `{` |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|  10851014 |  283 | `	if( pChunk == 0 ){` |
|       ! 0 |  284 | `		return SXRET_OK;` |
|         - |  285 | `	}` |
|  10851014 |  286 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  287 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  288 | `	}` |
|  10851014 |  289 | `	rc = MemBackendFree(&(*pBackend),pChunk);` |
|  10851014 |  290 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  291 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  292 | `	}` |
|  10851014 |  293 | `	return rc;` |
|   5425553 |  294 | `}` |
|         - |  295 | `#if defined(PH7_ENABLE_THREADS)` |
|      3678 |  296 | `PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods)` |
|         5 |  297 | `{` |
|         - |  298 | `	SyMutex *pMutex;` |
|         - |  299 | `#if defined(UNTRUST)` |
|         - |  300 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pMethods == 0 \|\| pMethods->xNew == 0){` |
|         - |  301 | `		return SXERR_CORRUPT;` |
|         - |  302 | `	}` |
|         - |  303 | `#endif` |
|      3683 |  304 | `	pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|      3683 |  305 | `	if( pMutex == 0 ){` |
|       ! 0 |  306 | `		return SXERR_OS;` |
|         - |  307 | `	}` |
|         - |  308 | `	/* Attach the mutex to the memory backend */` |
|      3683 |  309 | `	pBackend->pMutex = pMutex;` |
|      3683 |  310 | `	pBackend->pMutexMethods = pMethods;` |
|      3683 |  311 | `	return SXRET_OK;` |
|      1844 |  312 | `}` |
|      3678 |  313 | `PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend)` |
|         5 |  314 | `{` |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  317 | `		return SXERR_CORRUPT;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|      3683 |  320 | `	if( pBackend->pMutex == 0 ){` |
|         - |  321 | `		/* There is no mutex subsystem at all */` |
|       ! 0 |  322 | `		return SXRET_OK;` |
|         - |  323 | `	}` |
|      3683 |  324 | `	SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|      3683 |  325 | `	pBackend->pMutexMethods = 0;` |
|      3683 |  326 | `	pBackend->pMutex = 0;` |
|      3683 |  327 | `	return SXRET_OK;` |
|      1844 |  328 | `}` |
|         - |  329 | `#endif` |
|         - |  330 | `/*` |
|         - |  331 | ` * Memory pool allocator` |
|         - |  332 | ` */` |
|         - |  333 | `#define SXMEM_POOL_MAGIC		0xDEAD` |
|         - |  334 | `#define SXMEM_POOL_MAXALLOC		(1<<(SXMEM_POOL_NBUCKETS+SXMEM_POOL_INCR))` |
|         - |  335 | `#define SXMEM_POOL_MINALLOC		(1<<(SXMEM_POOL_INCR))` |
|     74056 |  336 | `static sxi32 MemPoolBucketAlloc(SyMemBackend *pBackend,sxu32 nBucket)` |
|         5 |  337 | `{` |
|         - |  338 | `	char *zBucket,*zBucketEnd;` |
|         - |  339 | `	SyMemHeader *pHeader;` |
|         - |  340 | `	sxu32 nBucketSize;` |
|         - |  341 |  |
|         - |  342 | `	/* Allocate one big block first */` |
|     74061 |  343 | `	zBucket = (char *)MemBackendAlloc(&(*pBackend),SXMEM_POOL_MAXALLOC);` |
|     74061 |  344 | `	if( zBucket == 0 ){` |
|       ! 0 |  345 | `		return SXERR_MEM;` |
|         - |  346 | `	}` |
|     74061 |  347 | `	zBucketEnd = &zBucket[SXMEM_POOL_MAXALLOC];` |
|         - |  348 | `	/* Divide the big block into mini bucket pool */` |
|     74061 |  349 | `	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);` |
|     74061 |  350 | `	pBackend->apPool[nBucket] = pHeader = (SyMemHeader *)zBucket;` |
|   8144744 |  351 | `	for(;;){` |
|  16289493 |  352 | `		if( &zBucket[nBucketSize] >= zBucketEnd ){` |
|     74061 |  353 | `			break;` |
|         - |  354 | `		}` |
|  16215437 |  355 | `		pHeader->pNext = (SyMemHeader *)&zBucket[nBucketSize];` |
|         - |  356 | `		/* Advance the cursor to the next available chunk */` |
|  16215437 |  357 | `		pHeader = pHeader->pNext;` |
|  16215437 |  358 | `		zBucket += nBucketSize;` |
|         5 |  359 | `	}` |
|     74061 |  360 | `	pHeader->pNext = 0;` |
|         - |  361 |  |
|     74061 |  362 | `	return SXRET_OK;` |
|     37033 |  363 | `}` |
|  20419788 |  364 | `static void * MemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         5 |  365 | `{` |
|         - |  366 | `	SyMemHeader *pBucket,*pNext;` |
|         - |  367 | `	sxu32 nBucketSize;` |
|         - |  368 | `	sxu32 nBucket;` |
|         - |  369 |  |
|  20419793 |  370 | `	if( nByte + sizeof(SyMemHeader) >= SXMEM_POOL_MAXALLOC ){` |
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
|  20419793 |  381 | `	nBucket = 0;` |
|  20419793 |  382 | `	nBucketSize = SXMEM_POOL_MINALLOC;` |
| 102284613 |  383 | `	while( nByte + sizeof(SyMemHeader) > nBucketSize  ){` |
|  81864825 |  384 | `		nBucketSize <<= 1;` |
|  81864825 |  385 | `		nBucket++;` |
|         5 |  386 | `	}` |
|  20419793 |  387 | `	pBucket = pBackend->apPool[nBucket];` |
|  20419793 |  388 | `	if( pBucket == 0 ){` |
|         - |  389 | `		sxi32 rc;` |
|     74061 |  390 | `		rc = MemPoolBucketAlloc(&(*pBackend),nBucket);` |
|     74061 |  391 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  392 | `			return 0;` |
|         - |  393 | `		}` |
|     74061 |  394 | `		pBucket = pBackend->apPool[nBucket];` |
|     37028 |  395 | `	}` |
|         - |  396 | `	/* Remove from the free list */` |
|  20419793 |  397 | `	pNext = pBucket->pNext;` |
|  20419793 |  398 | `	pBackend->apPool[nBucket] = pNext;` |
|         - |  399 | `	/* Record bucket&magic number */` |
|  20419793 |  400 | `	pBucket->nBucket = (SXMEM_POOL_MAGIC << 16) \| nBucket;` |
|  20419793 |  401 | `	return (void *)&pBucket[1];` |
|  10209899 |  402 | `}` |
|  20419788 |  403 | `PH7_PRIVATE void * SyMemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         5 |  404 | `{` |
|         - |  405 | `	void *pChunk;` |
|         - |  406 | `#if defined(UNTRUST)` |
|         - |  407 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  408 | `		return 0;` |
|         - |  409 | `	}` |
|         - |  410 | `#endif` |
|  20419793 |  411 | `	if( pBackend->pMutexMethods ){` |
|      3683 |  412 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1839 |  413 | `	}` |
|  20419793 |  414 | `	pChunk = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|  20419793 |  415 | `	if( pBackend->pMutexMethods ){` |
|      3683 |  416 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1839 |  417 | `	}` |
|  20419793 |  418 | `	return pChunk;` |
|         5 |  419 | `}` |
|  11305500 |  420 | `static sxi32 MemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|         5 |  421 | `{` |
|         - |  422 | `	SyMemHeader *pHeader;` |
|         - |  423 | `	sxu32 nBucket;` |
|         - |  424 | `	/* Get the corresponding bucket */` |
|  11305505 |  425 | `	pHeader = (SyMemHeader *)(((char *)pChunk) - sizeof(SyMemHeader));` |
|         - |  426 | `	/* Sanity check to avoid misuse */` |
|  11305505 |  427 | `	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){` |
|         3 |  428 | `		return SXERR_CORRUPT;` |
|         - |  429 | `	}` |
|  11305503 |  430 | `	nBucket = pHeader->nBucket & 0xFFFF;` |
|  11305503 |  431 | `	if( nBucket == SXU16_HIGH ){` |
|         - |  432 | `		/* Free the big block */` |
|       ! 0 |  433 | `		MemBackendFree(&(*pBackend),pHeader);` |
|  11305503 |  434 | `	}else if( nBucket >= SXMEM_POOL_NBUCKETS + SXMEM_POOL_INCR ){` |
|         - |  435 | `		/* Corrupted or misused bucket index */` |
|       ! 0 |  436 | `		return SXERR_CORRUPT;` |
|       ! 0 |  437 | `	}else{` |
|         - |  438 | `		/* Return to the free list */` |
|  11305503 |  439 | `		pHeader->pNext = pBackend->apPool[nBucket];` |
|  11305503 |  440 | `		pBackend->apPool[nBucket] = pHeader;` |
|         - |  441 | `	}` |
|  11305503 |  442 | `	return SXRET_OK;` |
|   5652755 |  443 | `}` |
|  11305500 |  444 | `PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|         5 |  445 | `{` |
|         - |  446 | `	sxi32 rc;` |
|         - |  447 | `#if defined(UNTRUST)` |
|         - |  448 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pChunk == 0 ){` |
|         - |  449 | `		return SXERR_CORRUPT;` |
|         - |  450 | `	}` |
|         - |  451 | `#endif` |
|  11305505 |  452 | `	if( pBackend->pMutexMethods ){` |
|      3319 |  453 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1657 |  454 | `	}` |
|  11305505 |  455 | `	rc = MemBackendPoolFree(&(*pBackend),pChunk);` |
|  11305505 |  456 | `	if( pBackend->pMutexMethods ){` |
|      3319 |  457 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1657 |  458 | `	}` |
|  11305505 |  459 | `	return rc;` |
|         5 |  460 | `}` |
|         - |  461 | `#if 0` |
|         - |  462 | `static void * MemBackendPoolRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         - |  463 | `{` |
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
|         - |  499 | `}` |
|         - |  500 | `PH7_PRIVATE void * SyMemBackendPoolRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         - |  501 | `{` |
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
|         - |  516 | `}` |
|         - |  517 | `#endif` |
|      3678 |  518 | `PH7_PRIVATE sxi32 SyMemBackendInit(SyMemBackend *pBackend,ProcMemError xMemErr,void * pUserData)` |
|         5 |  519 | `{` |
|         - |  520 | `#if defined(UNTRUST)` |
|         - |  521 | `	if( pBackend == 0 ){` |
|         - |  522 | `		return SXERR_EMPTY;` |
|         - |  523 | `	}` |
|         - |  524 | `#endif` |
|         - |  525 | `	/* Zero the allocator first */` |
|      3683 |  526 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|      3683 |  527 | `	pBackend->xMemError = xMemErr;` |
|      3683 |  528 | `	pBackend->pUserData = pUserData;` |
|         - |  529 | `	/* Switch to the OS memory allocator */` |
|      3683 |  530 | `	pBackend->pMethods = &sOSAllocMethods;` |
|      3683 |  531 | `	if( pBackend->pMethods->xInit ){` |
|         - |  532 | `		/* Initialize the backend  */` |
|       ! 0 |  533 | `		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){` |
|       ! 0 |  534 | `			return SXERR_ABORT;` |
|         - |  535 | `		}` |
|       ! 0 |  536 | `	}` |
|         - |  537 | `#if defined(UNTRUST)` |
|         - |  538 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|         - |  539 | `#endif` |
|      3683 |  540 | `	return SXRET_OK;` |
|      1844 |  541 | `}` |
|       ! 0 |  542 | `PH7_PRIVATE sxi32 SyMemBackendInitFromOthers(SyMemBackend *pBackend,const SyMemMethods *pMethods,ProcMemError xMemErr,void * pUserData)` |
|       ! 0 |  543 | `{` |
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
|       ! 0 |  569 | `}` |
|      7352 |  570 | `PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend *pBackend,SyMemBackend *pParent)` |
|         5 |  571 | `{` |
|         - |  572 | `	sxu8 bInheritMutex;` |
|         - |  573 | `#if defined(UNTRUST)` |
|         - |  574 | `	if( pBackend == 0 \|\| SXMEM_BACKEND_CORRUPT(pParent) ){` |
|         - |  575 | `		return SXERR_CORRUPT;` |
|         - |  576 | `	}` |
|         - |  577 | `#endif` |
|         - |  578 | `	/* Zero the allocator first */` |
|      7357 |  579 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|      7357 |  580 | `	pBackend->pMethods  = pParent->pMethods;` |
|      7357 |  581 | `	pBackend->xMemError = pParent->xMemError;` |
|      7357 |  582 | `	pBackend->pUserData = pParent->pUserData;` |
|      7357 |  583 | `	pBackend->nMaxRequest = pParent->nMaxRequest;` |
|      7357 |  584 | `	bInheritMutex = pParent->pMutexMethods ? TRUE : FALSE;` |
|      7357 |  585 | `	if( bInheritMutex ){` |
|      3683 |  586 | `		pBackend->pMutexMethods = pParent->pMutexMethods;` |
|         - |  587 | `		/* Create a private mutex */` |
|      3683 |  588 | `		pBackend->pMutex = pBackend->pMutexMethods->xNew(SXMUTEX_TYPE_FAST);` |
|      3683 |  589 | `		if( pBackend->pMutex ==  0){` |
|       ! 0 |  590 | `			return SXERR_OS;` |
|         - |  591 | `		}` |
|      1839 |  592 | `	}` |
|         - |  593 | `#if defined(UNTRUST)` |
|         - |  594 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|         - |  595 | `#endif` |
|      7357 |  596 | `	return SXRET_OK;` |
|      3681 |  597 | `}` |
|      7716 |  598 | `static sxi32 MemBackendRelease(SyMemBackend *pBackend)` |
|         5 |  599 | `{` |
|         - |  600 | `	SyMemBlock *pBlock,*pNext;` |
|         - |  601 |  |
|      7721 |  602 | `	pBlock = pBackend->pBlocks;` |
|    686791 |  603 | `	for(;;){` |
|   1373587 |  604 | `		if( pBackend->nBlock == 0 ){` |
|       818 |  605 | `			break;` |
|         - |  606 | `		}` |
|   1372773 |  607 | `		pNext  = pBlock->pNext;` |
|   1372773 |  608 | `		pBackend->pMethods->xFree(pBlock);` |
|   1372773 |  609 | `		pBlock = pNext;` |
|   1372773 |  610 | `		pBackend->nBlock--;` |
|         - |  611 | `		/* LOOP ONE */` |
|   1372773 |  612 | `		if( pBackend->nBlock == 0 ){` |
|      5819 |  613 | `			break;` |
|         - |  614 | `		}` |
|   1366959 |  615 | `		pNext  = pBlock->pNext;` |
|   1366959 |  616 | `		pBackend->pMethods->xFree(pBlock);` |
|   1366959 |  617 | `		pBlock = pNext;` |
|   1366959 |  618 | `		pBackend->nBlock--;` |
|         - |  619 | `		/* LOOP TWO */` |
|   1366959 |  620 | `		if( pBackend->nBlock == 0 ){` |
|       553 |  621 | `			break;` |
|         - |  622 | `		}` |
|   1366411 |  623 | `		pNext  = pBlock->pNext;` |
|   1366411 |  624 | `		pBackend->pMethods->xFree(pBlock);` |
|   1366411 |  625 | `		pBlock = pNext;` |
|   1366411 |  626 | `		pBackend->nBlock--;` |
|         - |  627 | `		/* LOOP THREE */` |
|   1366411 |  628 | `		if( pBackend->nBlock == 0 ){` |
|       544 |  629 | `			break;` |
|         - |  630 | `		}` |
|   1365871 |  631 | `		pNext  = pBlock->pNext;` |
|   1365871 |  632 | `		pBackend->pMethods->xFree(pBlock);` |
|   1365871 |  633 | `		pBlock = pNext;` |
|   1365871 |  634 | `		pBackend->nBlock--;` |
|         - |  635 | `		/* LOOP FOUR */` |
|         5 |  636 | `	}` |
|      7721 |  637 | `	if( pBackend->pMethods->xRelease ){` |
|       ! 0 |  638 | `		pBackend->pMethods->xRelease(pBackend->pMethods->pUserData);` |
|       ! 0 |  639 | `	}` |
|      7721 |  640 | `	pBackend->pMethods = 0;` |
|      7721 |  641 | `	pBackend->pBlocks  = 0;` |
|         - |  642 | `#if defined(UNTRUST)` |
|         - |  643 | `	pBackend->nMagic = 0x2626;` |
|         - |  644 | `#endif` |
|      7721 |  645 | `	return SXRET_OK;` |
|         5 |  646 | `}` |
|      7716 |  647 | `PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend *pBackend)` |
|         5 |  648 | `{` |
|         - |  649 | `#if defined(UNTRUST)` |
|         - |  650 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  651 | `		return SXERR_INVALID;` |
|         - |  652 | `	}` |
|         - |  653 | `#endif` |
|      7721 |  654 | `	if( pBackend->pMutexMethods ){` |
|       368 |  655 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       182 |  656 | `	}` |
|      7721 |  657 | `	(void)MemBackendRelease(&(*pBackend));` |
|      7721 |  658 | `	if( pBackend->pMutexMethods ){` |
|       368 |  659 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       368 |  660 | `		SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|       182 |  661 | `	}` |
|      7721 |  662 | `	return SXRET_OK;` |
|         5 |  663 | `}` |
|    702396 |  664 | `PH7_PRIVATE void * SyMemBackendDup(SyMemBackend *pBackend,const void *pSrc,sxu32 nSize)` |
|         5 |  665 | `{` |
|         - |  666 | `	void *pNew;` |
|         - |  667 | `#if defined(UNTRUST)` |
|         - |  668 | `	if( pSrc == 0 \|\| nSize <= 0 ){` |
|         - |  669 | `		return 0;` |
|         - |  670 | `	}` |
|         - |  671 | `#endif` |
|    702401 |  672 | `	pNew = SyMemBackendAlloc(&(*pBackend),nSize);` |
|    702401 |  673 | `	if( pNew ){` |
|    702401 |  674 | `		SyMemcpy(pSrc,pNew,nSize);` |
|    351198 |  675 | `	}` |
|    702401 |  676 | `	return pNew;` |
|         5 |  677 | `}` |
|   3305572 |  678 | `PH7_PRIVATE char * SyMemBackendStrDup(SyMemBackend *pBackend,const char *zSrc,sxu32 nSize)` |
|         5 |  679 | `{` |
|         - |  680 | `	char *zDest;` |
|   3305577 |  681 | `	zDest = (char *)SyMemBackendAlloc(&(*pBackend),nSize + 1);` |
|   3305577 |  682 | `	if( zDest ){` |
|   3305577 |  683 | `		Systrcpy(zDest,nSize+1,zSrc,nSize);` |
|   1652786 |  684 | `	}` |
|   3305577 |  685 | `	return zDest;` |
|         5 |  686 | `}` |
|    277768 |  687 | `PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize)` |
|         5 |  688 | `{` |
|         - |  689 | `#if defined(UNTRUST)` |
|         - |  690 | `	if( pBlob == 0 \|\| pBuffer == 0 \|\| nSize < 1 ){` |
|         - |  691 | `		return SXERR_EMPTY;` |
|         - |  692 | `	}` |
|         - |  693 | `#endif` |
|    277773 |  694 | `	pBlob->pBlob = pBuffer;` |
|    277773 |  695 | `	pBlob->mByte = nSize;` |
|    277773 |  696 | `	pBlob->nByte = 0;` |
|    277773 |  697 | `	pBlob->pAllocator = 0;` |
|    277773 |  698 | `	pBlob->nFlags = SXBLOB_LOCKED\|SXBLOB_STATIC;` |
|    277773 |  699 | `	return SXRET_OK;` |
|         5 |  700 | `}` |
|  10096709 |  701 | `PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator)` |
|         5 |  702 | `{` |
|         - |  703 | `#if defined(UNTRUST)` |
|         - |  704 | `	if( pBlob == 0  ){` |
|         - |  705 | `		return SXERR_EMPTY;` |
|         - |  706 | `	}` |
|         - |  707 | `#endif` |
|  10096714 |  708 | `	pBlob->pBlob = 0;` |
|  10096714 |  709 | `	pBlob->mByte = pBlob->nByte	= 0;` |
|  10096714 |  710 | `	pBlob->pAllocator = &(*pAllocator);` |
|  10096714 |  711 | `	pBlob->nFlags = 0;` |
|  10096714 |  712 | `	return SXRET_OK;` |
|         5 |  713 | `}` |
|   3428986 |  714 | `PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte)` |
|         5 |  715 | `{` |
|         - |  716 | `#if defined(UNTRUST)` |
|         - |  717 | `	if( pBlob == 0  ){` |
|         - |  718 | `		return SXERR_EMPTY;` |
|         - |  719 | `	}` |
|         - |  720 | `#endif` |
|   3428991 |  721 | `	pBlob->pBlob = (void *)pData;` |
|   3428991 |  722 | `	pBlob->nByte = nByte;` |
|   3428991 |  723 | `	pBlob->mByte = 0;` |
|   3428991 |  724 | `	pBlob->nFlags \|= SXBLOB_RDONLY;` |
|   3428991 |  725 | `	return SXRET_OK;` |
|         5 |  726 | `}` |
|         - |  727 | `#ifndef SXBLOB_MIN_GROWTH` |
|         - |  728 | `#define SXBLOB_MIN_GROWTH 16` |
|         - |  729 | `#endif` |
|   9791884 |  730 | `static sxi32 BlobPrepareGrow(SyBlob *pBlob,sxu32 *pByte)` |
|         5 |  731 | `{` |
|         - |  732 | `	sxu32 nByte;` |
|         - |  733 | `	void *pNew;` |
|   9791889 |  734 | `	nByte = *pByte;` |
|   9791889 |  735 | `	if( pBlob->nFlags & (SXBLOB_LOCKED\|SXBLOB_STATIC) ){` |
|   2217747 |  736 | `		if ( SyBlobFreeSpace(pBlob) < nByte ){` |
|       ! 0 |  737 | `			*pByte = SyBlobFreeSpace(pBlob);` |
|       ! 0 |  738 | `			if( (*pByte) == 0 ){` |
|       ! 0 |  739 | `				return SXERR_SHORT;` |
|         - |  740 | `			}` |
|       ! 0 |  741 | `		}` |
|   2217747 |  742 | `		return SXRET_OK;` |
|         - |  743 | `	}` |
|   7574147 |  744 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|         - |  745 | `		/* Make a copy of the read-only item */` |
|    702401 |  746 | `		if( pBlob->nByte > 0 ){` |
|    702401 |  747 | `			pNew = SyMemBackendDup(pBlob->pAllocator,pBlob->pBlob,pBlob->nByte);` |
|    702401 |  748 | `			if( pNew == 0 ){` |
|       ! 0 |  749 | `				return SXERR_MEM;` |
|         - |  750 | `			}` |
|    702401 |  751 | `			pBlob->pBlob = pNew;` |
|    702401 |  752 | `			pBlob->mByte = pBlob->nByte;` |
|    351203 |  753 | `		}else{` |
|       ! 0 |  754 | `			pBlob->pBlob = 0;` |
|       ! 0 |  755 | `			pBlob->mByte = 0;` |
|         - |  756 | `		}` |
|         - |  757 | `		/* Remove the read-only flag */` |
|    702401 |  758 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|    351198 |  759 | `	}` |
|   7574147 |  760 | `	if( SyBlobFreeSpace(pBlob) >= nByte ){` |
|   1531880 |  761 | `		return SXRET_OK;` |
|         - |  762 | `	}` |
|   6042272 |  763 | `	if( pBlob->mByte > 0 ){` |
|    826763 |  764 | `		nByte = nByte + pBlob->mByte * 2 + SXBLOB_MIN_GROWTH;` |
|   5628889 |  765 | `	}else if ( nByte < SXBLOB_MIN_GROWTH ){` |
|   4081167 |  766 | `		nByte = SXBLOB_MIN_GROWTH;` |
|   2040489 |  767 | `	}` |
|   6042272 |  768 | `	pNew = SyMemBackendRealloc(pBlob->pAllocator,pBlob->pBlob,nByte);` |
|   6042272 |  769 | `	if( pNew == 0 ){` |
|       ! 0 |  770 | `		return SXERR_MEM;` |
|         - |  771 | `	}` |
|   6042272 |  772 | `	pBlob->pBlob = pNew;` |
|   6042272 |  773 | `	pBlob->mByte = nByte;` |
|   6042272 |  774 | `	return SXRET_OK;` |
|   4896034 |  775 | `}` |
|   9855690 |  776 | `PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize)` |
|         5 |  777 | `{` |
|         - |  778 | `	sxu8 *zBlob;` |
|         - |  779 | `	sxi32 rc;` |
|   9855695 |  780 | `	if( nSize < 1 ){` |
|     63811 |  781 | `		return SXRET_OK;` |
|         - |  782 | `	}` |
|   9791889 |  783 | `	rc = BlobPrepareGrow(&(*pBlob),&nSize);` |
|   9791889 |  784 | `	if( SXRET_OK != rc ){` |
|       ! 0 |  785 | `		return rc;` |
|         - |  786 | `	}` |
|   9791889 |  787 | `	if( pData ){` |
|   9791857 |  788 | `		zBlob = (sxu8 *)pBlob->pBlob ;` |
|   9791857 |  789 | `		zBlob = &zBlob[pBlob->nByte];` |
|   9791857 |  790 | `		pBlob->nByte += nSize;` |
|  43961782 |  791 | `		SX_MACRO_FAST_MEMCPY(pData,zBlob,nSize);` |
|   4896013 |  792 | `	}` |
|   9791889 |  793 | `	return SXRET_OK;` |
|   4927937 |  794 | `}` |
|    715365 |  795 | `PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob)` |
|         5 |  796 | `{` |
|         - |  797 | `	sxi32 rc;` |
|         - |  798 | `	sxu32 n;` |
|    715370 |  799 | `	n = pBlob->nByte;` |
|    715370 |  800 | `	rc = SyBlobAppend(&(*pBlob),(const void *)"\0",sizeof(char));` |
|    715370 |  801 | `	if (rc == SXRET_OK ){` |
|    715370 |  802 | `		pBlob->nByte = n;` |
|    357726 |  803 | `	}` |
|    715370 |  804 | `	return rc;` |
|         5 |  805 | `}` |
|   3898110 |  806 | `PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest)` |
|         5 |  807 | `{` |
|   3898115 |  808 | `	sxi32 rc = SXRET_OK;` |
|         - |  809 | `#ifdef UNTRUST` |
|         - |  810 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|         - |  811 | `		return SXERR_EMPTY;` |
|         - |  812 | `	}` |
|         - |  813 | `#endif` |
|   3898115 |  814 | `	if( pSrc->nByte > 0 ){` |
|   3863103 |  815 | `		rc = SyBlobAppend(&(*pDest),pSrc->pBlob,pSrc->nByte);` |
|   1931549 |  816 | `	}` |
|   3898115 |  817 | `	return rc;` |
|         5 |  818 | `}` |
|         8 |  819 | `PH7_PRIVATE sxi32 SyBlobCmp(SyBlob *pLeft,SyBlob *pRight)` |
|         1 |  820 | `{` |
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
|         5 |  837 | `}` |
|   4630656 |  838 | `PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob)` |
|         5 |  839 | `{` |
|   4630661 |  840 | `	pBlob->nByte = 0;` |
|   4630661 |  841 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|      4899 |  842 | `		pBlob->pBlob = 0;` |
|      4899 |  843 | `		pBlob->mByte = 0;` |
|      4899 |  844 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|      2447 |  845 | `	}` |
|   4630661 |  846 | `	return SXRET_OK;` |
|         5 |  847 | `}` |
|  12671521 |  848 | `PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob)` |
|         5 |  849 | `{` |
|  12671526 |  850 | `	if( (pBlob->nFlags & (SXBLOB_STATIC\|SXBLOB_RDONLY)) == 0 && pBlob->mByte > 0 ){` |
|   5388720 |  851 | `		SyMemBackendFree(pBlob->pAllocator,pBlob->pBlob);` |
|   2694401 |  852 | `	}` |
|  12671526 |  853 | `	pBlob->pBlob = 0;` |
|  12671526 |  854 | `	pBlob->nByte = pBlob->mByte = 0;` |
|  12671526 |  855 | `	pBlob->nFlags = 0;` |
|  12671526 |  856 | `	return SXRET_OK;` |
|         5 |  857 | `}` |
|         - |  858 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    168272 |  859 | `PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft)` |
|         5 |  860 | `{` |
|    168277 |  861 | `	const char *zIn = (const char *)pBlob;` |
|         - |  862 | `	const char *zEnd;` |
|         - |  863 | `	sxi32 rc;` |
|    168277 |  864 | `	if( pLen > nLen ){` |
|      6259 |  865 | `		return SXERR_NOTFOUND;` |
|         - |  866 | `	}` |
|    162023 |  867 | `	zEnd = &zIn[nLen-pLen];` |
|   1396016 |  868 | `	for(;;){` |
|   2791971 |  869 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   2751637 |  870 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   2694155 |  871 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   2658585 |  872 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|         5 |  873 | `	}` |
|     24481 |  874 | `	return SXERR_NOTFOUND;` |
|     84141 |  875 | `}` |
|         - |  876 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - |  877 |  |
