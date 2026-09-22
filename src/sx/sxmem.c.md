# src/sx/sxmem.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 442/524 lines (84.35%)

[Root index](../../index.md) | [Directory index](index.md)

|        Hits | Line | Source |
| ----------: | ---: | :--- |
|           - |    1 | `/**` |
|           - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|           - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|           - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|           - |    5 | ` */` |
|           - |    6 | `#include "sxtypes.h"` |
|           - |    7 | `#include "sxmacros.h"` |
|           - |    8 | `#include "sxset.h"` |
|           - |    9 | `#include "sxmem.h"` |
|           - |   10 | `#include "sxmutex.h"` |
|           - |   11 | `#include "sxstr.h"` |
|           - |   12 | `#if defined(__WINNT__)` |
|           - |   13 | `#include <Windows.h>` |
|           - |   14 | `#else` |
|           - |   15 | `#include <stdlib.h>` |
|           - |   16 | `#endif` |
|           - |   17 |  |
|   137513241 |   18 | `static void * SyOSHeapAlloc(sxu32 nByte)` |
|           5 |   19 | `{` |
|           - |   20 | `	void *pNew;` |
|           - |   21 | `#if defined(__WINNT__)` |
|           5 |   22 | `	pNew = HeapAlloc(GetProcessHeap(),0,nByte);` |
|           - |   23 | `#else` |
|   137513241 |   24 | `	pNew = malloc((size_t)nByte);` |
|           - |   25 | `#endif` |
|   137513246 |   26 | `	return pNew;` |
|           5 |   27 | `}` |
|     6134486 |   28 | `static void * SyOSHeapRealloc(void *pOld,sxu32 nByte)` |
|           5 |   29 | `{` |
|           - |   30 | `	void *pNew;` |
|           - |   31 | `#if defined(__WINNT__)` |
|           5 |   32 | `	pNew = HeapReAlloc(GetProcessHeap(),0,pOld,nByte);` |
|           - |   33 | `#else` |
|     6134486 |   34 | `	pNew = realloc(pOld,(size_t)nByte);` |
|           - |   35 | `#endif` |
|     6134491 |   36 | `	return pNew;` |
|           5 |   37 | `}` |
|   137509285 |   38 | `static void SyOSHeapFree(void *pPtr)` |
|           5 |   39 | `{` |
|           - |   40 | `#if defined(__WINNT__)` |
|           5 |   41 | `	HeapFree(GetProcessHeap(),0,pPtr);` |
|           - |   42 | `#else` |
|   137509285 |   43 | `	free(pPtr);` |
|           - |   44 | `#endif` |
|   137509290 |   45 | `}` |
|           - |   46 |  |
|           - |   47 |  |
|   499867180 |   48 | `PH7_PRIVATE void SyZero(void *pSrc,sxu32 nSize)` |
|           5 |   49 | `{` |
|   499867185 |   50 | `	register unsigned char *zSrc = (unsigned char *)pSrc;` |
|           - |   51 | `	unsigned char *zEnd;` |
|           - |   52 | `#if defined(UNTRUST)` |
|           - |   53 | `	if( zSrc == 0 \|\| nSize <= 0 ){` |
|           - |   54 | `		return ;` |
|           - |   55 | `	}` |
|           - |   56 | `#endif` |
|   499867185 |   57 | `	zEnd = &zSrc[nSize];` |
|  6397129974 |   58 | `	for(;;){` |
| 12793178765 |   59 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 12293312167 |   60 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 12293311905 |   61 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 12293311661 |   62 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
|           5 |   63 | `	}` |
|   499867185 |   64 | `}` |
|   472401883 |   65 | `PH7_PRIVATE sxi32 SyMemcmp(const void *pB1,const void *pB2,sxu32 nSize)` |
|           5 |   66 | `{` |
|           - |   67 | `	sxi32 rc;` |
|   472401888 |   68 | `	if( nSize <= 0 ){` |
|       36875 |   69 | `		return 0;` |
|           - |   70 | `	}` |
|   472365018 |   71 | `	if( pB1 == 0 \|\| pB2 == 0 ){` |
|         ! 0 |   72 | `		return pB1 != 0 ? 1 : (pB2 == 0 ? 0 : -1);` |
|           - |   73 | `	}` |
|   576135416 |   74 | `	SX_MACRO_FAST_CMP(pB1,pB2,nSize,rc);` |
|   472365018 |   75 | `	return rc;` |
|   236206912 |   76 | `}` |
|    69610348 |   77 | `PH7_PRIVATE sxu32 SyMemcpy(const void *pSrc,void *pDest,sxu32 nLen)` |
|           5 |   78 | `{` |
|           - |   79 | `#if defined(UNTRUST)` |
|           - |   80 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|           - |   81 | `		return 0;` |
|           - |   82 | `	}` |
|           - |   83 | `#endif` |
|    69610353 |   84 | `	if( pSrc == (const void *)pDest ){` |
|         ! 0 |   85 | `		return nLen;` |
|           - |   86 | `	}` |
|   585581589 |   87 | `	SX_MACRO_FAST_MEMCPY(pSrc,pDest,nLen);` |
|    69610353 |   88 | `	return nLen;` |
|    34814309 |   89 | `}` |
|           - |   90 | `/* Size prefix stored ahead of every OS allocation. Padded to pointer size so` |
|           - |   91 | ` * the returned payload (and the SyMemBlock/SyMemHeader the backend lays on` |
|           - |   92 | ` * top of it) keeps the allocator's natural alignment — a bare sxu32 prefix` |
|           - |   93 | ` * left every chunk 4-misaligned on 64-bit platforms. */` |
|           - |   94 | `typedef union MemOSHeader MemOSHeader;` |
|           - |   95 | `union MemOSHeader {` |
|           - |   96 | `	sxu32 nBytes;` |
|           - |   97 | `	void *pAlign;` |
|           - |   98 | `};` |
|   137513241 |   99 | `static void * MemOSAlloc(sxu32 nBytes)` |
|           5 |  100 | `{` |
|           - |  101 | `	MemOSHeader *pChunk;` |
|   137513246 |  102 | `	pChunk = (MemOSHeader *)SyOSHeapAlloc(nBytes + sizeof(MemOSHeader));` |
|   137513246 |  103 | `	if( pChunk == 0 ){` |
|         ! 0 |  104 | `		return 0;` |
|           - |  105 | `	}` |
|   137513246 |  106 | `	pChunk->nBytes = nBytes;` |
|   137513246 |  107 | `	return (void *)&pChunk[1];` |
|    68761205 |  108 | `}` |
|     6134486 |  109 | `static void * MemOSRealloc(void *pOld,sxu32 nBytes)` |
|           5 |  110 | `{` |
|           - |  111 | `	MemOSHeader *pOldChunk;` |
|           - |  112 | `	MemOSHeader *pChunk;` |
|     6134491 |  113 | `	pOldChunk = (MemOSHeader *)(((char *)pOld)-sizeof(MemOSHeader));` |
|     6134491 |  114 | `	if( pOldChunk->nBytes >= nBytes ){` |
|         ! 0 |  115 | `		return pOld;` |
|           - |  116 | `	}` |
|     6134491 |  117 | `	pChunk = (MemOSHeader *)SyOSHeapRealloc(pOldChunk,nBytes + sizeof(MemOSHeader));` |
|     6134491 |  118 | `	if( pChunk == 0 ){` |
|         ! 0 |  119 | `		return 0;` |
|           - |  120 | `	}` |
|     6134491 |  121 | `	pChunk->nBytes = nBytes;` |
|     6134491 |  122 | `	return (void *)&pChunk[1];` |
|     3068876 |  123 | `}` |
|   137509285 |  124 | `static void MemOSFree(void *pBlock)` |
|           5 |  125 | `{` |
|           - |  126 | `	void *pChunk;` |
|   137509290 |  127 | `	pChunk = (void *)(((char *)pBlock)-sizeof(MemOSHeader));` |
|   137509290 |  128 | `	SyOSHeapFree(pChunk);` |
|   137509290 |  129 | `}` |
|         ! 0 |  130 | `static sxu32 MemOSChunkSize(void *pBlock)` |
|         ! 0 |  131 | `{` |
|           - |  132 | `	MemOSHeader *pChunk;` |
|         ! 0 |  133 | `	pChunk = (MemOSHeader *)(((char *)pBlock)-sizeof(MemOSHeader));` |
|         ! 0 |  134 | `	return pChunk->nBytes;` |
|         ! 0 |  135 | `}` |
|           - |  136 | `/* Export OS allocation methods */` |
|           - |  137 | `static const SyMemMethods sOSAllocMethods = {` |
|           - |  138 | `	MemOSAlloc,` |
|           - |  139 | `	MemOSRealloc,` |
|           - |  140 | `	MemOSFree,` |
|           - |  141 | `	MemOSChunkSize,` |
|           - |  142 | `	0,` |
|           - |  143 | `	0,` |
|           - |  144 | `	0` |
|           - |  145 | `};` |
|   137513241 |  146 | `static void * MemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|           5 |  147 | `{` |
|           - |  148 | `	SyMemBlock *pBlock;` |
|   137513246 |  149 | `	sxi32 nRetry = 0;` |
|           - |  150 |  |
|           - |  151 | `	/* Append an extra block so we can tracks allocated chunks and avoid memory` |
|           - |  152 | `	 * leaks.` |
|           - |  153 | `	 */` |
|   137513246 |  154 | `	nByte += sizeof(SyMemBlock);` |
|           - |  155 | `	/* Enforce the optional per-allocation cap (0 = unlimited). A capped failure` |
|           - |  156 | `	 * returns NULL just like a genuine OS failure, driving the normal SXERR_MEM` |
|           - |  157 | `	 * propagation; the retry callback is intentionally skipped (hard limit). */` |
|   137513246 |  158 | `	if( pBackend->nMaxRequest && nByte > pBackend->nMaxRequest ){` |
|         ! 0 |  159 | `		return 0;` |
|           - |  160 | `	}` |
|    68761200 |  161 | `	for(;;){` |
|    68761205 |  162 | `		pBlock = (SyMemBlock *)pBackend->pMethods->xAlloc(nByte);` |
|   137513241 |  163 | `		if( pBlock != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY` |
|           5 |  164 | `			\|\| SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|    68761205 |  165 | `				break;` |
|           - |  166 | `		}` |
|         ! 0 |  167 | `		nRetry++;` |
|         ! 0 |  168 | `	}` |
|   137513246 |  169 | `	if( pBlock  == 0 ){` |
|         ! 0 |  170 | `		return 0;` |
|           - |  171 | `	}` |
|   137513246 |  172 | `	pBlock->pNext = pBlock->pPrev = 0;` |
|           - |  173 | `	/* Link to the list of already tracked blocks */` |
|   137513246 |  174 | `	MACRO_LD_PUSH(pBackend->pBlocks,pBlock);` |
|           - |  175 | `#if defined(UNTRUST)` |
|           - |  176 | `	pBlock->nGuard = SXMEM_BACKEND_MAGIC;` |
|           - |  177 | `#endif` |
|   137513246 |  178 | `	pBlock->nSize = nByte;` |
|   137513246 |  179 | `	pBackend->nMemUsed += nByte;` |
|   137513246 |  180 | `	if( pBackend->nMemUsed > pBackend->nMemPeak ){` |
|    38246805 |  181 | `		pBackend->nMemPeak = pBackend->nMemUsed;` |
|    19123364 |  182 | `	}` |
|   137513246 |  183 | `	pBackend->nBlock++;` |
|   137513246 |  184 | `	return (void *)&pBlock[1];` |
|    68761205 |  185 | `}` |
|    64177377 |  186 | `PH7_PRIVATE void * SyMemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|           5 |  187 | `{` |
|           - |  188 | `	void *pChunk;` |
|           - |  189 | `#if defined(UNTRUST)` |
|           - |  190 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|           - |  191 | `		return 0;` |
|           - |  192 | `	}` |
|           - |  193 | `#endif` |
|    64177382 |  194 | `	if( pBackend->pMutexMethods ){` |
|         ! 0 |  195 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|         ! 0 |  196 | `	}` |
|    64177382 |  197 | `	pChunk = MemBackendAlloc(&(*pBackend),nByte);` |
|    64177382 |  198 | `	if( pBackend->pMutexMethods ){` |
|         ! 0 |  199 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|         ! 0 |  200 | `	}` |
|    64177382 |  201 | `	return pChunk;` |
|           5 |  202 | `}` |
|    79157654 |  203 | `static void * MemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|           5 |  204 | `{` |
|           - |  205 | `	SyMemBlock *pBlock,*pNew,*pPrev,*pNext;` |
|    79157659 |  206 | `	sxu32 nRetry = 0;` |
|           - |  207 |  |
|    79157659 |  208 | `	if( pOld == 0 ){` |
|    73023173 |  209 | `		return MemBackendAlloc(&(*pBackend),nByte);` |
|           - |  210 | `	}` |
|     6134491 |  211 | `	pBlock = (SyMemBlock *)(((char *)pOld) - sizeof(SyMemBlock));` |
|           - |  212 | `#if defined(UNTRUST)` |
|           - |  213 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|           - |  214 | `		return 0;` |
|           - |  215 | `	}` |
|           - |  216 | `#endif` |
|     6134491 |  217 | `	nByte += sizeof(SyMemBlock);` |
|           - |  218 | `	/* Enforce the optional per-allocation cap (0 = unlimited); see MemBackendAlloc. */` |
|     6134491 |  219 | `	if( pBackend->nMaxRequest && nByte > pBackend->nMaxRequest ){` |
|         ! 0 |  220 | `		return 0;` |
|           - |  221 | `	}` |
|     6134491 |  222 | `	pPrev = pBlock->pPrev;` |
|     6134491 |  223 | `	pNext = pBlock->pNext;` |
|           - |  224 | `	{` |
|           - |  225 | `		/* Old size, captured before realloc may move/free the block; the` |
|           - |  226 | `		 * live-byte counter is adjusted by the delta only on success below. */` |
|     6134491 |  227 | `		sxu32 nOld = pBlock->nSize;` |
|     3068871 |  228 | `	for(;;){` |
|     3068876 |  229 | `		pNew = (SyMemBlock *)pBackend->pMethods->xRealloc(pBlock,nByte);` |
|     6134491 |  230 | `		if( pNew != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY \|\|` |
|         ! 0 |  231 | `			SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|     3068876 |  232 | `				break;` |
|           - |  233 | `		}` |
|         ! 0 |  234 | `		nRetry++;` |
|         ! 0 |  235 | `	}` |
|     6134491 |  236 | `	if( pNew == 0 ){` |
|         ! 0 |  237 | `		return 0;` |
|           - |  238 | `	}` |
|     6134491 |  239 | `	if( pNew != pBlock ){` |
|     5392093 |  240 | `		if( pPrev == 0 ){` |
|     2422948 |  241 | `			pBackend->pBlocks = pNew;` |
|     1297749 |  242 | `		}else{` |
|     2969150 |  243 | `			pPrev->pNext = pNew;` |
|           - |  244 | `		}` |
|     5392093 |  245 | `		if( pNext ){` |
|     5392067 |  246 | `			pNext->pPrev = pNew;` |
|     3043934 |  247 | `		}` |
|           - |  248 | `#if defined(UNTRUST)` |
|           - |  249 | `		pNew->nGuard = SXMEM_BACKEND_MAGIC;` |
|           - |  250 | `#endif` |
|     3043948 |  251 | `	}` |
|           - |  252 | `	/* Apply the size delta to the live-byte counter (underflow-guarded). */` |
|     6134491 |  253 | `	pBackend->nMemUsed = (pBackend->nMemUsed >= nOld) ? (pBackend->nMemUsed - nOld) : 0;` |
|     6134491 |  254 | `	pBackend->nMemUsed += nByte;` |
|     6134491 |  255 | `	if( pBackend->nMemUsed > pBackend->nMemPeak ){` |
|     2868171 |  256 | `		pBackend->nMemPeak = pBackend->nMemUsed;` |
|     1434098 |  257 | `	}` |
|     6134491 |  258 | `	pNew->nSize = nByte;` |
|     6134491 |  259 | `	return (void *)&pNew[1];` |
|           - |  260 | `	}` |
|    39584043 |  261 | `}` |
|    79157654 |  262 | `PH7_PRIVATE void * SyMemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|           5 |  263 | `{` |
|           - |  264 | `	void *pChunk;` |
|           - |  265 | `#if defined(UNTRUST)` |
|           - |  266 | `	if( SXMEM_BACKEND_CORRUPT(pBackend)  ){` |
|           - |  267 | `		return 0;` |
|           - |  268 | `	}` |
|           - |  269 | `#endif` |
|    79157659 |  270 | `	if( pBackend->pMutexMethods ){` |
|         ! 0 |  271 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|         ! 0 |  272 | `	}` |
|    79157659 |  273 | `	pChunk = MemBackendRealloc(&(*pBackend),pOld,nByte);` |
|    79157659 |  274 | `	if( pBackend->pMutexMethods ){` |
|         ! 0 |  275 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|         ! 0 |  276 | `	}` |
|    79157659 |  277 | `	return pChunk;` |
|           5 |  278 | `}` |
|   103063087 |  279 | `static sxi32 MemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|           5 |  280 | `{` |
|           - |  281 | `	SyMemBlock *pBlock;` |
|   103063092 |  282 | `	pBlock = (SyMemBlock *)(((char *)pChunk) - sizeof(SyMemBlock));` |
|           - |  283 | `#if defined(UNTRUST)` |
|           - |  284 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|           - |  285 | `		return SXERR_CORRUPT;` |
|           - |  286 | `	}` |
|           - |  287 | `#endif` |
|           - |  288 | `	/* Unlink from the list of active blocks */` |
|   103063092 |  289 | `	if( pBackend->nBlock > 0 ){` |
|           - |  290 | `		/* Release the block */` |
|           - |  291 | `#if defined(UNTRUST)` |
|           - |  292 | `		/* Mark as stale block */` |
|           - |  293 | `		pBlock->nGuard = 0x635B;` |
|           - |  294 | `#endif` |
|   103063092 |  295 | `		MACRO_LD_REMOVE(pBackend->pBlocks,pBlock);` |
|   103063092 |  296 | `		pBackend->nBlock--;` |
|   154590056 |  297 | `		pBackend->nMemUsed = (pBackend->nMemUsed >= pBlock->nSize)` |
|   103063087 |  298 | `			? (pBackend->nMemUsed - pBlock->nSize) : 0;` |
|   103063092 |  299 | `		pBackend->pMethods->xFree(pBlock);` |
|    51536123 |  300 | `	}` |
|   103063092 |  301 | `	return SXRET_OK;` |
|           5 |  302 | `}` |
|   103063087 |  303 | `PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|           5 |  304 | `{` |
|           - |  305 | `	sxi32 rc;` |
|           - |  306 | `#if defined(UNTRUST)` |
|           - |  307 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|           - |  308 | `		return SXERR_CORRUPT;` |
|           - |  309 | `	}` |
|           - |  310 | `#endif` |
|   103063092 |  311 | `	if( pChunk == 0 ){` |
|         ! 0 |  312 | `		return SXRET_OK;` |
|           - |  313 | `	}` |
|   103063092 |  314 | `	if( pBackend->pMutexMethods ){` |
|         ! 0 |  315 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|         ! 0 |  316 | `	}` |
|   103063092 |  317 | `	rc = MemBackendFree(&(*pBackend),pChunk);` |
|   103063092 |  318 | `	if( pBackend->pMutexMethods ){` |
|         ! 0 |  319 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|         ! 0 |  320 | `	}` |
|   103063092 |  321 | `	return rc;` |
|    51536128 |  322 | `}` |
|           - |  323 | `#if defined(PH7_ENABLE_THREADS)` |
|        4530 |  324 | `PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods)` |
|           5 |  325 | `{` |
|           - |  326 | `	SyMutex *pMutex;` |
|           - |  327 | `#if defined(UNTRUST)` |
|           - |  328 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pMethods == 0 \|\| pMethods->xNew == 0){` |
|           - |  329 | `		return SXERR_CORRUPT;` |
|           - |  330 | `	}` |
|           - |  331 | `#endif` |
|        4535 |  332 | `	pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|        4535 |  333 | `	if( pMutex == 0 ){` |
|         ! 0 |  334 | `		return SXERR_OS;` |
|           - |  335 | `	}` |
|           - |  336 | `	/* Attach the mutex to the memory backend */` |
|        4535 |  337 | `	pBackend->pMutex = pMutex;` |
|        4535 |  338 | `	pBackend->pMutexMethods = pMethods;` |
|        4535 |  339 | `	return SXRET_OK;` |
|        2270 |  340 | `}` |
|        4530 |  341 | `PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend)` |
|           5 |  342 | `{` |
|           - |  343 | `#if defined(UNTRUST)` |
|           - |  344 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|           - |  345 | `		return SXERR_CORRUPT;` |
|           - |  346 | `	}` |
|           - |  347 | `#endif` |
|        4535 |  348 | `	if( pBackend->pMutex == 0 ){` |
|           - |  349 | `		/* There is no mutex subsystem at all */` |
|         ! 0 |  350 | `		return SXRET_OK;` |
|           - |  351 | `	}` |
|        4535 |  352 | `	SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|        4535 |  353 | `	pBackend->pMutexMethods = 0;` |
|        4535 |  354 | `	pBackend->pMutex = 0;` |
|        4535 |  355 | `	return SXRET_OK;` |
|        2270 |  356 | `}` |
|           - |  357 | `#endif` |
|           - |  358 | `/*` |
|           - |  359 | ` * Memory pool allocator` |
|           - |  360 | ` */` |
|           - |  361 | `#define SXMEM_POOL_MAGIC		0xDEAD` |
|           - |  362 | `#define SXMEM_POOL_MAXALLOC		(1<<(SXMEM_POOL_NBUCKETS+SXMEM_POOL_INCR))` |
|           - |  363 | `#define SXMEM_POOL_MINALLOC		(1<<(SXMEM_POOL_INCR))` |
|           - |  364 | `/* When SXMEM_POOL_BYPASS is defined (sanitizer builds) the bucket-recycling` |
|           - |  365 | ` * path is compiled but never taken — MemBackendPoolAlloc forces the big-block` |
|           - |  366 | ` * branch — so ASan sees one real allocation per request. A compile-time` |
|           - |  367 | ` * constant (not #ifdef scattered through the alloc body) keeps a single copy` |
|           - |  368 | ` * of the alloc/tag/free logic; production builds fold the constant to 0 and` |
|           - |  369 | ` * lose nothing. */` |
|           - |  370 | `#if defined(SXMEM_POOL_BYPASS)` |
|           - |  371 | `# define SXMEM_POOL_BYPASS_ACTIVE 1` |
|           - |  372 | `#else` |
|           - |  373 | `# define SXMEM_POOL_BYPASS_ACTIVE 0` |
|           - |  374 | `#endif` |
|      312696 |  375 | `static sxi32 MemPoolBucketAlloc(SyMemBackend *pBackend,sxu32 nBucket)` |
|           5 |  376 | `{` |
|           - |  377 | `	char *zBucket,*zBucketEnd;` |
|           - |  378 | `	SyMemHeader *pHeader;` |
|           - |  379 | `	sxu32 nBucketSize;` |
|           - |  380 |  |
|           - |  381 | `	/* Allocate one big block first */` |
|      312701 |  382 | `	zBucket = (char *)MemBackendAlloc(&(*pBackend),SXMEM_POOL_MAXALLOC);` |
|      312701 |  383 | `	if( zBucket == 0 ){` |
|         ! 0 |  384 | `		return SXERR_MEM;` |
|           - |  385 | `	}` |
|      312701 |  386 | `	zBucketEnd = &zBucket[SXMEM_POOL_MAXALLOC];` |
|           - |  387 | `	/* Divide the big block into mini bucket pool */` |
|      312701 |  388 | `	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);` |
|      312701 |  389 | `	pBackend->apPool[nBucket] = pHeader = (SyMemHeader *)zBucket;` |
|    31149792 |  390 | `	for(;;){` |
|    62299589 |  391 | `		if( &zBucket[nBucketSize] >= zBucketEnd ){` |
|      312701 |  392 | `			break;` |
|           - |  393 | `		}` |
|    61986893 |  394 | `		pHeader->pNext = (SyMemHeader *)&zBucket[nBucketSize];` |
|           - |  395 | `		/* Advance the cursor to the next available chunk */` |
|    61986893 |  396 | `		pHeader = pHeader->pNext;` |
|    61986893 |  397 | `		zBucket += nBucketSize;` |
|           5 |  398 | `	}` |
|      312701 |  399 | `	pHeader->pNext = 0;` |
|           - |  400 |  |
|      312701 |  401 | `	return SXRET_OK;` |
|      156353 |  402 | `}` |
|   226941320 |  403 | `static void * MemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|           5 |  404 | `{` |
|           - |  405 | `	SyMemHeader *pBucket,*pNext;` |
|           - |  406 | `	sxu32 nBucketSize;` |
|           - |  407 | `	sxu32 nBucket;` |
|           - |  408 |  |
|           - |  409 | `	/* SXMEM_POOL_BYPASS (sanitizer builds): force the big-block path for every` |
|           - |  410 | `	 * request so there is no bucket recycling and ASan tracks each object's` |
|           - |  411 | `	 * real lifetime. Chunks are freed through MemBackendPoolFree's big-block` |
|           - |  412 | `	 * branch either way — one copy of the alloc+tag logic. */` |
|   226941325 |  413 | `	if( SXMEM_POOL_BYPASS_ACTIVE \|\| nByte + sizeof(SyMemHeader) >= SXMEM_POOL_MAXALLOC ){` |
|           - |  414 | `		/* Allocate a big chunk directly */` |
|         ! 0 |  415 | `		pBucket = (SyMemHeader *)MemBackendAlloc(&(*pBackend),nByte+sizeof(SyMemHeader));` |
|         ! 0 |  416 | `		if( pBucket == 0 ){` |
|         ! 0 |  417 | `			return 0;` |
|           - |  418 | `		}` |
|           - |  419 | `		/* Record as big block */` |
|         ! 0 |  420 | `		pBucket->nBucket = ((sxu32)SXMEM_POOL_MAGIC << 16) \| SXU16_HIGH;` |
|         ! 0 |  421 | `		return (void *)(pBucket+1);` |
|           - |  422 | `	}` |
|           - |  423 | `	/* Locate the appropriate bucket */` |
|   226941325 |  424 | `	nBucket = 0;` |
|   226941325 |  425 | `	nBucketSize = SXMEM_POOL_MINALLOC;` |
|  1154896509 |  426 | `	while( nByte + sizeof(SyMemHeader) > nBucketSize  ){` |
|   927955189 |  427 | `		nBucketSize <<= 1;` |
|   927955189 |  428 | `		nBucket++;` |
|           5 |  429 | `	}` |
|   226941325 |  430 | `	pBucket = pBackend->apPool[nBucket];` |
|   226941325 |  431 | `	if( pBucket == 0 ){` |
|           - |  432 | `		sxi32 rc;` |
|      312701 |  433 | `		rc = MemPoolBucketAlloc(&(*pBackend),nBucket);` |
|      312701 |  434 | `		if( rc != SXRET_OK ){` |
|         ! 0 |  435 | `			return 0;` |
|           - |  436 | `		}` |
|      312701 |  437 | `		pBucket = pBackend->apPool[nBucket];` |
|      156348 |  438 | `	}` |
|           - |  439 | `	/* Remove from the free list */` |
|   226941325 |  440 | `	pNext = pBucket->pNext;` |
|   226941325 |  441 | `	pBackend->apPool[nBucket] = pNext;` |
|           - |  442 | `	/* Record bucket&magic number */` |
|   226941325 |  443 | `	pBucket->nBucket = (((sxu32)SXMEM_POOL_MAGIC << 16) \| nBucket);` |
|   226941325 |  444 | `	return (void *)&pBucket[1];` |
|   113473239 |  445 | `}` |
|   226941320 |  446 | `PH7_PRIVATE void * SyMemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|           5 |  447 | `{` |
|           - |  448 | `	void *pChunk;` |
|           - |  449 | `#if defined(UNTRUST)` |
|           - |  450 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|           - |  451 | `		return 0;` |
|           - |  452 | `	}` |
|           - |  453 | `#endif` |
|   226941325 |  454 | `	if( pBackend->pMutexMethods ){` |
|        4535 |  455 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|        2265 |  456 | `	}` |
|   226941325 |  457 | `	pChunk = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|   226941325 |  458 | `	if( pBackend->pMutexMethods ){` |
|        4535 |  459 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|        2265 |  460 | `	}` |
|   226941325 |  461 | `	return pChunk;` |
|           5 |  462 | `}` |
|   173591454 |  463 | `static sxi32 MemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|           5 |  464 | `{` |
|           - |  465 | `	SyMemHeader *pHeader;` |
|           - |  466 | `	sxu32 nBucket;` |
|           - |  467 | `	/* Get the corresponding bucket */` |
|   173591459 |  468 | `	pHeader = (SyMemHeader *)(((char *)pChunk) - sizeof(SyMemHeader));` |
|           - |  469 | `	/* Sanity check to avoid misuse */` |
|   173591459 |  470 | `	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){` |
|           3 |  471 | `		return SXERR_CORRUPT;` |
|           - |  472 | `	}` |
|   173591457 |  473 | `	nBucket = pHeader->nBucket & 0xFFFF;` |
|   173591457 |  474 | `	if( nBucket == SXU16_HIGH ){` |
|           - |  475 | `		/* Free the big block */` |
|         ! 0 |  476 | `		MemBackendFree(&(*pBackend),pHeader);` |
|   173591457 |  477 | `	}else if( nBucket >= SXMEM_POOL_NBUCKETS + SXMEM_POOL_INCR ){` |
|           - |  478 | `		/* Corrupted or misused bucket index */` |
|         ! 0 |  479 | `		return SXERR_CORRUPT;` |
|         ! 0 |  480 | `	}else{` |
|           - |  481 | `		/* Return to the free list */` |
|   173591457 |  482 | `		pHeader->pNext = pBackend->apPool[nBucket];` |
|   173591457 |  483 | `		pBackend->apPool[nBucket] = pHeader;` |
|           - |  484 | `	}` |
|   173591457 |  485 | `	return SXRET_OK;` |
|    86798306 |  486 | `}` |
|   173591454 |  487 | `PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|           5 |  488 | `{` |
|           - |  489 | `	sxi32 rc;` |
|           - |  490 | `#if defined(UNTRUST)` |
|           - |  491 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pChunk == 0 ){` |
|           - |  492 | `		return SXERR_CORRUPT;` |
|           - |  493 | `	}` |
|           - |  494 | `#endif` |
|   173591459 |  495 | `	if( pBackend->pMutexMethods ){` |
|        3967 |  496 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|        1981 |  497 | `	}` |
|   173591459 |  498 | `	rc = MemBackendPoolFree(&(*pBackend),pChunk);` |
|   173591459 |  499 | `	if( pBackend->pMutexMethods ){` |
|        3967 |  500 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|        1981 |  501 | `	}` |
|   173591459 |  502 | `	return rc;` |
|           5 |  503 | `}` |
|           - |  504 | `#if 0` |
|           - |  505 | `static void * MemBackendPoolRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|           - |  506 | `{` |
|           - |  507 | `	sxu32 nBucket,nBucketSize;` |
|           - |  508 | `	SyMemHeader *pHeader;` |
|           - |  509 | `	void * pNew;` |
|           - |  510 |  |
|           - |  511 | `	if( pOld == 0 ){` |
|           - |  512 | `		/* Allocate a new pool */` |
|           - |  513 | `		pNew = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|           - |  514 | `		return pNew;` |
|           - |  515 | `	}` |
|           - |  516 | `	/* Get the corresponding bucket */` |
|           - |  517 | `	pHeader = (SyMemHeader *)(((char *)pOld) - sizeof(SyMemHeader));` |
|           - |  518 | `	/* Sanity check to avoid misuse */` |
|           - |  519 | `	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){` |
|           - |  520 | `		return 0;` |
|           - |  521 | `	}` |
|           - |  522 | `	nBucket = pHeader->nBucket & 0xFFFF;` |
|           - |  523 | `	if( nBucket == SXU16_HIGH ){` |
|           - |  524 | `		/* Big block */` |
|           - |  525 | `		return MemBackendRealloc(&(*pBackend),pHeader,nByte);` |
|           - |  526 | `	}` |
|           - |  527 | `	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);` |
|           - |  528 | `	if( nBucketSize >= nByte + sizeof(SyMemHeader) ){` |
|           - |  529 | `		/* The old bucket can honor the requested size */` |
|           - |  530 | `		return pOld;` |
|           - |  531 | `	}` |
|           - |  532 | `	/* Allocate a new pool */` |
|           - |  533 | `	pNew = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|           - |  534 | `	if( pNew == 0 ){` |
|           - |  535 | `		return 0;` |
|           - |  536 | `	}` |
|           - |  537 | `	/* Copy the old data into the new block */` |
|           - |  538 | `	SyMemcpy(pOld,pNew,nBucketSize);` |
|           - |  539 | `	/* Free the stale block */` |
|           - |  540 | `	MemBackendPoolFree(&(*pBackend),pOld);` |
|           - |  541 | `	return pNew;` |
|           - |  542 | `}` |
|           - |  543 | `PH7_PRIVATE void * SyMemBackendPoolRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|           - |  544 | `{` |
|           - |  545 | `	void *pChunk;` |
|           - |  546 | `#if defined(UNTRUST)` |
|           - |  547 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|           - |  548 | `		return 0;` |
|           - |  549 | `	}` |
|           - |  550 | `#endif` |
|           - |  551 | `	if( pBackend->pMutexMethods ){` |
|           - |  552 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|           - |  553 | `	}` |
|           - |  554 | `	pChunk = MemBackendPoolRealloc(&(*pBackend),pOld,nByte);` |
|           - |  555 | `	if( pBackend->pMutexMethods ){` |
|           - |  556 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|           - |  557 | `	}` |
|           - |  558 | `	return pChunk;` |
|           - |  559 | `}` |
|           - |  560 | `#endif` |
|        4530 |  561 | `PH7_PRIVATE sxi32 SyMemBackendInit(SyMemBackend *pBackend,ProcMemError xMemErr,void * pUserData)` |
|           5 |  562 | `{` |
|           - |  563 | `#if defined(UNTRUST)` |
|           - |  564 | `	if( pBackend == 0 ){` |
|           - |  565 | `		return SXERR_EMPTY;` |
|           - |  566 | `	}` |
|           - |  567 | `#endif` |
|           - |  568 | `	/* Zero the allocator first */` |
|        4535 |  569 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|        4535 |  570 | `	pBackend->xMemError = xMemErr;` |
|        4535 |  571 | `	pBackend->pUserData = pUserData;` |
|           - |  572 | `	/* Switch to the OS memory allocator */` |
|        4535 |  573 | `	pBackend->pMethods = &sOSAllocMethods;` |
|        4535 |  574 | `	if( pBackend->pMethods->xInit ){` |
|           - |  575 | `		/* Initialize the backend  */` |
|         ! 0 |  576 | `		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){` |
|         ! 0 |  577 | `			return SXERR_ABORT;` |
|           - |  578 | `		}` |
|         ! 0 |  579 | `	}` |
|           - |  580 | `#if defined(UNTRUST)` |
|           - |  581 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|           - |  582 | `#endif` |
|        4535 |  583 | `	return SXRET_OK;` |
|        2270 |  584 | `}` |
|         ! 0 |  585 | `PH7_PRIVATE sxi32 SyMemBackendInitFromOthers(SyMemBackend *pBackend,const SyMemMethods *pMethods,ProcMemError xMemErr,void * pUserData)` |
|         ! 0 |  586 | `{` |
|           - |  587 | `#if defined(UNTRUST)` |
|           - |  588 | `	if( pBackend == 0 \|\| pMethods == 0){` |
|           - |  589 | `		return SXERR_EMPTY;` |
|           - |  590 | `	}` |
|           - |  591 | `#endif` |
|         ! 0 |  592 | `	if( pMethods->xAlloc == 0 \|\| pMethods->xRealloc == 0 \|\| pMethods->xFree == 0 \|\| pMethods->xChunkSize == 0 ){` |
|           - |  593 | `		/* mandatory methods are missing */` |
|         ! 0 |  594 | `		return SXERR_INVALID;` |
|           - |  595 | `	}` |
|           - |  596 | `	/* Zero the allocator first */` |
|         ! 0 |  597 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|         ! 0 |  598 | `	pBackend->xMemError = xMemErr;` |
|         ! 0 |  599 | `	pBackend->pUserData = pUserData;` |
|           - |  600 | `	/* Switch to the host application memory allocator */` |
|         ! 0 |  601 | `	pBackend->pMethods = pMethods;` |
|         ! 0 |  602 | `	if( pBackend->pMethods->xInit ){` |
|           - |  603 | `		/* Initialize the backend  */` |
|         ! 0 |  604 | `		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){` |
|         ! 0 |  605 | `			return SXERR_ABORT;` |
|           - |  606 | `		}` |
|         ! 0 |  607 | `	}` |
|           - |  608 | `#if defined(UNTRUST)` |
|           - |  609 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|           - |  610 | `#endif` |
|         ! 0 |  611 | `	return SXRET_OK;` |
|         ! 0 |  612 | `}` |
|        9058 |  613 | `PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend *pBackend,SyMemBackend *pParent)` |
|           5 |  614 | `{` |
|           - |  615 | `	sxu8 bInheritMutex;` |
|           - |  616 | `#if defined(UNTRUST)` |
|           - |  617 | `	if( pBackend == 0 \|\| SXMEM_BACKEND_CORRUPT(pParent) ){` |
|           - |  618 | `		return SXERR_CORRUPT;` |
|           - |  619 | `	}` |
|           - |  620 | `#endif` |
|           - |  621 | `	/* Zero the allocator first */` |
|        9063 |  622 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|        9063 |  623 | `	pBackend->pMethods  = pParent->pMethods;` |
|        9063 |  624 | `	pBackend->xMemError = pParent->xMemError;` |
|        9063 |  625 | `	pBackend->pUserData = pParent->pUserData;` |
|        9063 |  626 | `	pBackend->nMaxRequest = pParent->nMaxRequest;` |
|        9063 |  627 | `	bInheritMutex = pParent->pMutexMethods ? TRUE : FALSE;` |
|        9063 |  628 | `	if( bInheritMutex ){` |
|        4535 |  629 | `		pBackend->pMutexMethods = pParent->pMutexMethods;` |
|           - |  630 | `		/* Create a private mutex */` |
|        4535 |  631 | `		pBackend->pMutex = pBackend->pMutexMethods->xNew(SXMUTEX_TYPE_FAST);` |
|        4535 |  632 | `		if( pBackend->pMutex ==  0){` |
|         ! 0 |  633 | `			return SXERR_OS;` |
|           - |  634 | `		}` |
|        2265 |  635 | `	}` |
|           - |  636 | `#if defined(UNTRUST)` |
|           - |  637 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|           - |  638 | `#endif` |
|        9063 |  639 | `	return SXRET_OK;` |
|        4534 |  640 | `}` |
|        9638 |  641 | `static sxi32 MemBackendRelease(SyMemBackend *pBackend)` |
|           5 |  642 | `{` |
|           - |  643 | `	SyMemBlock *pBlock,*pNext;` |
|           - |  644 |  |
|        9643 |  645 | `	pBlock = pBackend->pBlocks;` |
|     4309110 |  646 | `	for(;;){` |
|     8618225 |  647 | `		if( pBackend->nBlock == 0 ){` |
|         583 |  648 | `			break;` |
|           - |  649 | `		}` |
|     8617647 |  650 | `		pNext  = pBlock->pNext;` |
|     8617647 |  651 | `		pBackend->pMethods->xFree(pBlock);` |
|     8617647 |  652 | `		pBlock = pNext;` |
|     8617647 |  653 | `		pBackend->nBlock--;` |
|           - |  654 | `		/* LOOP ONE */` |
|     8617647 |  655 | `		if( pBackend->nBlock == 0 ){` |
|        7283 |  656 | `			break;` |
|           - |  657 | `		}` |
|     8610369 |  658 | `		pNext  = pBlock->pNext;` |
|     8610369 |  659 | `		pBackend->pMethods->xFree(pBlock);` |
|     8610369 |  660 | `		pBlock = pNext;` |
|     8610369 |  661 | `		pBackend->nBlock--;` |
|           - |  662 | `		/* LOOP TWO */` |
|     8610369 |  663 | `		if( pBackend->nBlock == 0 ){` |
|         758 |  664 | `			break;` |
|           - |  665 | `		}` |
|     8609615 |  666 | `		pNext  = pBlock->pNext;` |
|     8609615 |  667 | `		pBackend->pMethods->xFree(pBlock);` |
|     8609615 |  668 | `		pBlock = pNext;` |
|     8609615 |  669 | `		pBackend->nBlock--;` |
|           - |  670 | `		/* LOOP THREE */` |
|     8609615 |  671 | `		if( pBackend->nBlock == 0 ){` |
|        1032 |  672 | `			break;` |
|           - |  673 | `		}` |
|     8608587 |  674 | `		pNext  = pBlock->pNext;` |
|     8608587 |  675 | `		pBackend->pMethods->xFree(pBlock);` |
|     8608587 |  676 | `		pBlock = pNext;` |
|     8608587 |  677 | `		pBackend->nBlock--;` |
|           - |  678 | `		/* LOOP FOUR */` |
|           5 |  679 | `	}` |
|        9643 |  680 | `	if( pBackend->pMethods->xRelease ){` |
|         ! 0 |  681 | `		pBackend->pMethods->xRelease(pBackend->pMethods->pUserData);` |
|         ! 0 |  682 | `	}` |
|        9643 |  683 | `	pBackend->pMethods = 0;` |
|        9643 |  684 | `	pBackend->pBlocks  = 0;` |
|           - |  685 | `#if defined(UNTRUST)` |
|           - |  686 | `	pBackend->nMagic = 0x2626;` |
|           - |  687 | `#endif` |
|        9643 |  688 | `	return SXRET_OK;` |
|           5 |  689 | `}` |
|        9638 |  690 | `PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend *pBackend)` |
|           5 |  691 | `{` |
|           - |  692 | `#if defined(UNTRUST)` |
|           - |  693 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|           - |  694 | `		return SXERR_INVALID;` |
|           - |  695 | `	}` |
|           - |  696 | `#endif` |
|        9643 |  697 | `	if( pBackend->pMutexMethods ){` |
|         578 |  698 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|         287 |  699 | `	}` |
|        9643 |  700 | `	(void)MemBackendRelease(&(*pBackend));` |
|        9643 |  701 | `	if( pBackend->pMutexMethods ){` |
|         578 |  702 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|         578 |  703 | `		SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|         287 |  704 | `	}` |
|        9643 |  705 | `	return SXRET_OK;` |
|           5 |  706 | `}` |
|     1132238 |  707 | `PH7_PRIVATE void * SyMemBackendDup(SyMemBackend *pBackend,const void *pSrc,sxu32 nSize)` |
|           5 |  708 | `{` |
|           - |  709 | `	void *pNew;` |
|           - |  710 | `#if defined(UNTRUST)` |
|           - |  711 | `	if( pSrc == 0 \|\| nSize <= 0 ){` |
|           - |  712 | `		return 0;` |
|           - |  713 | `	}` |
|           - |  714 | `#endif` |
|     1132243 |  715 | `	pNew = SyMemBackendAlloc(&(*pBackend),nSize);` |
|     1132243 |  716 | `	if( pNew ){` |
|     1132243 |  717 | `		SyMemcpy(pSrc,pNew,nSize);` |
|      566713 |  718 | `	}` |
|     1132243 |  719 | `	return pNew;` |
|           5 |  720 | `}` |
|    13302764 |  721 | `PH7_PRIVATE char * SyMemBackendStrDup(SyMemBackend *pBackend,const char *zSrc,sxu32 nSize)` |
|           5 |  722 | `{` |
|           - |  723 | `	char *zDest;` |
|    13302769 |  724 | `	zDest = (char *)SyMemBackendAlloc(&(*pBackend),nSize + 1);` |
|    13302769 |  725 | `	if( zDest ){` |
|    13302769 |  726 | `		Systrcpy(zDest,nSize+1,zSrc,nSize);` |
|     6651382 |  727 | `	}` |
|    13302769 |  728 | `	return zDest;` |
|           5 |  729 | `}` |
|     3595288 |  730 | `PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize)` |
|           5 |  731 | `{` |
|           - |  732 | `#if defined(UNTRUST)` |
|           - |  733 | `	if( pBlob == 0 \|\| pBuffer == 0 \|\| nSize < 1 ){` |
|           - |  734 | `		return SXERR_EMPTY;` |
|           - |  735 | `	}` |
|           - |  736 | `#endif` |
|     3595293 |  737 | `	pBlob->pBlob = pBuffer;` |
|     3595293 |  738 | `	pBlob->mByte = nSize;` |
|     3595293 |  739 | `	pBlob->nByte = 0;` |
|     3595293 |  740 | `	pBlob->pAllocator = 0;` |
|     3595293 |  741 | `	pBlob->nFlags = SXBLOB_LOCKED\|SXBLOB_STATIC;` |
|     3595293 |  742 | `	return SXRET_OK;` |
|           5 |  743 | `}` |
|   279640840 |  744 | `PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator)` |
|           5 |  745 | `{` |
|           - |  746 | `#if defined(UNTRUST)` |
|           - |  747 | `	if( pBlob == 0  ){` |
|           - |  748 | `		return SXERR_EMPTY;` |
|           - |  749 | `	}` |
|           - |  750 | `#endif` |
|   279640845 |  751 | `	pBlob->pBlob = 0;` |
|   279640845 |  752 | `	pBlob->mByte = pBlob->nByte	= 0;` |
|   279640845 |  753 | `	pBlob->pAllocator = &(*pAllocator);` |
|   279640845 |  754 | `	pBlob->nFlags = 0;` |
|   279640845 |  755 | `	return SXRET_OK;` |
|           5 |  756 | `}` |
|    18523379 |  757 | `PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte)` |
|           5 |  758 | `{` |
|           - |  759 | `#if defined(UNTRUST)` |
|           - |  760 | `	if( pBlob == 0  ){` |
|           - |  761 | `		return SXERR_EMPTY;` |
|           - |  762 | `	}` |
|           - |  763 | `#endif` |
|    18523384 |  764 | `	pBlob->pBlob = (void *)pData;` |
|    18523384 |  765 | `	pBlob->nByte = nByte;` |
|    18523384 |  766 | `	pBlob->mByte = 0;` |
|    18523384 |  767 | `	pBlob->nFlags \|= SXBLOB_RDONLY;` |
|    18523384 |  768 | `	return SXRET_OK;` |
|           5 |  769 | `}` |
|           - |  770 | `#ifndef SXBLOB_MIN_GROWTH` |
|           - |  771 | `#define SXBLOB_MIN_GROWTH 16` |
|           - |  772 | `#endif` |
|    66041383 |  773 | `static sxi32 BlobPrepareGrow(SyBlob *pBlob,sxu32 *pByte)` |
|           5 |  774 | `{` |
|           - |  775 | `	sxu32 nByte;` |
|           - |  776 | `	void *pNew;` |
|    66041388 |  777 | `	nByte = *pByte;` |
|    66041388 |  778 | `	if( pBlob->nFlags & (SXBLOB_LOCKED\|SXBLOB_STATIC) ){` |
|    27943257 |  779 | `		if ( SyBlobFreeSpace(pBlob) < nByte ){` |
|         ! 0 |  780 | `			*pByte = SyBlobFreeSpace(pBlob);` |
|         ! 0 |  781 | `			if( (*pByte) == 0 ){` |
|         ! 0 |  782 | `				return SXERR_SHORT;` |
|           - |  783 | `			}` |
|         ! 0 |  784 | `		}` |
|    27943257 |  785 | `		return SXRET_OK;` |
|           - |  786 | `	}` |
|    38098136 |  787 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|           - |  788 | `		/* Make a copy of the read-only item */` |
|     1132163 |  789 | `		if( pBlob->nByte > 0 ){` |
|     1132163 |  790 | `			pNew = SyMemBackendDup(pBlob->pAllocator,pBlob->pBlob,pBlob->nByte);` |
|     1132163 |  791 | `			if( pNew == 0 ){` |
|         ! 0 |  792 | `				return SXERR_MEM;` |
|           - |  793 | `			}` |
|     1132163 |  794 | `			pBlob->pBlob = pNew;` |
|     1132163 |  795 | `			pBlob->mByte = pBlob->nByte;` |
|      566678 |  796 | `		}else{` |
|         ! 0 |  797 | `			pBlob->pBlob = 0;` |
|         ! 0 |  798 | `			pBlob->mByte = 0;` |
|           - |  799 | `		}` |
|           - |  800 | `		/* Remove the read-only flag */` |
|     1132163 |  801 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|      566673 |  802 | `	}` |
|    38098136 |  803 | `	if( SyBlobFreeSpace(pBlob) >= nByte ){` |
|     4695585 |  804 | `		return SXRET_OK;` |
|           - |  805 | `	}` |
|    33402556 |  806 | `	if( pBlob->mByte > 0 ){` |
|     1912661 |  807 | `		nByte = nByte + pBlob->mByte * 2 + SXBLOB_MIN_GROWTH;` |
|    32447856 |  808 | `	}else if ( nByte < SXBLOB_MIN_GROWTH ){` |
|    21259145 |  809 | `		nByte = SXBLOB_MIN_GROWTH;` |
|    10630483 |  810 | `	}` |
|    33402556 |  811 | `	pNew = SyMemBackendRealloc(pBlob->pAllocator,pBlob->pBlob,nByte);` |
|    33402556 |  812 | `	if( pNew == 0 ){` |
|         ! 0 |  813 | `		return SXERR_MEM;` |
|           - |  814 | `	}` |
|    33402556 |  815 | `	pBlob->pBlob = pNew;` |
|    33402556 |  816 | `	pBlob->mByte = nByte;` |
|    33402556 |  817 | `	return SXRET_OK;` |
|    33023287 |  818 | `}` |
|    66150051 |  819 | `PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize)` |
|           5 |  820 | `{` |
|           - |  821 | `	sxu8 *zBlob;` |
|           - |  822 | `	sxi32 rc;` |
|    66150056 |  823 | `	if( nSize < 1 ){` |
|      108673 |  824 | `		return SXRET_OK;` |
|           - |  825 | `	}` |
|    66041388 |  826 | `	rc = BlobPrepareGrow(&(*pBlob),&nSize);` |
|    66041388 |  827 | `	if( SXRET_OK != rc ){` |
|         ! 0 |  828 | `		return rc;` |
|           - |  829 | `	}` |
|    66041388 |  830 | `	if( pData ){` |
|    66041226 |  831 | `		zBlob = (sxu8 *)pBlob->pBlob ;` |
|    66041226 |  832 | `		zBlob = &zBlob[pBlob->nByte];` |
|    66041226 |  833 | `		pBlob->nByte += nSize;` |
|   316202959 |  834 | `		SX_MACRO_FAST_MEMCPY(pData,zBlob,nSize);` |
|    33023201 |  835 | `	}` |
|    66041388 |  836 | `	return SXRET_OK;` |
|    33077621 |  837 | `}` |
|     1110045 |  838 | `PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob)` |
|           5 |  839 | `{` |
|           - |  840 | `	sxi32 rc;` |
|           - |  841 | `	sxu32 n;` |
|     1110050 |  842 | `	n = pBlob->nByte;` |
|     1110050 |  843 | `	rc = SyBlobAppend(&(*pBlob),(const void *)"\0",sizeof(char));` |
|     1110050 |  844 | `	if (rc == SXRET_OK ){` |
|     1110050 |  845 | `		pBlob->nByte = n;` |
|      555767 |  846 | `	}` |
|     1110050 |  847 | `	return rc;` |
|           5 |  848 | `}` |
|     9859087 |  849 | `PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest)` |
|           5 |  850 | `{` |
|     9859092 |  851 | `	sxi32 rc = SXRET_OK;` |
|           - |  852 | `#ifdef UNTRUST` |
|           - |  853 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|           - |  854 | `		return SXERR_EMPTY;` |
|           - |  855 | `	}` |
|           - |  856 | `#endif` |
|     9859092 |  857 | `	if( pSrc->nByte > 0 ){` |
|     9859050 |  858 | `		rc = SyBlobAppend(&(*pDest),pSrc->pBlob,pSrc->nByte);` |
|     4930417 |  859 | `	}` |
|     9859092 |  860 | `	return rc;` |
|           5 |  861 | `}` |
|         ! 0 |  862 | `PH7_PRIVATE sxi32 SyBlobCmp(SyBlob *pLeft,SyBlob *pRight)` |
|         ! 0 |  863 | `{` |
|           - |  864 | `	sxi32 rc;` |
|           - |  865 | `#ifdef UNTRUST` |
|           - |  866 | `	if( pLeft == 0 \|\| pRight == 0 ){` |
|           - |  867 | `		return pLeft ? 1 : -1;` |
|           - |  868 | `	}` |
|           - |  869 | `#endif` |
|         ! 0 |  870 | `	if( pLeft->nByte != pRight->nByte ){` |
|           - |  871 | `		/* Length differ */` |
|         ! 0 |  872 | `		return pLeft->nByte - pRight->nByte;` |
|           - |  873 | `	}` |
|         ! 0 |  874 | `	if( pLeft->nByte == 0 ){` |
|         ! 0 |  875 | `		return 0;` |
|           - |  876 | `	}` |
|           - |  877 | `	/* Perform a standard memcmp() operation */` |
|         ! 0 |  878 | `	rc = SyMemcmp(pLeft->pBlob,pRight->pBlob,pLeft->nByte);` |
|         ! 0 |  879 | `	return rc;` |
|         ! 0 |  880 | `}` |
|    22746680 |  881 | `PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob)` |
|           5 |  882 | `{` |
|    22746685 |  883 | `	pBlob->nByte = 0;` |
|    22746685 |  884 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|        6061 |  885 | `		pBlob->pBlob = 0;` |
|        6061 |  886 | `		pBlob->mByte = 0;` |
|        6061 |  887 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|        3028 |  888 | `	}` |
|    22746685 |  889 | `	return SXRET_OK;` |
|           5 |  890 | `}` |
|    97288898 |  891 | `PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob)` |
|           5 |  892 | `{` |
|    97288903 |  893 | `	if( (pBlob->nFlags & (SXBLOB_STATIC\|SXBLOB_RDONLY)) == 0 && pBlob->mByte > 0 ){` |
|    26693300 |  894 | `		SyMemBackendFree(pBlob->pAllocator,pBlob->pBlob);` |
|    13348892 |  895 | `	}` |
|    97288903 |  896 | `	pBlob->pBlob = 0;` |
|    97288903 |  897 | `	pBlob->nByte = pBlob->mByte = 0;` |
|    97288903 |  898 | `	pBlob->nFlags = 0;` |
|    97288903 |  899 | `	return SXRET_OK;` |
|           5 |  900 | `}` |
|           - |  901 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      239482 |  902 | `PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft)` |
|           5 |  903 | `{` |
|      239487 |  904 | `	const char *zIn = (const char *)pBlob;` |
|           - |  905 | `	const char *zEnd;` |
|           - |  906 | `	sxi32 rc;` |
|      239487 |  907 | `	if( pLen > nLen ){` |
|        7283 |  908 | `		return SXERR_NOTFOUND;` |
|           - |  909 | `	}` |
|      232209 |  910 | `	zEnd = &zIn[nLen-pLen];` |
|     2375016 |  911 | `	for(;;){` |
|     4747385 |  912 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|     4690319 |  913 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|     4609882 |  914 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|     4560548 |  915 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|           5 |  916 | `	}` |
|       35257 |  917 | `	return SXERR_NOTFOUND;` |
|      119746 |  918 | `}` |
|           - |  919 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|           - |  920 |  |
