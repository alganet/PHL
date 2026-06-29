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
|  15629585 |   18 | `static void * SyOSHeapAlloc(sxu32 nByte)` |
|         5 |   19 |  |
|         - |   20 | `	void *pNew;` |
|         - |   21 | `#if defined(__WINNT__)` |
|         5 |   22 | `	pNew = HeapAlloc(GetProcessHeap(),0,nByte);` |
|         - |   23 | `#else` |
|  15629585 |   24 | `	pNew = malloc((size_t)nByte);` |
|         - |   25 | `#endif` |
|  15629590 |   26 | `	return pNew;` |
|         5 |   27 |  |
|    959660 |   28 | `static void * SyOSHeapRealloc(void *pOld,sxu32 nByte)` |
|         5 |   29 |  |
|         - |   30 | `	void *pNew;` |
|         - |   31 | `#if defined(__WINNT__)` |
|         5 |   32 | `	pNew = HeapReAlloc(GetProcessHeap(),0,pOld,nByte);` |
|         - |   33 | `#else` |
|    959660 |   34 | `	pNew = realloc(pOld,(size_t)nByte);` |
|         - |   35 | `#endif` |
|    959665 |   36 | `	return pNew;` |
|         5 |   37 |  |
|  15626389 |   38 | `static void SyOSHeapFree(void *pPtr)` |
|         5 |   39 |  |
|         - |   40 | `#if defined(__WINNT__)` |
|         5 |   41 | `	HeapFree(GetProcessHeap(),0,pPtr);` |
|         - |   42 | `#else` |
|  15626389 |   43 | `	free(pPtr);` |
|         - |   44 | `#endif` |
|  15626394 |   45 |  |
|         - |   46 |  |
|         - |   47 |  |
|  28377641 |   48 | `PH7_PRIVATE void SyZero(void *pSrc,sxu32 nSize)` |
|         5 |   49 |  |
|  28377646 |   50 | `	register unsigned char *zSrc = (unsigned char *)pSrc;` |
|         - |   51 | `	unsigned char *zEnd;` |
|         - |   52 | `#if defined(UNTRUST)` |
|         - |   53 | `	if( zSrc == 0 \|\| nSize <= 0 ){` |
|         - |   54 | `		return ;` |
|         - |   55 | `	}` |
|         - |   56 | `#endif` |
|  28377646 |   57 | `	zEnd = &zSrc[nSize];` |
| 383084800 |   58 | `	for(;;){` |
| 766167230 |   59 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 737789813 |   60 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 737789729 |   61 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 737789619 |   62 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
|         5 |   63 | `	}` |
|  28377646 |   64 |  |
|  28659065 |   65 | `PH7_PRIVATE sxi32 SyMemcmp(const void *pB1,const void *pB2,sxu32 nSize)` |
|         5 |   66 |  |
|         - |   67 | `	sxi32 rc;` |
|  28659070 |   68 | `	if( nSize <= 0 ){` |
|        93 |   69 | `		return 0;` |
|         - |   70 | `	}` |
|  28658978 |   71 | `	if( pB1 == 0 \|\| pB2 == 0 ){` |
|       ! 0 |   72 | `		return pB1 != 0 ? 1 : (pB2 == 0 ? 0 : -1);` |
|         - |   73 | `	}` |
|  62210869 |   74 | `	SX_MACRO_FAST_CMP(pB1,pB2,nSize,rc);` |
|  28658978 |   75 | `	return rc;` |
|  14330295 |   76 |  |
|  11380147 |   77 | `PH7_PRIVATE sxu32 SyMemcpy(const void *pSrc,void *pDest,sxu32 nLen)` |
|         5 |   78 |  |
|         - |   79 | `#if defined(UNTRUST)` |
|         - |   80 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|         - |   81 | `		return 0;` |
|         - |   82 | `	}` |
|         - |   83 | `#endif` |
|  11380152 |   84 | `	if( pSrc == (const void *)pDest ){` |
|       ! 0 |   85 | `		return nLen;` |
|         - |   86 | `	}` |
|  93383234 |   87 | `	SX_MACRO_FAST_MEMCPY(pSrc,pDest,nLen);` |
|  11380152 |   88 | `	return nLen;` |
|   5690394 |   89 |  |
|  15629585 |   90 | `static void * MemOSAlloc(sxu32 nBytes)` |
|         5 |   91 |  |
|         - |   92 | `	sxu32 *pChunk;` |
|  15629590 |   93 | `	pChunk = (sxu32 *)SyOSHeapAlloc(nBytes + sizeof(sxu32));` |
|  15629590 |   94 | `	if( pChunk == 0 ){` |
|       ! 0 |   95 | `		return 0;` |
|         - |   96 | `	}` |
|  15629590 |   97 | `	pChunk[0] = nBytes;` |
|  15629590 |   98 | `	return (void *)&pChunk[1];` |
|   7814840 |   99 |  |
|    959660 |  100 | `static void * MemOSRealloc(void *pOld,sxu32 nBytes)` |
|         5 |  101 |  |
|         - |  102 | `	sxu32 *pOldChunk;` |
|         - |  103 | `	sxu32 *pChunk;` |
|    959665 |  104 | `	pOldChunk = (sxu32 *)(((char *)pOld)-sizeof(sxu32));` |
|    959665 |  105 | `	if( pOldChunk[0] >= nBytes ){` |
|       ! 0 |  106 | `		return pOld;` |
|         - |  107 | `	}` |
|    959665 |  108 | `	pChunk = (sxu32 *)SyOSHeapRealloc(pOldChunk,nBytes + sizeof(sxu32));` |
|    959665 |  109 | `	if( pChunk == 0 ){` |
|       ! 0 |  110 | `		return 0;` |
|         - |  111 | `	}` |
|    959665 |  112 | `	pChunk[0] = nBytes;` |
|    959665 |  113 | `	return (void *)&pChunk[1];` |
|    479832 |  114 |  |
|  15626389 |  115 | `static void MemOSFree(void *pBlock)` |
|         5 |  116 |  |
|         - |  117 | `	void *pChunk;` |
|  15626394 |  118 | `	pChunk = (void *)(((char *)pBlock)-sizeof(sxu32));` |
|  15626394 |  119 | `	SyOSHeapFree(pChunk);` |
|  15626394 |  120 |  |
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
|  15629585 |  137 | `static void * MemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         5 |  138 |  |
|         - |  139 | `	SyMemBlock *pBlock;` |
|  15629590 |  140 | `	sxi32 nRetry = 0;` |
|         - |  141 |  |
|         - |  142 | `	/* Append an extra block so we can tracks allocated chunks and avoid memory` |
|         - |  143 | `	 * leaks.` |
|         - |  144 | `	 */` |
|  15629590 |  145 | `	nByte += sizeof(SyMemBlock);` |
|         - |  146 | `	/* Enforce the optional per-allocation cap (0 = unlimited). A capped failure` |
|         - |  147 | `	 * returns NULL just like a genuine OS failure, driving the normal SXERR_MEM` |
|         - |  148 | `	 * propagation; the retry callback is intentionally skipped (hard limit). */` |
|  15629590 |  149 | `	if( pBackend->nMaxRequest && nByte > pBackend->nMaxRequest ){` |
|       ! 0 |  150 | `		return 0;` |
|         - |  151 | `	}` |
|   7814835 |  152 | `	for(;;){` |
|   7814840 |  153 | `		pBlock = (SyMemBlock *)pBackend->pMethods->xAlloc(nByte);` |
|  15629585 |  154 | `		if( pBlock != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY` |
|         5 |  155 | `			\|\| SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|   7814840 |  156 | `				break;` |
|         - |  157 | `		}` |
|       ! 0 |  158 | `		nRetry++;` |
|       ! 0 |  159 | `	}` |
|  15629590 |  160 | `	if( pBlock  == 0 ){` |
|       ! 0 |  161 | `		return 0;` |
|         - |  162 | `	}` |
|  15629590 |  163 | `	pBlock->pNext = pBlock->pPrev = 0;` |
|         - |  164 | `	/* Link to the list of already tracked blocks */` |
|  15629590 |  165 | `	MACRO_LD_PUSH(pBackend->pBlocks,pBlock);` |
|         - |  166 | `#if defined(UNTRUST)` |
|         - |  167 | `	pBlock->nGuard = SXMEM_BACKEND_MAGIC;` |
|         - |  168 | `#endif` |
|  15629590 |  169 | `	pBackend->nBlock++;` |
|  15629590 |  170 | `	return (void *)&pBlock[1];` |
|   7814840 |  171 |  |
|   6057258 |  172 | `PH7_PRIVATE void * SyMemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         5 |  173 |  |
|         - |  174 | `	void *pChunk;` |
|         - |  175 | `#if defined(UNTRUST)` |
|         - |  176 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  177 | `		return 0;` |
|         - |  178 | `	}` |
|         - |  179 | `#endif` |
|   6057263 |  180 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  181 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  182 | `	}` |
|   6057263 |  183 | `	pChunk = MemBackendAlloc(&(*pBackend),nByte);` |
|   6057263 |  184 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  185 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  186 | `	}` |
|   6057263 |  187 | `	return pChunk;` |
|         5 |  188 |  |
|  10463365 |  189 | `static void * MemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         5 |  190 |  |
|         - |  191 | `	SyMemBlock *pBlock,*pNew,*pPrev,*pNext;` |
|  10463370 |  192 | `	sxu32 nRetry = 0;` |
|         - |  193 |  |
|  10463370 |  194 | `	if( pOld == 0 ){` |
|   9503710 |  195 | `		return MemBackendAlloc(&(*pBackend),nByte);` |
|         - |  196 | `	}` |
|    959665 |  197 | `	pBlock = (SyMemBlock *)(((char *)pOld) - sizeof(SyMemBlock));` |
|         - |  198 | `#if defined(UNTRUST)` |
|         - |  199 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|         - |  200 | `		return 0;` |
|         - |  201 | `	}` |
|         - |  202 | `#endif` |
|    959665 |  203 | `	nByte += sizeof(SyMemBlock);` |
|         - |  204 | `	/* Enforce the optional per-allocation cap (0 = unlimited); see MemBackendAlloc. */` |
|    959665 |  205 | `	if( pBackend->nMaxRequest && nByte > pBackend->nMaxRequest ){` |
|       ! 0 |  206 | `		return 0;` |
|         - |  207 | `	}` |
|    959665 |  208 | `	pPrev = pBlock->pPrev;` |
|    959665 |  209 | `	pNext = pBlock->pNext;` |
|    479827 |  210 | `	for(;;){` |
|    479832 |  211 | `		pNew = (SyMemBlock *)pBackend->pMethods->xRealloc(pBlock,nByte);` |
|    959665 |  212 | `		if( pNew != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY \|\|` |
|       ! 0 |  213 | `			SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|    479832 |  214 | `				break;` |
|         - |  215 | `		}` |
|       ! 0 |  216 | `		nRetry++;` |
|       ! 0 |  217 | `	}` |
|    959665 |  218 | `	if( pNew == 0 ){` |
|       ! 0 |  219 | `		return 0;` |
|         - |  220 | `	}` |
|    959665 |  221 | `	if( pNew != pBlock ){` |
|    879304 |  222 | `		if( pPrev == 0 ){` |
|    666522 |  223 | `			pBackend->pBlocks = pNew;` |
|    362799 |  224 | `		}else{` |
|    212787 |  225 | `			pPrev->pNext = pNew;` |
|         - |  226 | `		}` |
|    879304 |  227 | `		if( pNext ){` |
|    879292 |  228 | `			pNext->pPrev = pNew;` |
|    478037 |  229 | `		}` |
|         - |  230 | `#if defined(UNTRUST)` |
|         - |  231 | `		pNew->nGuard = SXMEM_BACKEND_MAGIC;` |
|         - |  232 | `#endif` |
|    478044 |  233 | `	}` |
|    959665 |  234 | `	return (void *)&pNew[1];` |
|   5231727 |  235 |  |
|  10463365 |  236 | `PH7_PRIVATE void * SyMemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|         5 |  237 |  |
|         - |  238 | `	void *pChunk;` |
|         - |  239 | `#if defined(UNTRUST)` |
|         - |  240 | `	if( SXMEM_BACKEND_CORRUPT(pBackend)  ){` |
|         - |  241 | `		return 0;` |
|         - |  242 | `	}` |
|         - |  243 | `#endif` |
|  10463370 |  244 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  245 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  246 | `	}` |
|  10463370 |  247 | `	pChunk = MemBackendRealloc(&(*pBackend),pOld,nByte);` |
|  10463370 |  248 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  249 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  250 | `	}` |
|  10463370 |  251 | `	return pChunk;` |
|         5 |  252 |  |
|  10541621 |  253 | `static sxi32 MemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|         5 |  254 |  |
|         - |  255 | `	SyMemBlock *pBlock;` |
|  10541626 |  256 | `	pBlock = (SyMemBlock *)(((char *)pChunk) - sizeof(SyMemBlock));` |
|         - |  257 | `#if defined(UNTRUST)` |
|         - |  258 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|         - |  259 | `		return SXERR_CORRUPT;` |
|         - |  260 | `	}` |
|         - |  261 | `#endif` |
|         - |  262 | `	/* Unlink from the list of active blocks */` |
|  10541626 |  263 | `	if( pBackend->nBlock > 0 ){` |
|         - |  264 | `		/* Release the block */` |
|         - |  265 | `#if defined(UNTRUST)` |
|         - |  266 | `		/* Mark as stale block */` |
|         - |  267 | `		pBlock->nGuard = 0x635B;` |
|         - |  268 | `#endif` |
|  10541626 |  269 | `		MACRO_LD_REMOVE(pBackend->pBlocks,pBlock);` |
|  10541626 |  270 | `		pBackend->nBlock--;` |
|  10541626 |  271 | `		pBackend->pMethods->xFree(pBlock);` |
|   5270853 |  272 | `	}` |
|  10541626 |  273 | `	return SXRET_OK;` |
|         5 |  274 |  |
|  10541621 |  275 | `PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|         5 |  276 |  |
|         - |  277 | `	sxi32 rc;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  280 | `		return SXERR_CORRUPT;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|  10541626 |  283 | `	if( pChunk == 0 ){` |
|       ! 0 |  284 | `		return SXRET_OK;` |
|         - |  285 | `	}` |
|  10541626 |  286 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  287 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  288 | `	}` |
|  10541626 |  289 | `	rc = MemBackendFree(&(*pBackend),pChunk);` |
|  10541626 |  290 | `	if( pBackend->pMutexMethods ){` |
|       ! 0 |  291 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       ! 0 |  292 | `	}` |
|  10541626 |  293 | `	return rc;` |
|   5270858 |  294 |  |
|         - |  295 | `#if defined(PH7_ENABLE_THREADS)` |
|      3542 |  296 | `PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods)` |
|         5 |  297 |  |
|         - |  298 | `	SyMutex *pMutex;` |
|         - |  299 | `#if defined(UNTRUST)` |
|         - |  300 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pMethods == 0 \|\| pMethods->xNew == 0){` |
|         - |  301 | `		return SXERR_CORRUPT;` |
|         - |  302 | `	}` |
|         - |  303 | `#endif` |
|      3547 |  304 | `	pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|      3547 |  305 | `	if( pMutex == 0 ){` |
|       ! 0 |  306 | `		return SXERR_OS;` |
|         - |  307 | `	}` |
|         - |  308 | `	/* Attach the mutex to the memory backend */` |
|      3547 |  309 | `	pBackend->pMutex = pMutex;` |
|      3547 |  310 | `	pBackend->pMutexMethods = pMethods;` |
|      3547 |  311 | `	return SXRET_OK;` |
|      1776 |  312 |  |
|      3542 |  313 | `PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend)` |
|         5 |  314 |  |
|         - |  315 | `#if defined(UNTRUST)` |
|         - |  316 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  317 | `		return SXERR_CORRUPT;` |
|         - |  318 | `	}` |
|         - |  319 | `#endif` |
|      3547 |  320 | `	if( pBackend->pMutex == 0 ){` |
|         - |  321 | `		/* There is no mutex subsystem at all */` |
|       ! 0 |  322 | `		return SXRET_OK;` |
|         - |  323 | `	}` |
|      3547 |  324 | `	SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|      3547 |  325 | `	pBackend->pMutexMethods = 0;` |
|      3547 |  326 | `	pBackend->pMutex = 0;` |
|      3547 |  327 | `	return SXRET_OK;` |
|      1776 |  328 |  |
|         - |  329 | `#endif` |
|         - |  330 | `/*` |
|         - |  331 | ` * Memory pool allocator` |
|         - |  332 | ` */` |
|         - |  333 | `#define SXMEM_POOL_MAGIC		0xDEAD` |
|         - |  334 | `#define SXMEM_POOL_MAXALLOC		(1<<(SXMEM_POOL_NBUCKETS+SXMEM_POOL_INCR))` |
|         - |  335 | `#define SXMEM_POOL_MINALLOC		(1<<(SXMEM_POOL_INCR))` |
|     68622 |  336 | `static sxi32 MemPoolBucketAlloc(SyMemBackend *pBackend,sxu32 nBucket)` |
|         5 |  337 |  |
|         - |  338 | `	char *zBucket,*zBucketEnd;` |
|         - |  339 | `	SyMemHeader *pHeader;` |
|         - |  340 | `	sxu32 nBucketSize;` |
|         - |  341 |  |
|         - |  342 | `	/* Allocate one big block first */` |
|     68627 |  343 | `	zBucket = (char *)MemBackendAlloc(&(*pBackend),SXMEM_POOL_MAXALLOC);` |
|     68627 |  344 | `	if( zBucket == 0 ){` |
|       ! 0 |  345 | `		return SXERR_MEM;` |
|         - |  346 | `	}` |
|     68627 |  347 | `	zBucketEnd = &zBucket[SXMEM_POOL_MAXALLOC];` |
|         - |  348 | `	/* Divide the big block into mini bucket pool */` |
|     68627 |  349 | `	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);` |
|     68627 |  350 | `	pBackend->apPool[nBucket] = pHeader = (SyMemHeader *)zBucket;` |
|   7805832 |  351 | `	for(;;){` |
|  15611669 |  352 | `		if( &zBucket[nBucketSize] >= zBucketEnd ){` |
|     68627 |  353 | `			break;` |
|         - |  354 | `		}` |
|  15543047 |  355 | `		pHeader->pNext = (SyMemHeader *)&zBucket[nBucketSize];` |
|         - |  356 | `		/* Advance the cursor to the next available chunk */` |
|  15543047 |  357 | `		pHeader = pHeader->pNext;` |
|  15543047 |  358 | `		zBucket += nBucketSize;` |
|         5 |  359 | `	}` |
|     68627 |  360 | `	pHeader->pNext = 0;` |
|         - |  361 |  |
|     68627 |  362 | `	return SXRET_OK;` |
|     34316 |  363 |  |
|  19439234 |  364 | `static void * MemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         5 |  365 |  |
|         - |  366 | `	SyMemHeader *pBucket,*pNext;` |
|         - |  367 | `	sxu32 nBucketSize;` |
|         - |  368 | `	sxu32 nBucket;` |
|         - |  369 |  |
|  19439239 |  370 | `	if( nByte + sizeof(SyMemHeader) >= SXMEM_POOL_MAXALLOC ){` |
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
|  19439239 |  381 | `	nBucket = 0;` |
|  19439239 |  382 | `	nBucketSize = SXMEM_POOL_MINALLOC;` |
|  97374377 |  383 | `	while( nByte + sizeof(SyMemHeader) > nBucketSize  ){` |
|  77935143 |  384 | `		nBucketSize <<= 1;` |
|  77935143 |  385 | `		nBucket++;` |
|         5 |  386 | `	}` |
|  19439239 |  387 | `	pBucket = pBackend->apPool[nBucket];` |
|  19439239 |  388 | `	if( pBucket == 0 ){` |
|         - |  389 | `		sxi32 rc;` |
|     68627 |  390 | `		rc = MemPoolBucketAlloc(&(*pBackend),nBucket);` |
|     68627 |  391 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  392 | `			return 0;` |
|         - |  393 | `		}` |
|     68627 |  394 | `		pBucket = pBackend->apPool[nBucket];` |
|     34311 |  395 | `	}` |
|         - |  396 | `	/* Remove from the free list */` |
|  19439239 |  397 | `	pNext = pBucket->pNext;` |
|  19439239 |  398 | `	pBackend->apPool[nBucket] = pNext;` |
|         - |  399 | `	/* Record bucket&magic number */` |
|  19439239 |  400 | `	pBucket->nBucket = (SXMEM_POOL_MAGIC << 16) \| nBucket;` |
|  19439239 |  401 | `	return (void *)&pBucket[1];` |
|   9719622 |  402 |  |
|  19439234 |  403 | `PH7_PRIVATE void * SyMemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|         5 |  404 |  |
|         - |  405 | `	void *pChunk;` |
|         - |  406 | `#if defined(UNTRUST)` |
|         - |  407 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  408 | `		return 0;` |
|         - |  409 | `	}` |
|         - |  410 | `#endif` |
|  19439239 |  411 | `	if( pBackend->pMutexMethods ){` |
|      3547 |  412 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1771 |  413 | `	}` |
|  19439239 |  414 | `	pChunk = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|  19439239 |  415 | `	if( pBackend->pMutexMethods ){` |
|      3547 |  416 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1771 |  417 | `	}` |
|  19439239 |  418 | `	return pChunk;` |
|         5 |  419 |  |
|  10937660 |  420 | `static sxi32 MemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|         5 |  421 |  |
|         - |  422 | `	SyMemHeader *pHeader;` |
|         - |  423 | `	sxu32 nBucket;` |
|         - |  424 | `	/* Get the corresponding bucket */` |
|  10937665 |  425 | `	pHeader = (SyMemHeader *)(((char *)pChunk) - sizeof(SyMemHeader));` |
|         - |  426 | `	/* Sanity check to avoid misuse */` |
|  10937665 |  427 | `	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){` |
|         3 |  428 | `		return SXERR_CORRUPT;` |
|         - |  429 | `	}` |
|  10937663 |  430 | `	nBucket = pHeader->nBucket & 0xFFFF;` |
|  10937663 |  431 | `	if( nBucket == SXU16_HIGH ){` |
|         - |  432 | `		/* Free the big block */` |
|       ! 0 |  433 | `		MemBackendFree(&(*pBackend),pHeader);` |
|  10937663 |  434 | `	}else if( nBucket >= SXMEM_POOL_NBUCKETS + SXMEM_POOL_INCR ){` |
|         - |  435 | `		/* Corrupted or misused bucket index */` |
|       ! 0 |  436 | `		return SXERR_CORRUPT;` |
|       ! 0 |  437 | `	}else{` |
|         - |  438 | `		/* Return to the free list */` |
|  10937663 |  439 | `		pHeader->pNext = pBackend->apPool[nBucket];` |
|  10937663 |  440 | `		pBackend->apPool[nBucket] = pHeader;` |
|         - |  441 | `	}` |
|  10937663 |  442 | `	return SXRET_OK;` |
|   5468835 |  443 |  |
|  10937660 |  444 | `PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|         5 |  445 |  |
|         - |  446 | `	sxi32 rc;` |
|         - |  447 | `#if defined(UNTRUST)` |
|         - |  448 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pChunk == 0 ){` |
|         - |  449 | `		return SXERR_CORRUPT;` |
|         - |  450 | `	}` |
|         - |  451 | `#endif` |
|  10937665 |  452 | `	if( pBackend->pMutexMethods ){` |
|      3201 |  453 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1598 |  454 | `	}` |
|  10937665 |  455 | `	rc = MemBackendPoolFree(&(*pBackend),pChunk);` |
|  10937665 |  456 | `	if( pBackend->pMutexMethods ){` |
|      3201 |  457 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|      1598 |  458 | `	}` |
|  10937665 |  459 | `	return rc;` |
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
|      3542 |  518 | `PH7_PRIVATE sxi32 SyMemBackendInit(SyMemBackend *pBackend,ProcMemError xMemErr,void * pUserData)` |
|         5 |  519 |  |
|         - |  520 | `#if defined(UNTRUST)` |
|         - |  521 | `	if( pBackend == 0 ){` |
|         - |  522 | `		return SXERR_EMPTY;` |
|         - |  523 | `	}` |
|         - |  524 | `#endif` |
|         - |  525 | `	/* Zero the allocator first */` |
|      3547 |  526 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|      3547 |  527 | `	pBackend->xMemError = xMemErr;` |
|      3547 |  528 | `	pBackend->pUserData = pUserData;` |
|         - |  529 | `	/* Switch to the OS memory allocator */` |
|      3547 |  530 | `	pBackend->pMethods = &sOSAllocMethods;` |
|      3547 |  531 | `	if( pBackend->pMethods->xInit ){` |
|         - |  532 | `		/* Initialize the backend  */` |
|       ! 0 |  533 | `		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){` |
|       ! 0 |  534 | `			return SXERR_ABORT;` |
|         - |  535 | `		}` |
|       ! 0 |  536 | `	}` |
|         - |  537 | `#if defined(UNTRUST)` |
|         - |  538 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|         - |  539 | `#endif` |
|      3547 |  540 | `	return SXRET_OK;` |
|      1776 |  541 |  |
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
|      7080 |  570 | `PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend *pBackend,SyMemBackend *pParent)` |
|         5 |  571 |  |
|         - |  572 | `	sxu8 bInheritMutex;` |
|         - |  573 | `#if defined(UNTRUST)` |
|         - |  574 | `	if( pBackend == 0 \|\| SXMEM_BACKEND_CORRUPT(pParent) ){` |
|         - |  575 | `		return SXERR_CORRUPT;` |
|         - |  576 | `	}` |
|         - |  577 | `#endif` |
|         - |  578 | `	/* Zero the allocator first */` |
|      7085 |  579 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|      7085 |  580 | `	pBackend->pMethods  = pParent->pMethods;` |
|      7085 |  581 | `	pBackend->xMemError = pParent->xMemError;` |
|      7085 |  582 | `	pBackend->pUserData = pParent->pUserData;` |
|      7085 |  583 | `	pBackend->nMaxRequest = pParent->nMaxRequest;` |
|      7085 |  584 | `	bInheritMutex = pParent->pMutexMethods ? TRUE : FALSE;` |
|      7085 |  585 | `	if( bInheritMutex ){` |
|      3547 |  586 | `		pBackend->pMutexMethods = pParent->pMutexMethods;` |
|         - |  587 | `		/* Create a private mutex */` |
|      3547 |  588 | `		pBackend->pMutex = pBackend->pMutexMethods->xNew(SXMUTEX_TYPE_FAST);` |
|      3547 |  589 | `		if( pBackend->pMutex ==  0){` |
|       ! 0 |  590 | `			return SXERR_OS;` |
|         - |  591 | `		}` |
|      1771 |  592 | `	}` |
|         - |  593 | `#if defined(UNTRUST)` |
|         - |  594 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|         - |  595 | `#endif` |
|      7085 |  596 | `	return SXRET_OK;` |
|      3545 |  597 |  |
|      7426 |  598 | `static sxi32 MemBackendRelease(SyMemBackend *pBackend)` |
|         5 |  599 |  |
|         - |  600 | `	SyMemBlock *pBlock,*pNext;` |
|         - |  601 |  |
|      7431 |  602 | `	pBlock = pBackend->pBlocks;` |
|    637948 |  603 | `	for(;;){` |
|   1275901 |  604 | `		if( pBackend->nBlock == 0 ){` |
|       585 |  605 | `			break;` |
|         - |  606 | `		}` |
|   1275321 |  607 | `		pNext  = pBlock->pNext;` |
|   1275321 |  608 | `		pBackend->pMethods->xFree(pBlock);` |
|   1275321 |  609 | `		pBlock = pNext;` |
|   1275321 |  610 | `		pBackend->nBlock--;` |
|         - |  611 | `		/* LOOP ONE */` |
|   1275321 |  612 | `		if( pBackend->nBlock == 0 ){` |
|      4431 |  613 | `			break;` |
|         - |  614 | `		}` |
|   1270895 |  615 | `		pNext  = pBlock->pNext;` |
|   1270895 |  616 | `		pBackend->pMethods->xFree(pBlock);` |
|   1270895 |  617 | `		pBlock = pNext;` |
|   1270895 |  618 | `		pBackend->nBlock--;` |
|         - |  619 | `		/* LOOP TWO */` |
|   1270895 |  620 | `		if( pBackend->nBlock == 0 ){` |
|       802 |  621 | `			break;` |
|         - |  622 | `		}` |
|   1270097 |  623 | `		pNext  = pBlock->pNext;` |
|   1270097 |  624 | `		pBackend->pMethods->xFree(pBlock);` |
|   1270097 |  625 | `		pBlock = pNext;` |
|   1270097 |  626 | `		pBackend->nBlock--;` |
|         - |  627 | `		/* LOOP THREE */` |
|   1270097 |  628 | `		if( pBackend->nBlock == 0 ){` |
|      1626 |  629 | `			break;` |
|         - |  630 | `		}` |
|   1268475 |  631 | `		pNext  = pBlock->pNext;` |
|   1268475 |  632 | `		pBackend->pMethods->xFree(pBlock);` |
|   1268475 |  633 | `		pBlock = pNext;` |
|   1268475 |  634 | `		pBackend->nBlock--;` |
|         - |  635 | `		/* LOOP FOUR */` |
|         5 |  636 | `	}` |
|      7431 |  637 | `	if( pBackend->pMethods->xRelease ){` |
|       ! 0 |  638 | `		pBackend->pMethods->xRelease(pBackend->pMethods->pUserData);` |
|       ! 0 |  639 | `	}` |
|      7431 |  640 | `	pBackend->pMethods = 0;` |
|      7431 |  641 | `	pBackend->pBlocks  = 0;` |
|         - |  642 | `#if defined(UNTRUST)` |
|         - |  643 | `	pBackend->nMagic = 0x2626;` |
|         - |  644 | `#endif` |
|      7431 |  645 | `	return SXRET_OK;` |
|         5 |  646 |  |
|      7426 |  647 | `PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend *pBackend)` |
|         5 |  648 |  |
|         - |  649 | `#if defined(UNTRUST)` |
|         - |  650 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|         - |  651 | `		return SXERR_INVALID;` |
|         - |  652 | `	}` |
|         - |  653 | `#endif` |
|      7431 |  654 | `	if( pBackend->pMutexMethods ){` |
|       350 |  655 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       173 |  656 | `	}` |
|      7431 |  657 | `	(void)MemBackendRelease(&(*pBackend));` |
|      7431 |  658 | `	if( pBackend->pMutexMethods ){` |
|       350 |  659 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       350 |  660 | `		SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|       173 |  661 | `	}` |
|      7431 |  662 | `	return SXRET_OK;` |
|         5 |  663 |  |
|    683880 |  664 | `PH7_PRIVATE void * SyMemBackendDup(SyMemBackend *pBackend,const void *pSrc,sxu32 nSize)` |
|         5 |  665 |  |
|         - |  666 | `	void *pNew;` |
|         - |  667 | `#if defined(UNTRUST)` |
|         - |  668 | `	if( pSrc == 0 \|\| nSize <= 0 ){` |
|         - |  669 | `		return 0;` |
|         - |  670 | `	}` |
|         - |  671 | `#endif` |
|    683885 |  672 | `	pNew = SyMemBackendAlloc(&(*pBackend),nSize);` |
|    683885 |  673 | `	if( pNew ){` |
|    683885 |  674 | `		SyMemcpy(pSrc,pNew,nSize);` |
|    341940 |  675 | `	}` |
|    683885 |  676 | `	return pNew;` |
|         5 |  677 |  |
|   3085310 |  678 | `PH7_PRIVATE char * SyMemBackendStrDup(SyMemBackend *pBackend,const char *zSrc,sxu32 nSize)` |
|         5 |  679 |  |
|         - |  680 | `	char *zDest;` |
|   3085315 |  681 | `	zDest = (char *)SyMemBackendAlloc(&(*pBackend),nSize + 1);` |
|   3085315 |  682 | `	if( zDest ){` |
|   3085315 |  683 | `		Systrcpy(zDest,nSize+1,zSrc,nSize);` |
|   1542655 |  684 | `	}` |
|   3085315 |  685 | `	return zDest;` |
|         5 |  686 |  |
|    253294 |  687 | `PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize)` |
|         5 |  688 |  |
|         - |  689 | `#if defined(UNTRUST)` |
|         - |  690 | `	if( pBlob == 0 \|\| pBuffer == 0 \|\| nSize < 1 ){` |
|         - |  691 | `		return SXERR_EMPTY;` |
|         - |  692 | `	}` |
|         - |  693 | `#endif` |
|    253299 |  694 | `	pBlob->pBlob = pBuffer;` |
|    253299 |  695 | `	pBlob->mByte = nSize;` |
|    253299 |  696 | `	pBlob->nByte = 0;` |
|    253299 |  697 | `	pBlob->pAllocator = 0;` |
|    253299 |  698 | `	pBlob->nFlags = SXBLOB_LOCKED\|SXBLOB_STATIC;` |
|    253299 |  699 | `	return SXRET_OK;` |
|         5 |  700 |  |
|   9353231 |  701 | `PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator)` |
|         5 |  702 |  |
|         - |  703 | `#if defined(UNTRUST)` |
|         - |  704 | `	if( pBlob == 0  ){` |
|         - |  705 | `		return SXERR_EMPTY;` |
|         - |  706 | `	}` |
|         - |  707 | `#endif` |
|   9353236 |  708 | `	pBlob->pBlob = 0;` |
|   9353236 |  709 | `	pBlob->mByte = pBlob->nByte	= 0;` |
|   9353236 |  710 | `	pBlob->pAllocator = &(*pAllocator);` |
|   9353236 |  711 | `	pBlob->nFlags = 0;` |
|   9353236 |  712 | `	return SXRET_OK;` |
|         5 |  713 |  |
|   3332246 |  714 | `PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte)` |
|         5 |  715 |  |
|         - |  716 | `#if defined(UNTRUST)` |
|         - |  717 | `	if( pBlob == 0  ){` |
|         - |  718 | `		return SXERR_EMPTY;` |
|         - |  719 | `	}` |
|         - |  720 | `#endif` |
|   3332251 |  721 | `	pBlob->pBlob = (void *)pData;` |
|   3332251 |  722 | `	pBlob->nByte = nByte;` |
|   3332251 |  723 | `	pBlob->mByte = 0;` |
|   3332251 |  724 | `	pBlob->nFlags \|= SXBLOB_RDONLY;` |
|   3332251 |  725 | `	return SXRET_OK;` |
|         5 |  726 |  |
|         - |  727 | `#ifndef SXBLOB_MIN_GROWTH` |
|         - |  728 | `#define SXBLOB_MIN_GROWTH 16` |
|         - |  729 | `#endif` |
|   9403498 |  730 | `static sxi32 BlobPrepareGrow(SyBlob *pBlob,sxu32 *pByte)` |
|         5 |  731 |  |
|         - |  732 | `	sxu32 nByte;` |
|         - |  733 | `	void *pNew;` |
|   9403503 |  734 | `	nByte = *pByte;` |
|   9403503 |  735 | `	if( pBlob->nFlags & (SXBLOB_LOCKED\|SXBLOB_STATIC) ){` |
|   2022275 |  736 | `		if ( SyBlobFreeSpace(pBlob) < nByte ){` |
|       ! 0 |  737 | `			*pByte = SyBlobFreeSpace(pBlob);` |
|       ! 0 |  738 | `			if( (*pByte) == 0 ){` |
|       ! 0 |  739 | `				return SXERR_SHORT;` |
|         - |  740 | `			}` |
|       ! 0 |  741 | `		}` |
|   2022275 |  742 | `		return SXRET_OK;` |
|         - |  743 | `	}` |
|   7381233 |  744 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|         - |  745 | `		/* Make a copy of the read-only item */` |
|    683885 |  746 | `		if( pBlob->nByte > 0 ){` |
|    683885 |  747 | `			pNew = SyMemBackendDup(pBlob->pAllocator,pBlob->pBlob,pBlob->nByte);` |
|    683885 |  748 | `			if( pNew == 0 ){` |
|       ! 0 |  749 | `				return SXERR_MEM;` |
|         - |  750 | `			}` |
|    683885 |  751 | `			pBlob->pBlob = pNew;` |
|    683885 |  752 | `			pBlob->mByte = pBlob->nByte;` |
|    341945 |  753 | `		}else{` |
|       ! 0 |  754 | `			pBlob->pBlob = 0;` |
|       ! 0 |  755 | `			pBlob->mByte = 0;` |
|         - |  756 | `		}` |
|         - |  757 | `		/* Remove the read-only flag */` |
|    683885 |  758 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|    341940 |  759 | `	}` |
|   7381233 |  760 | `	if( SyBlobFreeSpace(pBlob) >= nByte ){` |
|   1489496 |  761 | `		return SXRET_OK;` |
|         - |  762 | `	}` |
|   5891742 |  763 | `	if( pBlob->mByte > 0 ){` |
|    801421 |  764 | `		nByte = nByte + pBlob->mByte * 2 + SXBLOB_MIN_GROWTH;` |
|   5491031 |  765 | `	}else if ( nByte < SXBLOB_MIN_GROWTH ){` |
|   3991838 |  766 | `		nByte = SXBLOB_MIN_GROWTH;` |
|   1995823 |  767 | `	}` |
|   5891742 |  768 | `	pNew = SyMemBackendRealloc(pBlob->pAllocator,pBlob->pBlob,nByte);` |
|   5891742 |  769 | `	if( pNew == 0 ){` |
|       ! 0 |  770 | `		return SXERR_MEM;` |
|         - |  771 | `	}` |
|   5891742 |  772 | `	pBlob->pBlob = pNew;` |
|   5891742 |  773 | `	pBlob->mByte = nByte;` |
|   5891742 |  774 | `	return SXRET_OK;` |
|   4701839 |  775 |  |
|   9465952 |  776 | `PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize)` |
|         5 |  777 |  |
|         - |  778 | `	sxu8 *zBlob;` |
|         - |  779 | `	sxi32 rc;` |
|   9465957 |  780 | `	if( nSize < 1 ){` |
|     62459 |  781 | `		return SXRET_OK;` |
|         - |  782 | `	}` |
|   9403503 |  783 | `	rc = BlobPrepareGrow(&(*pBlob),&nSize);` |
|   9403503 |  784 | `	if( SXRET_OK != rc ){` |
|       ! 0 |  785 | `		return rc;` |
|         - |  786 | `	}` |
|   9403503 |  787 | `	if( pData ){` |
|   9403471 |  788 | `		zBlob = (sxu8 *)pBlob->pBlob ;` |
|   9403471 |  789 | `		zBlob = &zBlob[pBlob->nByte];` |
|   9403471 |  790 | `		pBlob->nByte += nSize;` |
|  42152078 |  791 | `		SX_MACRO_FAST_MEMCPY(pData,zBlob,nSize);` |
|   4701818 |  792 | `	}` |
|   9403503 |  793 | `	return SXRET_OK;` |
|   4733066 |  794 |  |
|    694251 |  795 | `PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob)` |
|         5 |  796 |  |
|         - |  797 | `	sxi32 rc;` |
|         - |  798 | `	sxu32 n;` |
|    694256 |  799 | `	n = pBlob->nByte;` |
|    694256 |  800 | `	rc = SyBlobAppend(&(*pBlob),(const void *)"\0",sizeof(char));` |
|    694256 |  801 | `	if (rc == SXRET_OK ){` |
|    694256 |  802 | `		pBlob->nByte = n;` |
|    347168 |  803 | `	}` |
|    694256 |  804 | `	return rc;` |
|         5 |  805 |  |
|   3848752 |  806 | `PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest)` |
|         5 |  807 |  |
|   3848757 |  808 | `	sxi32 rc = SXRET_OK;` |
|         - |  809 | `#ifdef UNTRUST` |
|         - |  810 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|         - |  811 | `		return SXERR_EMPTY;` |
|         - |  812 | `	}` |
|         - |  813 | `#endif` |
|   3848757 |  814 | `	if( pSrc->nByte > 0 ){` |
|   3821489 |  815 | `		rc = SyBlobAppend(&(*pDest),pSrc->pBlob,pSrc->nByte);` |
|   1910742 |  816 | `	}` |
|   3848757 |  817 | `	return rc;` |
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
|   4531312 |  838 | `PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob)` |
|         5 |  839 |  |
|   4531317 |  840 | `	pBlob->nByte = 0;` |
|   4531317 |  841 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|      4815 |  842 | `		pBlob->pBlob = 0;` |
|      4815 |  843 | `		pBlob->mByte = 0;` |
|      4815 |  844 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|      2405 |  845 | `	}` |
|   4531317 |  846 | `	return SXRET_OK;` |
|         5 |  847 |  |
|  12309245 |  848 | `PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob)` |
|         5 |  849 |  |
|  12309250 |  850 | `	if( (pBlob->nFlags & (SXBLOB_STATIC\|SXBLOB_RDONLY)) == 0 && pBlob->mByte > 0 ){` |
|   5283376 |  851 | `		SyMemBackendFree(pBlob->pAllocator,pBlob->pBlob);` |
|   2641728 |  852 | `	}` |
|  12309250 |  853 | `	pBlob->pBlob = 0;` |
|  12309250 |  854 | `	pBlob->nByte = pBlob->mByte = 0;` |
|  12309250 |  855 | `	pBlob->nFlags = 0;` |
|  12309250 |  856 | `	return SXRET_OK;` |
|         5 |  857 |  |
|         - |  858 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    163344 |  859 | `PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft)` |
|         5 |  860 |  |
|    163349 |  861 | `	const char *zIn = (const char *)pBlob;` |
|         - |  862 | `	const char *zEnd;` |
|         - |  863 | `	sxi32 rc;` |
|    163349 |  864 | `	if( pLen > nLen ){` |
|      6125 |  865 | `		return SXERR_NOTFOUND;` |
|         - |  866 | `	}` |
|    157229 |  867 | `	zEnd = &zIn[nLen-pLen];` |
|   1340435 |  868 | `	for(;;){` |
|   2680789 |  869 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   2641740 |  870 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   2585933 |  871 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|   2551326 |  872 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|         5 |  873 | `	}` |
|     23925 |  874 | `	return SXERR_NOTFOUND;` |
|     81677 |  875 |  |
|         - |  876 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - |  877 |  |
