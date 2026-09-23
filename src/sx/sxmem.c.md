# src/sx/sxmem.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 443/524 lines (84.54%)

[Root index](../../index.md) | [Directory index](index.md)

|       Hits | Line | Source |
| ---------: | ---: | :--- |
|          - |    1 | `/**` |
|          - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|          - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|          - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|          - |    5 | ` */` |
|          - |    6 | `#include "sxtypes.h"` |
|          - |    7 | `#include "sxmacros.h"` |
|          - |    8 | `#include "sxset.h"` |
|          - |    9 | `#include "sxmem.h"` |
|          - |   10 | `#include "sxmutex.h"` |
|          - |   11 | `#include "sxstr.h"` |
|          - |   12 | `#if defined(__WINNT__)` |
|          - |   13 | `#include <Windows.h>` |
|          - |   14 | `#else` |
|          - |   15 | `#include <stdlib.h>` |
|          - |   16 | `#endif` |
|          - |   17 |  |
|   76198215 |   18 | `static void * SyOSHeapAlloc(sxu32 nByte)` |
|          5 |   19 | `{` |
|          - |   20 | `	void *pNew;` |
|          - |   21 | `#if defined(__WINNT__)` |
|          5 |   22 | `	pNew = HeapAlloc(GetProcessHeap(),0,nByte);` |
|          - |   23 | `#else` |
|   76198215 |   24 | `	pNew = malloc((size_t)nByte);` |
|          - |   25 | `#endif` |
|   76198220 |   26 | `	return pNew;` |
|          5 |   27 | `}` |
|    5208845 |   28 | `static void * SyOSHeapRealloc(void *pOld,sxu32 nByte)` |
|          5 |   29 | `{` |
|          - |   30 | `	void *pNew;` |
|          - |   31 | `#if defined(__WINNT__)` |
|          5 |   32 | `	pNew = HeapReAlloc(GetProcessHeap(),0,pOld,nByte);` |
|          - |   33 | `#else` |
|    5208845 |   34 | `	pNew = realloc(pOld,(size_t)nByte);` |
|          - |   35 | `#endif` |
|    5208850 |   36 | `	return pNew;` |
|          5 |   37 | `}` |
|   76194139 |   38 | `static void SyOSHeapFree(void *pPtr)` |
|          5 |   39 | `{` |
|          - |   40 | `#if defined(__WINNT__)` |
|          5 |   41 | `	HeapFree(GetProcessHeap(),0,pPtr);` |
|          - |   42 | `#else` |
|   76194139 |   43 | `	free(pPtr);` |
|          - |   44 | `#endif` |
|   76194144 |   45 | `}` |
|          - |   46 |  |
|          - |   47 |  |
|  234924392 |   48 | `PH7_PRIVATE void SyZero(void *pSrc,sxu32 nSize)` |
|          5 |   49 | `{` |
|  234924397 |   50 | `	register unsigned char *zSrc = (unsigned char *)pSrc;` |
|          - |   51 | `	unsigned char *zEnd;` |
|          - |   52 | `#if defined(UNTRUST)` |
|          - |   53 | `	if( zSrc == 0 \|\| nSize <= 0 ){` |
|          - |   54 | `		return ;` |
|          - |   55 | `	}` |
|          - |   56 | `#endif` |
|  234924397 |   57 | `	zEnd = &zSrc[nSize];` |
| 3113372021 |   58 | `	for(;;){` |
| 6225536432 |   59 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 5990612735 |   60 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 5990612423 |   61 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 5990612129 |   62 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
|          5 |   63 | `	}` |
|  234924397 |   64 | `}` |
|   82657975 |   65 | `PH7_PRIVATE sxi32 SyMemcmp(const void *pB1,const void *pB2,sxu32 nSize)` |
|          5 |   66 | `{` |
|          - |   67 | `	sxi32 rc;` |
|   82657980 |   68 | `	if( nSize <= 0 ){` |
|      37181 |   69 | `		return 0;` |
|          - |   70 | `	}` |
|   82620804 |   71 | `	if( pB1 == 0 \|\| pB2 == 0 ){` |
|        ! 0 |   72 | `		return pB1 != 0 ? 1 : (pB2 == 0 ? 0 : -1);` |
|          - |   73 | `	}` |
|  128471333 |   74 | `	SX_MACRO_FAST_CMP(pB1,pB2,nSize,rc);` |
|   82620804 |   75 | `	return rc;` |
|   41339117 |   76 | `}` |
|   37522099 |   77 | `PH7_PRIVATE sxu32 SyMemcpy(const void *pSrc,void *pDest,sxu32 nLen)` |
|          5 |   78 | `{` |
|          - |   79 | `#if defined(UNTRUST)` |
|          - |   80 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|          - |   81 | `		return 0;` |
|          - |   82 | `	}` |
|          - |   83 | `#endif` |
|   37522104 |   84 | `	if( pSrc == (const void *)pDest ){` |
|          3 |   85 | `		return nLen;` |
|          - |   86 | `	}` |
|  320988959 |   87 | `	SX_MACRO_FAST_MEMCPY(pSrc,pDest,nLen);` |
|   37522102 |   88 | `	return nLen;` |
|   18773892 |   89 | `}` |
|          - |   90 | `/* Size prefix stored ahead of every OS allocation. Padded to pointer size so` |
|          - |   91 | ` * the returned payload (and the SyMemBlock/SyMemHeader the backend lays on` |
|          - |   92 | ` * top of it) keeps the allocator's natural alignment — a bare sxu32 prefix` |
|          - |   93 | ` * left every chunk 4-misaligned on 64-bit platforms. */` |
|          - |   94 | `typedef union MemOSHeader MemOSHeader;` |
|          - |   95 | `union MemOSHeader {` |
|          - |   96 | `	sxu32 nBytes;` |
|          - |   97 | `	void *pAlign;` |
|          - |   98 | `};` |
|   76198215 |   99 | `static void * MemOSAlloc(sxu32 nBytes)` |
|          5 |  100 | `{` |
|          - |  101 | `	MemOSHeader *pChunk;` |
|   76198220 |  102 | `	pChunk = (MemOSHeader *)SyOSHeapAlloc(nBytes + sizeof(MemOSHeader));` |
|   76198220 |  103 | `	if( pChunk == 0 ){` |
|        ! 0 |  104 | `		return 0;` |
|          - |  105 | `	}` |
|   76198220 |  106 | `	pChunk->nBytes = nBytes;` |
|   76198220 |  107 | `	return (void *)&pChunk[1];` |
|   38105249 |  108 | `}` |
|    5208845 |  109 | `static void * MemOSRealloc(void *pOld,sxu32 nBytes)` |
|          5 |  110 | `{` |
|          - |  111 | `	MemOSHeader *pOldChunk;` |
|          - |  112 | `	MemOSHeader *pChunk;` |
|    5208850 |  113 | `	pOldChunk = (MemOSHeader *)(((char *)pOld)-sizeof(MemOSHeader));` |
|    5208850 |  114 | `	if( pOldChunk->nBytes >= nBytes ){` |
|        ! 0 |  115 | `		return pOld;` |
|          - |  116 | `	}` |
|    5208850 |  117 | `	pChunk = (MemOSHeader *)SyOSHeapRealloc(pOldChunk,nBytes + sizeof(MemOSHeader));` |
|    5208850 |  118 | `	if( pChunk == 0 ){` |
|        ! 0 |  119 | `		return 0;` |
|          - |  120 | `	}` |
|    5208850 |  121 | `	pChunk->nBytes = nBytes;` |
|    5208850 |  122 | `	return (void *)&pChunk[1];` |
|    2607147 |  123 | `}` |
|   76194139 |  124 | `static void MemOSFree(void *pBlock)` |
|          5 |  125 | `{` |
|          - |  126 | `	void *pChunk;` |
|   76194144 |  127 | `	pChunk = (void *)(((char *)pBlock)-sizeof(MemOSHeader));` |
|   76194144 |  128 | `	SyOSHeapFree(pChunk);` |
|   76194144 |  129 | `}` |
|        ! 0 |  130 | `static sxu32 MemOSChunkSize(void *pBlock)` |
|        ! 0 |  131 | `{` |
|          - |  132 | `	MemOSHeader *pChunk;` |
|        ! 0 |  133 | `	pChunk = (MemOSHeader *)(((char *)pBlock)-sizeof(MemOSHeader));` |
|        ! 0 |  134 | `	return pChunk->nBytes;` |
|        ! 0 |  135 | `}` |
|          - |  136 | `/* Export OS allocation methods */` |
|          - |  137 | `static const SyMemMethods sOSAllocMethods = {` |
|          - |  138 | `	MemOSAlloc,` |
|          - |  139 | `	MemOSRealloc,` |
|          - |  140 | `	MemOSFree,` |
|          - |  141 | `	MemOSChunkSize,` |
|          - |  142 | `	0,` |
|          - |  143 | `	0,` |
|          - |  144 | `	0` |
|          - |  145 | `};` |
|   76198215 |  146 | `static void * MemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|          5 |  147 | `{` |
|          - |  148 | `	SyMemBlock *pBlock;` |
|   76198220 |  149 | `	sxi32 nRetry = 0;` |
|          - |  150 |  |
|          - |  151 | `	/* Append an extra block so we can tracks allocated chunks and avoid memory` |
|          - |  152 | `	 * leaks.` |
|          - |  153 | `	 */` |
|   76198220 |  154 | `	nByte += sizeof(SyMemBlock);` |
|          - |  155 | `	/* Enforce the optional per-allocation cap (0 = unlimited). A capped failure` |
|          - |  156 | `	 * returns NULL just like a genuine OS failure, driving the normal SXERR_MEM` |
|          - |  157 | `	 * propagation; the retry callback is intentionally skipped (hard limit). */` |
|   76198220 |  158 | `	if( pBackend->nMaxRequest && nByte > pBackend->nMaxRequest ){` |
|        ! 0 |  159 | `		return 0;` |
|          - |  160 | `	}` |
|   38105244 |  161 | `	for(;;){` |
|   38105249 |  162 | `		pBlock = (SyMemBlock *)pBackend->pMethods->xAlloc(nByte);` |
|   76198215 |  163 | `		if( pBlock != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY` |
|          5 |  164 | `			\|\| SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|   38105249 |  165 | `				break;` |
|          - |  166 | `		}` |
|        ! 0 |  167 | `		nRetry++;` |
|        ! 0 |  168 | `	}` |
|   76198220 |  169 | `	if( pBlock  == 0 ){` |
|        ! 0 |  170 | `		return 0;` |
|          - |  171 | `	}` |
|   76198220 |  172 | `	pBlock->pNext = pBlock->pPrev = 0;` |
|          - |  173 | `	/* Link to the list of already tracked blocks */` |
|   76198220 |  174 | `	MACRO_LD_PUSH(pBackend->pBlocks,pBlock);` |
|          - |  175 | `#if defined(UNTRUST)` |
|          - |  176 | `	pBlock->nGuard = SXMEM_BACKEND_MAGIC;` |
|          - |  177 | `#endif` |
|   76198220 |  178 | `	pBlock->nSize = nByte;` |
|   76198220 |  179 | `	pBackend->nMemUsed += nByte;` |
|   76198220 |  180 | `	if( pBackend->nMemUsed > pBackend->nMemPeak ){` |
|   26110903 |  181 | `		pBackend->nMemPeak = pBackend->nMemUsed;` |
|   13055400 |  182 | `	}` |
|   76198220 |  183 | `	pBackend->nBlock++;` |
|   76198220 |  184 | `	return (void *)&pBlock[1];` |
|   38105249 |  185 | `}` |
|   38185686 |  186 | `PH7_PRIVATE void * SyMemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|          5 |  187 | `{` |
|          - |  188 | `	void *pChunk;` |
|          - |  189 | `#if defined(UNTRUST)` |
|          - |  190 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  191 | `		return 0;` |
|          - |  192 | `	}` |
|          - |  193 | `#endif` |
|   38185691 |  194 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  195 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  196 | `	}` |
|   38185691 |  197 | `	pChunk = MemBackendAlloc(&(*pBackend),nByte);` |
|   38185691 |  198 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  199 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  200 | `	}` |
|   38185691 |  201 | `	return pChunk;` |
|          5 |  202 | `}` |
|   42953798 |  203 | `static void * MemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|          5 |  204 | `{` |
|          - |  205 | `	SyMemBlock *pBlock,*pNew,*pPrev,*pNext;` |
|   42953803 |  206 | `	sxu32 nRetry = 0;` |
|          - |  207 |  |
|   42953803 |  208 | `	if( pOld == 0 ){` |
|   37744958 |  209 | `		return MemBackendAlloc(&(*pBackend),nByte);` |
|          - |  210 | `	}` |
|    5208850 |  211 | `	pBlock = (SyMemBlock *)(((char *)pOld) - sizeof(SyMemBlock));` |
|          - |  212 | `#if defined(UNTRUST)` |
|          - |  213 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|          - |  214 | `		return 0;` |
|          - |  215 | `	}` |
|          - |  216 | `#endif` |
|    5208850 |  217 | `	nByte += sizeof(SyMemBlock);` |
|          - |  218 | `	/* Enforce the optional per-allocation cap (0 = unlimited); see MemBackendAlloc. */` |
|    5208850 |  219 | `	if( pBackend->nMaxRequest && nByte > pBackend->nMaxRequest ){` |
|        ! 0 |  220 | `		return 0;` |
|          - |  221 | `	}` |
|    5208850 |  222 | `	pPrev = pBlock->pPrev;` |
|    5208850 |  223 | `	pNext = pBlock->pNext;` |
|          - |  224 | `	{` |
|          - |  225 | `		/* Old size, captured before realloc may move/free the block; the` |
|          - |  226 | `		 * live-byte counter is adjusted by the delta only on success below. */` |
|    5208850 |  227 | `		sxu32 nOld = pBlock->nSize;` |
|    2607142 |  228 | `	for(;;){` |
|    2607147 |  229 | `		pNew = (SyMemBlock *)pBackend->pMethods->xRealloc(pBlock,nByte);` |
|    5208850 |  230 | `		if( pNew != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY \|\|` |
|        ! 0 |  231 | `			SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|    2607147 |  232 | `				break;` |
|          - |  233 | `		}` |
|        ! 0 |  234 | `		nRetry++;` |
|        ! 0 |  235 | `	}` |
|    5208850 |  236 | `	if( pNew == 0 ){` |
|        ! 0 |  237 | `		return 0;` |
|          - |  238 | `	}` |
|    5208850 |  239 | `	if( pNew != pBlock ){` |
|    5012175 |  240 | `		if( pPrev == 0 ){` |
|    4584735 |  241 | `			pBackend->pBlocks = pNew;` |
|    2366897 |  242 | `		}else{` |
|     427445 |  243 | `			pPrev->pNext = pNew;` |
|          - |  244 | `		}` |
|    5012175 |  245 | `		if( pNext ){` |
|    5012123 |  246 | `			pNext->pPrev = pNew;` |
|    2602416 |  247 | `		}` |
|          - |  248 | `#if defined(UNTRUST)` |
|          - |  249 | `		pNew->nGuard = SXMEM_BACKEND_MAGIC;` |
|          - |  250 | `#endif` |
|    2602443 |  251 | `	}` |
|          - |  252 | `	/* Apply the size delta to the live-byte counter (underflow-guarded). */` |
|    5208850 |  253 | `	pBackend->nMemUsed = (pBackend->nMemUsed >= nOld) ? (pBackend->nMemUsed - nOld) : 0;` |
|    5208850 |  254 | `	pBackend->nMemUsed += nByte;` |
|    5208850 |  255 | `	if( pBackend->nMemUsed > pBackend->nMemPeak ){` |
|     554835 |  256 | `		pBackend->nMemPeak = pBackend->nMemUsed;` |
|     277431 |  257 | `	}` |
|    5208850 |  258 | `	pNew->nSize = nByte;` |
|    5208850 |  259 | `	return (void *)&pNew[1];` |
|          - |  260 | `	}` |
|   21483678 |  261 | `}` |
|   42953798 |  262 | `PH7_PRIVATE void * SyMemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|          5 |  263 | `{` |
|          - |  264 | `	void *pChunk;` |
|          - |  265 | `#if defined(UNTRUST)` |
|          - |  266 | `	if( SXMEM_BACKEND_CORRUPT(pBackend)  ){` |
|          - |  267 | `		return 0;` |
|          - |  268 | `	}` |
|          - |  269 | `#endif` |
|   42953803 |  270 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  271 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  272 | `	}` |
|   42953803 |  273 | `	pChunk = MemBackendRealloc(&(*pBackend),pOld,nByte);` |
|   42953803 |  274 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  275 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  276 | `	}` |
|   42953803 |  277 | `	return pChunk;` |
|          5 |  278 | `}` |
|   52621465 |  279 | `static sxi32 MemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|          5 |  280 | `{` |
|          - |  281 | `	SyMemBlock *pBlock;` |
|   52621470 |  282 | `	pBlock = (SyMemBlock *)(((char *)pChunk) - sizeof(SyMemBlock));` |
|          - |  283 | `#if defined(UNTRUST)` |
|          - |  284 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|          - |  285 | `		return SXERR_CORRUPT;` |
|          - |  286 | `	}` |
|          - |  287 | `#endif` |
|          - |  288 | `	/* Unlink from the list of active blocks */` |
|   52621470 |  289 | `	if( pBackend->nBlock > 0 ){` |
|          - |  290 | `		/* Release the block */` |
|          - |  291 | `#if defined(UNTRUST)` |
|          - |  292 | `		/* Mark as stale block */` |
|          - |  293 | `		pBlock->nGuard = 0x635B;` |
|          - |  294 | `#endif` |
|   52621470 |  295 | `		MACRO_LD_REMOVE(pBackend->pBlocks,pBlock);` |
|   52621470 |  296 | `		pBackend->nBlock--;` |
|   78926066 |  297 | `		pBackend->nMemUsed = (pBackend->nMemUsed >= pBlock->nSize)` |
|   52621465 |  298 | `			? (pBackend->nMemUsed - pBlock->nSize) : 0;` |
|   52621470 |  299 | `		pBackend->pMethods->xFree(pBlock);` |
|   26316869 |  300 | `	}` |
|   52621470 |  301 | `	return SXRET_OK;` |
|          5 |  302 | `}` |
|   52621465 |  303 | `PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|          5 |  304 | `{` |
|          - |  305 | `	sxi32 rc;` |
|          - |  306 | `#if defined(UNTRUST)` |
|          - |  307 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  308 | `		return SXERR_CORRUPT;` |
|          - |  309 | `	}` |
|          - |  310 | `#endif` |
|   52621470 |  311 | `	if( pChunk == 0 ){` |
|        ! 0 |  312 | `		return SXRET_OK;` |
|          - |  313 | `	}` |
|   52621470 |  314 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  315 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  316 | `	}` |
|   52621470 |  317 | `	rc = MemBackendFree(&(*pBackend),pChunk);` |
|   52621470 |  318 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  319 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  320 | `	}` |
|   52621470 |  321 | `	return rc;` |
|   26316874 |  322 | `}` |
|          - |  323 | `#if defined(PH7_ENABLE_THREADS)` |
|       4672 |  324 | `PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods)` |
|          5 |  325 | `{` |
|          - |  326 | `	SyMutex *pMutex;` |
|          - |  327 | `#if defined(UNTRUST)` |
|          - |  328 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pMethods == 0 \|\| pMethods->xNew == 0){` |
|          - |  329 | `		return SXERR_CORRUPT;` |
|          - |  330 | `	}` |
|          - |  331 | `#endif` |
|       4677 |  332 | `	pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|       4677 |  333 | `	if( pMutex == 0 ){` |
|        ! 0 |  334 | `		return SXERR_OS;` |
|          - |  335 | `	}` |
|          - |  336 | `	/* Attach the mutex to the memory backend */` |
|       4677 |  337 | `	pBackend->pMutex = pMutex;` |
|       4677 |  338 | `	pBackend->pMutexMethods = pMethods;` |
|       4677 |  339 | `	return SXRET_OK;` |
|       2341 |  340 | `}` |
|       4672 |  341 | `PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend)` |
|          5 |  342 | `{` |
|          - |  343 | `#if defined(UNTRUST)` |
|          - |  344 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  345 | `		return SXERR_CORRUPT;` |
|          - |  346 | `	}` |
|          - |  347 | `#endif` |
|       4677 |  348 | `	if( pBackend->pMutex == 0 ){` |
|          - |  349 | `		/* There is no mutex subsystem at all */` |
|        ! 0 |  350 | `		return SXRET_OK;` |
|          - |  351 | `	}` |
|       4677 |  352 | `	SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|       4677 |  353 | `	pBackend->pMutexMethods = 0;` |
|       4677 |  354 | `	pBackend->pMutex = 0;` |
|       4677 |  355 | `	return SXRET_OK;` |
|       2341 |  356 | `}` |
|          - |  357 | `#endif` |
|          - |  358 | `/*` |
|          - |  359 | ` * Memory pool allocator` |
|          - |  360 | ` */` |
|          - |  361 | `#define SXMEM_POOL_MAGIC		0xDEAD` |
|          - |  362 | `#define SXMEM_POOL_MAXALLOC		(1<<(SXMEM_POOL_NBUCKETS+SXMEM_POOL_INCR))` |
|          - |  363 | `#define SXMEM_POOL_MINALLOC		(1<<(SXMEM_POOL_INCR))` |
|          - |  364 | `/* When SXMEM_POOL_BYPASS is defined (sanitizer builds) the bucket-recycling` |
|          - |  365 | ` * path is compiled but never taken — MemBackendPoolAlloc forces the big-block` |
|          - |  366 | ` * branch — so ASan sees one real allocation per request. A compile-time` |
|          - |  367 | ` * constant (not #ifdef scattered through the alloc body) keeps a single copy` |
|          - |  368 | ` * of the alloc/tag/free logic; production builds fold the constant to 0 and` |
|          - |  369 | ` * lose nothing. */` |
|          - |  370 | `#if defined(SXMEM_POOL_BYPASS)` |
|          - |  371 | `# define SXMEM_POOL_BYPASS_ACTIVE 1` |
|          - |  372 | `#else` |
|          - |  373 | `# define SXMEM_POOL_BYPASS_ACTIVE 0` |
|          - |  374 | `#endif` |
|     267576 |  375 | `static sxi32 MemPoolBucketAlloc(SyMemBackend *pBackend,sxu32 nBucket)` |
|          5 |  376 | `{` |
|          - |  377 | `	char *zBucket,*zBucketEnd;` |
|          - |  378 | `	SyMemHeader *pHeader;` |
|          - |  379 | `	sxu32 nBucketSize;` |
|          - |  380 |  |
|          - |  381 | `	/* Allocate one big block first */` |
|     267581 |  382 | `	zBucket = (char *)MemBackendAlloc(&(*pBackend),SXMEM_POOL_MAXALLOC);` |
|     267581 |  383 | `	if( zBucket == 0 ){` |
|        ! 0 |  384 | `		return SXERR_MEM;` |
|          - |  385 | `	}` |
|     267581 |  386 | `	zBucketEnd = &zBucket[SXMEM_POOL_MAXALLOC];` |
|          - |  387 | `	/* Divide the big block into mini bucket pool */` |
|     267581 |  388 | `	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);` |
|     267581 |  389 | `	pBackend->apPool[nBucket] = pHeader = (SyMemHeader *)zBucket;` |
|   23302364 |  390 | `	for(;;){` |
|   46604733 |  391 | `		if( &zBucket[nBucketSize] >= zBucketEnd ){` |
|     267581 |  392 | `			break;` |
|          - |  393 | `		}` |
|   46337157 |  394 | `		pHeader->pNext = (SyMemHeader *)&zBucket[nBucketSize];` |
|          - |  395 | `		/* Advance the cursor to the next available chunk */` |
|   46337157 |  396 | `		pHeader = pHeader->pNext;` |
|   46337157 |  397 | `		zBucket += nBucketSize;` |
|          5 |  398 | `	}` |
|     267581 |  399 | `	pHeader->pNext = 0;` |
|          - |  400 |  |
|     267581 |  401 | `	return SXRET_OK;` |
|     133793 |  402 | `}` |
|  113865576 |  403 | `static void * MemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|          5 |  404 | `{` |
|          - |  405 | `	SyMemHeader *pBucket,*pNext;` |
|          - |  406 | `	sxu32 nBucketSize;` |
|          - |  407 | `	sxu32 nBucket;` |
|          - |  408 |  |
|          - |  409 | `	/* SXMEM_POOL_BYPASS (sanitizer builds): force the big-block path for every` |
|          - |  410 | `	 * request so there is no bucket recycling and ASan tracks each object's` |
|          - |  411 | `	 * real lifetime. Chunks are freed through MemBackendPoolFree's big-block` |
|          - |  412 | `	 * branch either way — one copy of the alloc+tag logic. */` |
|  113865581 |  413 | `	if( SXMEM_POOL_BYPASS_ACTIVE \|\| nByte + sizeof(SyMemHeader) >= SXMEM_POOL_MAXALLOC ){` |
|          - |  414 | `		/* Allocate a big chunk directly */` |
|        ! 0 |  415 | `		pBucket = (SyMemHeader *)MemBackendAlloc(&(*pBackend),nByte+sizeof(SyMemHeader));` |
|        ! 0 |  416 | `		if( pBucket == 0 ){` |
|        ! 0 |  417 | `			return 0;` |
|          - |  418 | `		}` |
|          - |  419 | `		/* Record as big block */` |
|        ! 0 |  420 | `		pBucket->nBucket = ((sxu32)SXMEM_POOL_MAGIC << 16) \| SXU16_HIGH;` |
|        ! 0 |  421 | `		return (void *)(pBucket+1);` |
|          - |  422 | `	}` |
|          - |  423 | `	/* Locate the appropriate bucket */` |
|  113865581 |  424 | `	nBucket = 0;` |
|  113865581 |  425 | `	nBucketSize = SXMEM_POOL_MINALLOC;` |
|  588499237 |  426 | `	while( nByte + sizeof(SyMemHeader) > nBucketSize  ){` |
|  474633661 |  427 | `		nBucketSize <<= 1;` |
|  474633661 |  428 | `		nBucket++;` |
|          5 |  429 | `	}` |
|  113865581 |  430 | `	pBucket = pBackend->apPool[nBucket];` |
|  113865581 |  431 | `	if( pBucket == 0 ){` |
|          - |  432 | `		sxi32 rc;` |
|     267581 |  433 | `		rc = MemPoolBucketAlloc(&(*pBackend),nBucket);` |
|     267581 |  434 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  435 | `			return 0;` |
|          - |  436 | `		}` |
|     267581 |  437 | `		pBucket = pBackend->apPool[nBucket];` |
|     133788 |  438 | `	}` |
|          - |  439 | `	/* Remove from the free list */` |
|  113865581 |  440 | `	pNext = pBucket->pNext;` |
|  113865581 |  441 | `	pBackend->apPool[nBucket] = pNext;` |
|          - |  442 | `	/* Record bucket&magic number */` |
|  113865581 |  443 | `	pBucket->nBucket = (((sxu32)SXMEM_POOL_MAGIC << 16) \| nBucket);` |
|  113865581 |  444 | `	return (void *)&pBucket[1];` |
|   56935549 |  445 | `}` |
|  113865576 |  446 | `PH7_PRIVATE void * SyMemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|          5 |  447 | `{` |
|          - |  448 | `	void *pChunk;` |
|          - |  449 | `#if defined(UNTRUST)` |
|          - |  450 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  451 | `		return 0;` |
|          - |  452 | `	}` |
|          - |  453 | `#endif` |
|  113865581 |  454 | `	if( pBackend->pMutexMethods ){` |
|       4677 |  455 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       2336 |  456 | `	}` |
|  113865581 |  457 | `	pChunk = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|  113865581 |  458 | `	if( pBackend->pMutexMethods ){` |
|       4677 |  459 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       2336 |  460 | `	}` |
|  113865581 |  461 | `	return pChunk;` |
|          5 |  462 | `}` |
|   74969688 |  463 | `static sxi32 MemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|          5 |  464 | `{` |
|          - |  465 | `	SyMemHeader *pHeader;` |
|          - |  466 | `	sxu32 nBucket;` |
|          - |  467 | `	/* Get the corresponding bucket */` |
|   74969693 |  468 | `	pHeader = (SyMemHeader *)(((char *)pChunk) - sizeof(SyMemHeader));` |
|          - |  469 | `	/* Sanity check to avoid misuse */` |
|   74969693 |  470 | `	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){` |
|          3 |  471 | `		return SXERR_CORRUPT;` |
|          - |  472 | `	}` |
|   74969691 |  473 | `	nBucket = pHeader->nBucket & 0xFFFF;` |
|   74969691 |  474 | `	if( nBucket == SXU16_HIGH ){` |
|          - |  475 | `		/* Free the big block */` |
|        ! 0 |  476 | `		MemBackendFree(&(*pBackend),pHeader);` |
|   74969691 |  477 | `	}else if( nBucket >= SXMEM_POOL_NBUCKETS + SXMEM_POOL_INCR ){` |
|          - |  478 | `		/* Corrupted or misused bucket index */` |
|        ! 0 |  479 | `		return SXERR_CORRUPT;` |
|        ! 0 |  480 | `	}else{` |
|          - |  481 | `		/* Return to the free list */` |
|   74969691 |  482 | `		pHeader->pNext = pBackend->apPool[nBucket];` |
|   74969691 |  483 | `		pBackend->apPool[nBucket] = pHeader;` |
|          - |  484 | `	}` |
|   74969691 |  485 | `	return SXRET_OK;` |
|   37487605 |  486 | `}` |
|   74969688 |  487 | `PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|          5 |  488 | `{` |
|          - |  489 | `	sxi32 rc;` |
|          - |  490 | `#if defined(UNTRUST)` |
|          - |  491 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pChunk == 0 ){` |
|          - |  492 | `		return SXERR_CORRUPT;` |
|          - |  493 | `	}` |
|          - |  494 | `#endif` |
|   74969693 |  495 | `	if( pBackend->pMutexMethods ){` |
|       4087 |  496 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       2041 |  497 | `	}` |
|   74969693 |  498 | `	rc = MemBackendPoolFree(&(*pBackend),pChunk);` |
|   74969693 |  499 | `	if( pBackend->pMutexMethods ){` |
|       4087 |  500 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       2041 |  501 | `	}` |
|   74969693 |  502 | `	return rc;` |
|          5 |  503 | `}` |
|          - |  504 | `#if 0` |
|          - |  505 | `static void * MemBackendPoolRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|          - |  506 | `{` |
|          - |  507 | `	sxu32 nBucket,nBucketSize;` |
|          - |  508 | `	SyMemHeader *pHeader;` |
|          - |  509 | `	void * pNew;` |
|          - |  510 |  |
|          - |  511 | `	if( pOld == 0 ){` |
|          - |  512 | `		/* Allocate a new pool */` |
|          - |  513 | `		pNew = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|          - |  514 | `		return pNew;` |
|          - |  515 | `	}` |
|          - |  516 | `	/* Get the corresponding bucket */` |
|          - |  517 | `	pHeader = (SyMemHeader *)(((char *)pOld) - sizeof(SyMemHeader));` |
|          - |  518 | `	/* Sanity check to avoid misuse */` |
|          - |  519 | `	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){` |
|          - |  520 | `		return 0;` |
|          - |  521 | `	}` |
|          - |  522 | `	nBucket = pHeader->nBucket & 0xFFFF;` |
|          - |  523 | `	if( nBucket == SXU16_HIGH ){` |
|          - |  524 | `		/* Big block */` |
|          - |  525 | `		return MemBackendRealloc(&(*pBackend),pHeader,nByte);` |
|          - |  526 | `	}` |
|          - |  527 | `	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);` |
|          - |  528 | `	if( nBucketSize >= nByte + sizeof(SyMemHeader) ){` |
|          - |  529 | `		/* The old bucket can honor the requested size */` |
|          - |  530 | `		return pOld;` |
|          - |  531 | `	}` |
|          - |  532 | `	/* Allocate a new pool */` |
|          - |  533 | `	pNew = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|          - |  534 | `	if( pNew == 0 ){` |
|          - |  535 | `		return 0;` |
|          - |  536 | `	}` |
|          - |  537 | `	/* Copy the old data into the new block */` |
|          - |  538 | `	SyMemcpy(pOld,pNew,nBucketSize);` |
|          - |  539 | `	/* Free the stale block */` |
|          - |  540 | `	MemBackendPoolFree(&(*pBackend),pOld);` |
|          - |  541 | `	return pNew;` |
|          - |  542 | `}` |
|          - |  543 | `PH7_PRIVATE void * SyMemBackendPoolRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|          - |  544 | `{` |
|          - |  545 | `	void *pChunk;` |
|          - |  546 | `#if defined(UNTRUST)` |
|          - |  547 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  548 | `		return 0;` |
|          - |  549 | `	}` |
|          - |  550 | `#endif` |
|          - |  551 | `	if( pBackend->pMutexMethods ){` |
|          - |  552 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|          - |  553 | `	}` |
|          - |  554 | `	pChunk = MemBackendPoolRealloc(&(*pBackend),pOld,nByte);` |
|          - |  555 | `	if( pBackend->pMutexMethods ){` |
|          - |  556 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|          - |  557 | `	}` |
|          - |  558 | `	return pChunk;` |
|          - |  559 | `}` |
|          - |  560 | `#endif` |
|       4672 |  561 | `PH7_PRIVATE sxi32 SyMemBackendInit(SyMemBackend *pBackend,ProcMemError xMemErr,void * pUserData)` |
|          5 |  562 | `{` |
|          - |  563 | `#if defined(UNTRUST)` |
|          - |  564 | `	if( pBackend == 0 ){` |
|          - |  565 | `		return SXERR_EMPTY;` |
|          - |  566 | `	}` |
|          - |  567 | `#endif` |
|          - |  568 | `	/* Zero the allocator first */` |
|       4677 |  569 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|       4677 |  570 | `	pBackend->xMemError = xMemErr;` |
|       4677 |  571 | `	pBackend->pUserData = pUserData;` |
|          - |  572 | `	/* Switch to the OS memory allocator */` |
|       4677 |  573 | `	pBackend->pMethods = &sOSAllocMethods;` |
|       4677 |  574 | `	if( pBackend->pMethods->xInit ){` |
|          - |  575 | `		/* Initialize the backend  */` |
|        ! 0 |  576 | `		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){` |
|        ! 0 |  577 | `			return SXERR_ABORT;` |
|          - |  578 | `		}` |
|        ! 0 |  579 | `	}` |
|          - |  580 | `#if defined(UNTRUST)` |
|          - |  581 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|          - |  582 | `#endif` |
|       4677 |  583 | `	return SXRET_OK;` |
|       2341 |  584 | `}` |
|        ! 0 |  585 | `PH7_PRIVATE sxi32 SyMemBackendInitFromOthers(SyMemBackend *pBackend,const SyMemMethods *pMethods,ProcMemError xMemErr,void * pUserData)` |
|        ! 0 |  586 | `{` |
|          - |  587 | `#if defined(UNTRUST)` |
|          - |  588 | `	if( pBackend == 0 \|\| pMethods == 0){` |
|          - |  589 | `		return SXERR_EMPTY;` |
|          - |  590 | `	}` |
|          - |  591 | `#endif` |
|        ! 0 |  592 | `	if( pMethods->xAlloc == 0 \|\| pMethods->xRealloc == 0 \|\| pMethods->xFree == 0 \|\| pMethods->xChunkSize == 0 ){` |
|          - |  593 | `		/* mandatory methods are missing */` |
|        ! 0 |  594 | `		return SXERR_INVALID;` |
|          - |  595 | `	}` |
|          - |  596 | `	/* Zero the allocator first */` |
|        ! 0 |  597 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|        ! 0 |  598 | `	pBackend->xMemError = xMemErr;` |
|        ! 0 |  599 | `	pBackend->pUserData = pUserData;` |
|          - |  600 | `	/* Switch to the host application memory allocator */` |
|        ! 0 |  601 | `	pBackend->pMethods = pMethods;` |
|        ! 0 |  602 | `	if( pBackend->pMethods->xInit ){` |
|          - |  603 | `		/* Initialize the backend  */` |
|        ! 0 |  604 | `		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){` |
|        ! 0 |  605 | `			return SXERR_ABORT;` |
|          - |  606 | `		}` |
|        ! 0 |  607 | `	}` |
|          - |  608 | `#if defined(UNTRUST)` |
|          - |  609 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|          - |  610 | `#endif` |
|        ! 0 |  611 | `	return SXRET_OK;` |
|        ! 0 |  612 | `}` |
|       9342 |  613 | `PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend *pBackend,SyMemBackend *pParent)` |
|          5 |  614 | `{` |
|          - |  615 | `	sxu8 bInheritMutex;` |
|          - |  616 | `#if defined(UNTRUST)` |
|          - |  617 | `	if( pBackend == 0 \|\| SXMEM_BACKEND_CORRUPT(pParent) ){` |
|          - |  618 | `		return SXERR_CORRUPT;` |
|          - |  619 | `	}` |
|          - |  620 | `#endif` |
|          - |  621 | `	/* Zero the allocator first */` |
|       9347 |  622 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|       9347 |  623 | `	pBackend->pMethods  = pParent->pMethods;` |
|       9347 |  624 | `	pBackend->xMemError = pParent->xMemError;` |
|       9347 |  625 | `	pBackend->pUserData = pParent->pUserData;` |
|       9347 |  626 | `	pBackend->nMaxRequest = pParent->nMaxRequest;` |
|       9347 |  627 | `	bInheritMutex = pParent->pMutexMethods ? TRUE : FALSE;` |
|       9347 |  628 | `	if( bInheritMutex ){` |
|       4677 |  629 | `		pBackend->pMutexMethods = pParent->pMutexMethods;` |
|          - |  630 | `		/* Create a private mutex */` |
|       4677 |  631 | `		pBackend->pMutex = pBackend->pMutexMethods->xNew(SXMUTEX_TYPE_FAST);` |
|       4677 |  632 | `		if( pBackend->pMutex ==  0){` |
|        ! 0 |  633 | `			return SXERR_OS;` |
|          - |  634 | `		}` |
|       2336 |  635 | `	}` |
|          - |  636 | `#if defined(UNTRUST)` |
|          - |  637 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|          - |  638 | `#endif` |
|       9347 |  639 | `	return SXRET_OK;` |
|       4676 |  640 | `}` |
|       9944 |  641 | `static sxi32 MemBackendRelease(SyMemBackend *pBackend)` |
|          5 |  642 | `{` |
|          - |  643 | `	SyMemBlock *pBlock,*pNext;` |
|          - |  644 |  |
|       9949 |  645 | `	pBlock = pBackend->pBlocks;` |
|    2949681 |  646 | `	for(;;){` |
|    5899367 |  647 | `		if( pBackend->nBlock == 0 ){` |
|        780 |  648 | `			break;` |
|          - |  649 | `		}` |
|    5898591 |  650 | `		pNext  = pBlock->pNext;` |
|    5898591 |  651 | `		pBackend->pMethods->xFree(pBlock);` |
|    5898591 |  652 | `		pBlock = pNext;` |
|    5898591 |  653 | `		pBackend->nBlock--;` |
|          - |  654 | `		/* LOOP ONE */` |
|    5898591 |  655 | `		if( pBackend->nBlock == 0 ){` |
|       5763 |  656 | `			break;` |
|          - |  657 | `		}` |
|    5892833 |  658 | `		pNext  = pBlock->pNext;` |
|    5892833 |  659 | `		pBackend->pMethods->xFree(pBlock);` |
|    5892833 |  660 | `		pBlock = pNext;` |
|    5892833 |  661 | `		pBackend->nBlock--;` |
|          - |  662 | `		/* LOOP TWO */` |
|    5892833 |  663 | `		if( pBackend->nBlock == 0 ){` |
|        991 |  664 | `			break;` |
|          - |  665 | `		}` |
|    5891847 |  666 | `		pNext  = pBlock->pNext;` |
|    5891847 |  667 | `		pBackend->pMethods->xFree(pBlock);` |
|    5891847 |  668 | `		pBlock = pNext;` |
|    5891847 |  669 | `		pBackend->nBlock--;` |
|          - |  670 | `		/* LOOP THREE */` |
|    5891847 |  671 | `		if( pBackend->nBlock == 0 ){` |
|       2428 |  672 | `			break;` |
|          - |  673 | `		}` |
|    5889423 |  674 | `		pNext  = pBlock->pNext;` |
|    5889423 |  675 | `		pBackend->pMethods->xFree(pBlock);` |
|    5889423 |  676 | `		pBlock = pNext;` |
|    5889423 |  677 | `		pBackend->nBlock--;` |
|          - |  678 | `		/* LOOP FOUR */` |
|          5 |  679 | `	}` |
|       9949 |  680 | `	if( pBackend->pMethods->xRelease ){` |
|        ! 0 |  681 | `		pBackend->pMethods->xRelease(pBackend->pMethods->pUserData);` |
|        ! 0 |  682 | `	}` |
|       9949 |  683 | `	pBackend->pMethods = 0;` |
|       9949 |  684 | `	pBackend->pBlocks  = 0;` |
|          - |  685 | `#if defined(UNTRUST)` |
|          - |  686 | `	pBackend->nMagic = 0x2626;` |
|          - |  687 | `#endif` |
|       9949 |  688 | `	return SXRET_OK;` |
|          5 |  689 | `}` |
|       9944 |  690 | `PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend *pBackend)` |
|          5 |  691 | `{` |
|          - |  692 | `#if defined(UNTRUST)` |
|          - |  693 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  694 | `		return SXERR_INVALID;` |
|          - |  695 | `	}` |
|          - |  696 | `#endif` |
|       9949 |  697 | `	if( pBackend->pMutexMethods ){` |
|        600 |  698 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|        298 |  699 | `	}` |
|       9949 |  700 | `	(void)MemBackendRelease(&(*pBackend));` |
|       9949 |  701 | `	if( pBackend->pMutexMethods ){` |
|        600 |  702 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|        600 |  703 | `		SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|        298 |  704 | `	}` |
|       9949 |  705 | `	return SXRET_OK;` |
|          5 |  706 | `}` |
|    4045876 |  707 | `PH7_PRIVATE void * SyMemBackendDup(SyMemBackend *pBackend,const void *pSrc,sxu32 nSize)` |
|          5 |  708 | `{` |
|          - |  709 | `	void *pNew;` |
|          - |  710 | `#if defined(UNTRUST)` |
|          - |  711 | `	if( pSrc == 0 \|\| nSize <= 0 ){` |
|          - |  712 | `		return 0;` |
|          - |  713 | `	}` |
|          - |  714 | `#endif` |
|    4045881 |  715 | `	pNew = SyMemBackendAlloc(&(*pBackend),nSize);` |
|    4045881 |  716 | `	if( pNew ){` |
|    4045881 |  717 | `		SyMemcpy(pSrc,pNew,nSize);` |
|    2024587 |  718 | `	}` |
|    4045881 |  719 | `	return pNew;` |
|          5 |  720 | `}` |
|   10876114 |  721 | `PH7_PRIVATE char * SyMemBackendStrDup(SyMemBackend *pBackend,const char *zSrc,sxu32 nSize)` |
|          5 |  722 | `{` |
|          - |  723 | `	char *zDest;` |
|   10876119 |  724 | `	zDest = (char *)SyMemBackendAlloc(&(*pBackend),nSize + 1);` |
|   10876119 |  725 | `	if( zDest ){` |
|   10876119 |  726 | `		Systrcpy(zDest,nSize+1,zSrc,nSize);` |
|    5438057 |  727 | `	}` |
|   10876119 |  728 | `	return zDest;` |
|          5 |  729 | `}` |
|    7279026 |  730 | `PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize)` |
|          5 |  731 | `{` |
|          - |  732 | `#if defined(UNTRUST)` |
|          - |  733 | `	if( pBlob == 0 \|\| pBuffer == 0 \|\| nSize < 1 ){` |
|          - |  734 | `		return SXERR_EMPTY;` |
|          - |  735 | `	}` |
|          - |  736 | `#endif` |
|    7279031 |  737 | `	pBlob->pBlob = pBuffer;` |
|    7279031 |  738 | `	pBlob->mByte = nSize;` |
|    7279031 |  739 | `	pBlob->nByte = 0;` |
|    7279031 |  740 | `	pBlob->pAllocator = 0;` |
|    7279031 |  741 | `	pBlob->nFlags = SXBLOB_LOCKED\|SXBLOB_STATIC;` |
|    7279031 |  742 | `	return SXRET_OK;` |
|          5 |  743 | `}` |
|  125319555 |  744 | `PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator)` |
|          5 |  745 | `{` |
|          - |  746 | `#if defined(UNTRUST)` |
|          - |  747 | `	if( pBlob == 0  ){` |
|          - |  748 | `		return SXERR_EMPTY;` |
|          - |  749 | `	}` |
|          - |  750 | `#endif` |
|  125319560 |  751 | `	pBlob->pBlob = 0;` |
|  125319560 |  752 | `	pBlob->mByte = pBlob->nByte	= 0;` |
|  125319560 |  753 | `	pBlob->pAllocator = &(*pAllocator);` |
|  125319560 |  754 | `	pBlob->nFlags = 0;` |
|  125319560 |  755 | `	return SXRET_OK;` |
|          5 |  756 | `}` |
|   10268930 |  757 | `PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte)` |
|          5 |  758 | `{` |
|          - |  759 | `#if defined(UNTRUST)` |
|          - |  760 | `	if( pBlob == 0  ){` |
|          - |  761 | `		return SXERR_EMPTY;` |
|          - |  762 | `	}` |
|          - |  763 | `#endif` |
|   10268935 |  764 | `	pBlob->pBlob = (void *)pData;` |
|   10268935 |  765 | `	pBlob->nByte = nByte;` |
|   10268935 |  766 | `	pBlob->mByte = 0;` |
|   10268935 |  767 | `	pBlob->nFlags \|= SXBLOB_RDONLY;` |
|   10268935 |  768 | `	return SXRET_OK;` |
|          5 |  769 | `}` |
|          - |  770 | `#ifndef SXBLOB_MIN_GROWTH` |
|          - |  771 | `#define SXBLOB_MIN_GROWTH 16` |
|          - |  772 | `#endif` |
|   76638517 |  773 | `static sxi32 BlobPrepareGrow(SyBlob *pBlob,sxu32 *pByte)` |
|          5 |  774 | `{` |
|          - |  775 | `	sxu32 nByte;` |
|          - |  776 | `	void *pNew;` |
|   76638522 |  777 | `	nByte = *pByte;` |
|   76638522 |  778 | `	if( pBlob->nFlags & (SXBLOB_LOCKED\|SXBLOB_STATIC) ){` |
|   43776385 |  779 | `		if ( SyBlobFreeSpace(pBlob) < nByte ){` |
|        ! 0 |  780 | `			*pByte = SyBlobFreeSpace(pBlob);` |
|        ! 0 |  781 | `			if( (*pByte) == 0 ){` |
|        ! 0 |  782 | `				return SXERR_SHORT;` |
|          - |  783 | `			}` |
|        ! 0 |  784 | `		}` |
|   43776385 |  785 | `		return SXRET_OK;` |
|          - |  786 | `	}` |
|   32862142 |  787 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|          - |  788 | `		/* Make a copy of the read-only item */` |
|    4045801 |  789 | `		if( pBlob->nByte > 0 ){` |
|    4045801 |  790 | `			pNew = SyMemBackendDup(pBlob->pAllocator,pBlob->pBlob,pBlob->nByte);` |
|    4045801 |  791 | `			if( pNew == 0 ){` |
|        ! 0 |  792 | `				return SXERR_MEM;` |
|          - |  793 | `			}` |
|    4045801 |  794 | `			pBlob->pBlob = pNew;` |
|    4045801 |  795 | `			pBlob->mByte = pBlob->nByte;` |
|    2024552 |  796 | `		}else{` |
|        ! 0 |  797 | `			pBlob->pBlob = 0;` |
|        ! 0 |  798 | `			pBlob->mByte = 0;` |
|          - |  799 | `		}` |
|          - |  800 | `		/* Remove the read-only flag */` |
|    4045801 |  801 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|    2024547 |  802 | `	}` |
|   32862142 |  803 | `	if( SyBlobFreeSpace(pBlob) >= nByte ){` |
|    4818660 |  804 | `		return SXRET_OK;` |
|          - |  805 | `	}` |
|   28043487 |  806 | `	if( pBlob->mByte > 0 ){` |
|    4823572 |  807 | `		nByte = nByte + pBlob->mByte * 2 + SXBLOB_MIN_GROWTH;` |
|   25634423 |  808 | `	}else if ( nByte < SXBLOB_MIN_GROWTH ){` |
|   15231625 |  809 | `		nByte = SXBLOB_MIN_GROWTH;` |
|    7616974 |  810 | `	}` |
|   28043487 |  811 | `	pNew = SyMemBackendRealloc(pBlob->pAllocator,pBlob->pBlob,nByte);` |
|   28043487 |  812 | `	if( pNew == 0 ){` |
|        ! 0 |  813 | `		return SXERR_MEM;` |
|          - |  814 | `	}` |
|   28043487 |  815 | `	pBlob->pBlob = pNew;` |
|   28043487 |  816 | `	pBlob->mByte = nByte;` |
|   28043487 |  817 | `	return SXRET_OK;` |
|   38323471 |  818 | `}` |
|   81126221 |  819 | `PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize)` |
|          5 |  820 | `{` |
|          - |  821 | `	sxu8 *zBlob;` |
|          - |  822 | `	sxi32 rc;` |
|   81126226 |  823 | `	if( nSize < 1 ){` |
|    4487709 |  824 | `		return SXRET_OK;` |
|          - |  825 | `	}` |
|   76638522 |  826 | `	rc = BlobPrepareGrow(&(*pBlob),&nSize);` |
|   76638522 |  827 | `	if( SXRET_OK != rc ){` |
|        ! 0 |  828 | `		return rc;` |
|          - |  829 | `	}` |
|   76638522 |  830 | `	if( pData ){` |
|   76637792 |  831 | `		zBlob = (sxu8 *)pBlob->pBlob ;` |
|   76637792 |  832 | `		zBlob = &zBlob[pBlob->nByte];` |
|   76637792 |  833 | `		pBlob->nByte += nSize;` |
|  335479445 |  834 | `		SX_MACRO_FAST_MEMCPY(pData,zBlob,nSize);` |
|   38323101 |  835 | `	}` |
|   76638522 |  836 | `	return SXRET_OK;` |
|   40567324 |  837 | `}` |
|    4034341 |  838 | `PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob)` |
|          5 |  839 | `{` |
|          - |  840 | `	sxi32 rc;` |
|          - |  841 | `	sxu32 n;` |
|    4034346 |  842 | `	n = pBlob->nByte;` |
|    4034346 |  843 | `	rc = SyBlobAppend(&(*pBlob),(const void *)"\0",sizeof(char));` |
|    4034346 |  844 | `	if (rc == SXRET_OK ){` |
|    4034346 |  845 | `		pBlob->nByte = n;` |
|    2018987 |  846 | `	}` |
|    4034346 |  847 | `	return rc;` |
|          5 |  848 | `}` |
|    8730825 |  849 | `PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest)` |
|          5 |  850 | `{` |
|    8730830 |  851 | `	sxi32 rc = SXRET_OK;` |
|          - |  852 | `#ifdef UNTRUST` |
|          - |  853 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|          - |  854 | `		return SXERR_EMPTY;` |
|          - |  855 | `	}` |
|          - |  856 | `#endif` |
|    8730830 |  857 | `	if( pSrc->nByte > 0 ){` |
|    8730788 |  858 | `		rc = SyBlobAppend(&(*pDest),pSrc->pBlob,pSrc->nByte);` |
|    4366557 |  859 | `	}` |
|    8730830 |  860 | `	return rc;` |
|          5 |  861 | `}` |
|        ! 0 |  862 | `PH7_PRIVATE sxi32 SyBlobCmp(SyBlob *pLeft,SyBlob *pRight)` |
|        ! 0 |  863 | `{` |
|          - |  864 | `	sxi32 rc;` |
|          - |  865 | `#ifdef UNTRUST` |
|          - |  866 | `	if( pLeft == 0 \|\| pRight == 0 ){` |
|          - |  867 | `		return pLeft ? 1 : -1;` |
|          - |  868 | `	}` |
|          - |  869 | `#endif` |
|        ! 0 |  870 | `	if( pLeft->nByte != pRight->nByte ){` |
|          - |  871 | `		/* Length differ */` |
|        ! 0 |  872 | `		return pLeft->nByte - pRight->nByte;` |
|          - |  873 | `	}` |
|        ! 0 |  874 | `	if( pLeft->nByte == 0 ){` |
|        ! 0 |  875 | `		return 0;` |
|          - |  876 | `	}` |
|          - |  877 | `	/* Perform a standard memcmp() operation */` |
|        ! 0 |  878 | `	rc = SyMemcmp(pLeft->pBlob,pRight->pBlob,pLeft->nByte);` |
|        ! 0 |  879 | `	return rc;` |
|        ! 0 |  880 | `}` |
|   14919187 |  881 | `PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob)` |
|          5 |  882 | `{` |
|   14919192 |  883 | `	pBlob->nByte = 0;` |
|   14919192 |  884 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|       6209 |  885 | `		pBlob->pBlob = 0;` |
|       6209 |  886 | `		pBlob->mByte = 0;` |
|       6209 |  887 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|       3102 |  888 | `	}` |
|   14919192 |  889 | `	return SXRET_OK;` |
|          5 |  890 | `}` |
|   65595408 |  891 | `PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob)` |
|          5 |  892 | `{` |
|   65595413 |  893 | `	if( (pBlob->nFlags & (SXBLOB_STATIC\|SXBLOB_RDONLY)) == 0 && pBlob->mByte > 0 ){` |
|   26349898 |  894 | `		SyMemBackendFree(pBlob->pAllocator,pBlob->pBlob);` |
|   13178577 |  895 | `	}` |
|   65595413 |  896 | `	pBlob->pBlob = 0;` |
|   65595413 |  897 | `	pBlob->nByte = pBlob->mByte = 0;` |
|   65595413 |  898 | `	pBlob->nFlags = 0;` |
|   65595413 |  899 | `	return SXRET_OK;` |
|          5 |  900 | `}` |
|          - |  901 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     285816 |  902 | `PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft)` |
|          5 |  903 | `{` |
|     285821 |  904 | `	const char *zIn = (const char *)pBlob;` |
|          - |  905 | `	const char *zEnd;` |
|          - |  906 | `	sxi32 rc;` |
|     285821 |  907 | `	if( pLen > nLen ){` |
|       7851 |  908 | `		return SXERR_NOTFOUND;` |
|          - |  909 | `	}` |
|     277975 |  910 | `	zEnd = &zIn[nLen-pLen];` |
|    3091695 |  911 | `	for(;;){` |
|    6179614 |  912 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|    6111025 |  913 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|    6016908 |  914 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|    5957710 |  915 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|          5 |  916 | `	}` |
|      39869 |  917 | `	return SXERR_NOTFOUND;` |
|     142913 |  918 | `}` |
|          - |  919 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|          - |  920 |  |
