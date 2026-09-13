# src/sx/sxmem.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 442/524 lines (84.35%)

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
|   65601471 |   18 | `static void * SyOSHeapAlloc(sxu32 nByte)` |
|          5 |   19 | `{` |
|          - |   20 | `	void *pNew;` |
|          - |   21 | `#if defined(__WINNT__)` |
|          5 |   22 | `	pNew = HeapAlloc(GetProcessHeap(),0,nByte);` |
|          - |   23 | `#else` |
|   65601471 |   24 | `	pNew = malloc((size_t)nByte);` |
|          - |   25 | `#endif` |
|   65601476 |   26 | `	return pNew;` |
|          5 |   27 | `}` |
|    4475070 |   28 | `static void * SyOSHeapRealloc(void *pOld,sxu32 nByte)` |
|          5 |   29 | `{` |
|          - |   30 | `	void *pNew;` |
|          - |   31 | `#if defined(__WINNT__)` |
|          5 |   32 | `	pNew = HeapReAlloc(GetProcessHeap(),0,pOld,nByte);` |
|          - |   33 | `#else` |
|    4475070 |   34 | `	pNew = realloc(pOld,(size_t)nByte);` |
|          - |   35 | `#endif` |
|    4475075 |   36 | `	return pNew;` |
|          5 |   37 | `}` |
|   65598037 |   38 | `static void SyOSHeapFree(void *pPtr)` |
|          5 |   39 | `{` |
|          - |   40 | `#if defined(__WINNT__)` |
|          5 |   41 | `	HeapFree(GetProcessHeap(),0,pPtr);` |
|          - |   42 | `#else` |
|   65598037 |   43 | `	free(pPtr);` |
|          - |   44 | `#endif` |
|   65598042 |   45 | `}` |
|          - |   46 |  |
|          - |   47 |  |
|  178910628 |   48 | `PH7_PRIVATE void SyZero(void *pSrc,sxu32 nSize)` |
|          5 |   49 | `{` |
|  178910633 |   50 | `	register unsigned char *zSrc = (unsigned char *)pSrc;` |
|          - |   51 | `	unsigned char *zEnd;` |
|          - |   52 | `#if defined(UNTRUST)` |
|          - |   53 | `	if( zSrc == 0 \|\| nSize <= 0 ){` |
|          - |   54 | `		return ;` |
|          - |   55 | `	}` |
|          - |   56 | `#endif` |
|  178910633 |   57 | `	zEnd = &zSrc[nSize];` |
| 2713291262 |   58 | `	for(;;){` |
| 5425444495 |   59 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 5246534297 |   60 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 5246534139 |   61 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 5246533931 |   62 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
|          5 |   63 | `	}` |
|  178910633 |   64 | `}` |
|  364740695 |   65 | `PH7_PRIVATE sxi32 SyMemcmp(const void *pB1,const void *pB2,sxu32 nSize)` |
|          5 |   66 | `{` |
|          - |   67 | `	sxi32 rc;` |
|  364740700 |   68 | `	if( nSize <= 0 ){` |
|      15923 |   69 | `		return 0;` |
|          - |   70 | `	}` |
|  364724782 |   71 | `	if( pB1 == 0 \|\| pB2 == 0 ){` |
|        ! 0 |   72 | `		return pB1 != 0 ? 1 : (pB2 == 0 ? 0 : -1);` |
|          - |   73 | `	}` |
|  468443656 |   74 | `	SX_MACRO_FAST_CMP(pB1,pB2,nSize,rc);` |
|  364724782 |   75 | `	return rc;` |
|  182374562 |   76 | `}` |
|   16084834 |   77 | `PH7_PRIVATE sxu32 SyMemcpy(const void *pSrc,void *pDest,sxu32 nLen)` |
|          5 |   78 | `{` |
|          - |   79 | `#if defined(UNTRUST)` |
|          - |   80 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|          - |   81 | `		return 0;` |
|          - |   82 | `	}` |
|          - |   83 | `#endif` |
|   16084839 |   84 | `	if( pSrc == (const void *)pDest ){` |
|        ! 0 |   85 | `		return nLen;` |
|          - |   86 | `	}` |
|  147506059 |   87 | `	SX_MACRO_FAST_MEMCPY(pSrc,pDest,nLen);` |
|   16084839 |   88 | `	return nLen;` |
|    8046563 |   89 | `}` |
|          - |   90 | `/* Size prefix stored ahead of every OS allocation. Padded to pointer size so` |
|          - |   91 | ` * the returned payload (and the SyMemBlock/SyMemHeader the backend lays on` |
|          - |   92 | ` * top of it) keeps the allocator's natural alignment — a bare sxu32 prefix` |
|          - |   93 | ` * left every chunk 4-misaligned on 64-bit platforms. */` |
|          - |   94 | `typedef union MemOSHeader MemOSHeader;` |
|          - |   95 | `union MemOSHeader {` |
|          - |   96 | `	sxu32 nBytes;` |
|          - |   97 | `	void *pAlign;` |
|          - |   98 | `};` |
|   65601471 |   99 | `static void * MemOSAlloc(sxu32 nBytes)` |
|          5 |  100 | `{` |
|          - |  101 | `	MemOSHeader *pChunk;` |
|   65601476 |  102 | `	pChunk = (MemOSHeader *)SyOSHeapAlloc(nBytes + sizeof(MemOSHeader));` |
|   65601476 |  103 | `	if( pChunk == 0 ){` |
|        ! 0 |  104 | `		return 0;` |
|          - |  105 | `	}` |
|   65601476 |  106 | `	pChunk->nBytes = nBytes;` |
|   65601476 |  107 | `	return (void *)&pChunk[1];` |
|   32802446 |  108 | `}` |
|    4475070 |  109 | `static void * MemOSRealloc(void *pOld,sxu32 nBytes)` |
|          5 |  110 | `{` |
|          - |  111 | `	MemOSHeader *pOldChunk;` |
|          - |  112 | `	MemOSHeader *pChunk;` |
|    4475075 |  113 | `	pOldChunk = (MemOSHeader *)(((char *)pOld)-sizeof(MemOSHeader));` |
|    4475075 |  114 | `	if( pOldChunk->nBytes >= nBytes ){` |
|        ! 0 |  115 | `		return pOld;` |
|          - |  116 | `	}` |
|    4475075 |  117 | `	pChunk = (MemOSHeader *)SyOSHeapRealloc(pOldChunk,nBytes + sizeof(MemOSHeader));` |
|    4475075 |  118 | `	if( pChunk == 0 ){` |
|        ! 0 |  119 | `		return 0;` |
|          - |  120 | `	}` |
|    4475075 |  121 | `	pChunk->nBytes = nBytes;` |
|    4475075 |  122 | `	return (void *)&pChunk[1];` |
|    2238658 |  123 | `}` |
|   65598037 |  124 | `static void MemOSFree(void *pBlock)` |
|          5 |  125 | `{` |
|          - |  126 | `	void *pChunk;` |
|   65598042 |  127 | `	pChunk = (void *)(((char *)pBlock)-sizeof(MemOSHeader));` |
|   65598042 |  128 | `	SyOSHeapFree(pChunk);` |
|   65598042 |  129 | `}` |
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
|   65601471 |  146 | `static void * MemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|          5 |  147 | `{` |
|          - |  148 | `	SyMemBlock *pBlock;` |
|   65601476 |  149 | `	sxi32 nRetry = 0;` |
|          - |  150 |  |
|          - |  151 | `	/* Append an extra block so we can tracks allocated chunks and avoid memory` |
|          - |  152 | `	 * leaks.` |
|          - |  153 | `	 */` |
|   65601476 |  154 | `	nByte += sizeof(SyMemBlock);` |
|          - |  155 | `	/* Enforce the optional per-allocation cap (0 = unlimited). A capped failure` |
|          - |  156 | `	 * returns NULL just like a genuine OS failure, driving the normal SXERR_MEM` |
|          - |  157 | `	 * propagation; the retry callback is intentionally skipped (hard limit). */` |
|   65601476 |  158 | `	if( pBackend->nMaxRequest && nByte > pBackend->nMaxRequest ){` |
|        ! 0 |  159 | `		return 0;` |
|          - |  160 | `	}` |
|   32802441 |  161 | `	for(;;){` |
|   32802446 |  162 | `		pBlock = (SyMemBlock *)pBackend->pMethods->xAlloc(nByte);` |
|   65601471 |  163 | `		if( pBlock != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY` |
|          5 |  164 | `			\|\| SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|   32802446 |  165 | `				break;` |
|          - |  166 | `		}` |
|        ! 0 |  167 | `		nRetry++;` |
|        ! 0 |  168 | `	}` |
|   65601476 |  169 | `	if( pBlock  == 0 ){` |
|        ! 0 |  170 | `		return 0;` |
|          - |  171 | `	}` |
|   65601476 |  172 | `	pBlock->pNext = pBlock->pPrev = 0;` |
|          - |  173 | `	/* Link to the list of already tracked blocks */` |
|   65601476 |  174 | `	MACRO_LD_PUSH(pBackend->pBlocks,pBlock);` |
|          - |  175 | `#if defined(UNTRUST)` |
|          - |  176 | `	pBlock->nGuard = SXMEM_BACKEND_MAGIC;` |
|          - |  177 | `#endif` |
|   65601476 |  178 | `	pBlock->nSize = nByte;` |
|   65601476 |  179 | `	pBackend->nMemUsed += nByte;` |
|   65601476 |  180 | `	if( pBackend->nMemUsed > pBackend->nMemPeak ){` |
|   30047284 |  181 | `		pBackend->nMemPeak = pBackend->nMemUsed;` |
|   15023613 |  182 | `	}` |
|   65601476 |  183 | `	pBackend->nBlock++;` |
|   65601476 |  184 | `	return (void *)&pBlock[1];` |
|   32802446 |  185 | `}` |
|   34964990 |  186 | `PH7_PRIVATE void * SyMemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|          5 |  187 | `{` |
|          - |  188 | `	void *pChunk;` |
|          - |  189 | `#if defined(UNTRUST)` |
|          - |  190 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  191 | `		return 0;` |
|          - |  192 | `	}` |
|          - |  193 | `#endif` |
|   34964995 |  194 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  195 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  196 | `	}` |
|   34964995 |  197 | `	pChunk = MemBackendAlloc(&(*pBackend),nByte);` |
|   34964995 |  198 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  199 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  200 | `	}` |
|   34964995 |  201 | `	return pChunk;` |
|          5 |  202 | `}` |
|   34873161 |  203 | `static void * MemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|          5 |  204 | `{` |
|          - |  205 | `	SyMemBlock *pBlock,*pNew,*pPrev,*pNext;` |
|   34873166 |  206 | `	sxu32 nRetry = 0;` |
|          - |  207 |  |
|   34873166 |  208 | `	if( pOld == 0 ){` |
|   30398096 |  209 | `		return MemBackendAlloc(&(*pBackend),nByte);` |
|          - |  210 | `	}` |
|    4475075 |  211 | `	pBlock = (SyMemBlock *)(((char *)pOld) - sizeof(SyMemBlock));` |
|          - |  212 | `#if defined(UNTRUST)` |
|          - |  213 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|          - |  214 | `		return 0;` |
|          - |  215 | `	}` |
|          - |  216 | `#endif` |
|    4475075 |  217 | `	nByte += sizeof(SyMemBlock);` |
|          - |  218 | `	/* Enforce the optional per-allocation cap (0 = unlimited); see MemBackendAlloc. */` |
|    4475075 |  219 | `	if( pBackend->nMaxRequest && nByte > pBackend->nMaxRequest ){` |
|        ! 0 |  220 | `		return 0;` |
|          - |  221 | `	}` |
|    4475075 |  222 | `	pPrev = pBlock->pPrev;` |
|    4475075 |  223 | `	pNext = pBlock->pNext;` |
|          - |  224 | `	{` |
|          - |  225 | `		/* Old size, captured before realloc may move/free the block; the` |
|          - |  226 | `		 * live-byte counter is adjusted by the delta only on success below. */` |
|    4475075 |  227 | `		sxu32 nOld = pBlock->nSize;` |
|    2238653 |  228 | `	for(;;){` |
|    2238658 |  229 | `		pNew = (SyMemBlock *)pBackend->pMethods->xRealloc(pBlock,nByte);` |
|    4475075 |  230 | `		if( pNew != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY \|\|` |
|        ! 0 |  231 | `			SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|    2238658 |  232 | `				break;` |
|          - |  233 | `		}` |
|        ! 0 |  234 | `		nRetry++;` |
|        ! 0 |  235 | `	}` |
|    4475075 |  236 | `	if( pNew == 0 ){` |
|        ! 0 |  237 | `		return 0;` |
|          - |  238 | `	}` |
|    4475075 |  239 | `	if( pNew != pBlock ){` |
|    3902728 |  240 | `		if( pPrev == 0 ){` |
|    1525048 |  241 | `			pBackend->pBlocks = pNew;` |
|     819548 |  242 | `		}else{` |
|    2377685 |  243 | `			pPrev->pNext = pNew;` |
|          - |  244 | `		}` |
|    3902728 |  245 | `		if( pNext ){` |
|    3902714 |  246 | `			pNext->pPrev = pNew;` |
|    2215302 |  247 | `		}` |
|          - |  248 | `#if defined(UNTRUST)` |
|          - |  249 | `		pNew->nGuard = SXMEM_BACKEND_MAGIC;` |
|          - |  250 | `#endif` |
|    2215309 |  251 | `	}` |
|          - |  252 | `	/* Apply the size delta to the live-byte counter (underflow-guarded). */` |
|    4475075 |  253 | `	pBackend->nMemUsed = (pBackend->nMemUsed >= nOld) ? (pBackend->nMemUsed - nOld) : 0;` |
|    4475075 |  254 | `	pBackend->nMemUsed += nByte;` |
|    4475075 |  255 | `	if( pBackend->nMemUsed > pBackend->nMemPeak ){` |
|    2324405 |  256 | `		pBackend->nMemPeak = pBackend->nMemUsed;` |
|    1162210 |  257 | `	}` |
|    4475075 |  258 | `	pNew->nSize = nByte;` |
|    4475075 |  259 | `	return (void *)&pNew[1];` |
|          - |  260 | `	}` |
|   17439036 |  261 | `}` |
|   34873161 |  262 | `PH7_PRIVATE void * SyMemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|          5 |  263 | `{` |
|          - |  264 | `	void *pChunk;` |
|          - |  265 | `#if defined(UNTRUST)` |
|          - |  266 | `	if( SXMEM_BACKEND_CORRUPT(pBackend)  ){` |
|          - |  267 | `		return 0;` |
|          - |  268 | `	}` |
|          - |  269 | `#endif` |
|   34873166 |  270 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  271 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  272 | `	}` |
|   34873166 |  273 | `	pChunk = MemBackendRealloc(&(*pBackend),pOld,nByte);` |
|   34873166 |  274 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  275 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  276 | `	}` |
|   34873166 |  277 | `	return pChunk;` |
|          5 |  278 | `}` |
|   40716669 |  279 | `static sxi32 MemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|          5 |  280 | `{` |
|          - |  281 | `	SyMemBlock *pBlock;` |
|   40716674 |  282 | `	pBlock = (SyMemBlock *)(((char *)pChunk) - sizeof(SyMemBlock));` |
|          - |  283 | `#if defined(UNTRUST)` |
|          - |  284 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|          - |  285 | `		return SXERR_CORRUPT;` |
|          - |  286 | `	}` |
|          - |  287 | `#endif` |
|          - |  288 | `	/* Unlink from the list of active blocks */` |
|   40716674 |  289 | `	if( pBackend->nBlock > 0 ){` |
|          - |  290 | `		/* Release the block */` |
|          - |  291 | `#if defined(UNTRUST)` |
|          - |  292 | `		/* Mark as stale block */` |
|          - |  293 | `		pBlock->nGuard = 0x635B;` |
|          - |  294 | `#endif` |
|   40716674 |  295 | `		MACRO_LD_REMOVE(pBackend->pBlocks,pBlock);` |
|   40716674 |  296 | `		pBackend->nBlock--;` |
|   61073303 |  297 | `		pBackend->nMemUsed = (pBackend->nMemUsed >= pBlock->nSize)` |
|   40716669 |  298 | `			? (pBackend->nMemUsed - pBlock->nSize) : 0;` |
|   40716674 |  299 | `		pBackend->pMethods->xFree(pBlock);` |
|   20360040 |  300 | `	}` |
|   40716674 |  301 | `	return SXRET_OK;` |
|          5 |  302 | `}` |
|   40716669 |  303 | `PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|          5 |  304 | `{` |
|          - |  305 | `	sxi32 rc;` |
|          - |  306 | `#if defined(UNTRUST)` |
|          - |  307 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  308 | `		return SXERR_CORRUPT;` |
|          - |  309 | `	}` |
|          - |  310 | `#endif` |
|   40716674 |  311 | `	if( pChunk == 0 ){` |
|        ! 0 |  312 | `		return SXRET_OK;` |
|          - |  313 | `	}` |
|   40716674 |  314 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  315 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  316 | `	}` |
|   40716674 |  317 | `	rc = MemBackendFree(&(*pBackend),pChunk);` |
|   40716674 |  318 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  319 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  320 | `	}` |
|   40716674 |  321 | `	return rc;` |
|   20360045 |  322 | `}` |
|          - |  323 | `#if defined(PH7_ENABLE_THREADS)` |
|       3890 |  324 | `PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods)` |
|          5 |  325 | `{` |
|          - |  326 | `	SyMutex *pMutex;` |
|          - |  327 | `#if defined(UNTRUST)` |
|          - |  328 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pMethods == 0 \|\| pMethods->xNew == 0){` |
|          - |  329 | `		return SXERR_CORRUPT;` |
|          - |  330 | `	}` |
|          - |  331 | `#endif` |
|       3895 |  332 | `	pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|       3895 |  333 | `	if( pMutex == 0 ){` |
|        ! 0 |  334 | `		return SXERR_OS;` |
|          - |  335 | `	}` |
|          - |  336 | `	/* Attach the mutex to the memory backend */` |
|       3895 |  337 | `	pBackend->pMutex = pMutex;` |
|       3895 |  338 | `	pBackend->pMutexMethods = pMethods;` |
|       3895 |  339 | `	return SXRET_OK;` |
|       1950 |  340 | `}` |
|       3890 |  341 | `PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend)` |
|          5 |  342 | `{` |
|          - |  343 | `#if defined(UNTRUST)` |
|          - |  344 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  345 | `		return SXERR_CORRUPT;` |
|          - |  346 | `	}` |
|          - |  347 | `#endif` |
|       3895 |  348 | `	if( pBackend->pMutex == 0 ){` |
|          - |  349 | `		/* There is no mutex subsystem at all */` |
|        ! 0 |  350 | `		return SXRET_OK;` |
|          - |  351 | `	}` |
|       3895 |  352 | `	SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|       3895 |  353 | `	pBackend->pMutexMethods = 0;` |
|       3895 |  354 | `	pBackend->pMutex = 0;` |
|       3895 |  355 | `	return SXRET_OK;` |
|       1950 |  356 | `}` |
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
|     238390 |  375 | `static sxi32 MemPoolBucketAlloc(SyMemBackend *pBackend,sxu32 nBucket)` |
|          5 |  376 | `{` |
|          - |  377 | `	char *zBucket,*zBucketEnd;` |
|          - |  378 | `	SyMemHeader *pHeader;` |
|          - |  379 | `	sxu32 nBucketSize;` |
|          - |  380 |  |
|          - |  381 | `	/* Allocate one big block first */` |
|     238395 |  382 | `	zBucket = (char *)MemBackendAlloc(&(*pBackend),SXMEM_POOL_MAXALLOC);` |
|     238395 |  383 | `	if( zBucket == 0 ){` |
|        ! 0 |  384 | `		return SXERR_MEM;` |
|          - |  385 | `	}` |
|     238395 |  386 | `	zBucketEnd = &zBucket[SXMEM_POOL_MAXALLOC];` |
|          - |  387 | `	/* Divide the big block into mini bucket pool */` |
|     238395 |  388 | `	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);` |
|     238395 |  389 | `	pBackend->apPool[nBucket] = pHeader = (SyMemHeader *)zBucket;` |
|   24480608 |  390 | `	for(;;){` |
|   48961221 |  391 | `		if( &zBucket[nBucketSize] >= zBucketEnd ){` |
|     238395 |  392 | `			break;` |
|          - |  393 | `		}` |
|   48722831 |  394 | `		pHeader->pNext = (SyMemHeader *)&zBucket[nBucketSize];` |
|          - |  395 | `		/* Advance the cursor to the next available chunk */` |
|   48722831 |  396 | `		pHeader = pHeader->pNext;` |
|   48722831 |  397 | `		zBucket += nBucketSize;` |
|          5 |  398 | `	}` |
|     238395 |  399 | `	pHeader->pNext = 0;` |
|          - |  400 |  |
|     238395 |  401 | `	return SXRET_OK;` |
|     119200 |  402 | `}` |
|  134504005 |  403 | `static void * MemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|          5 |  404 | `{` |
|          - |  405 | `	SyMemHeader *pBucket,*pNext;` |
|          - |  406 | `	sxu32 nBucketSize;` |
|          - |  407 | `	sxu32 nBucket;` |
|          - |  408 |  |
|          - |  409 | `	/* SXMEM_POOL_BYPASS (sanitizer builds): force the big-block path for every` |
|          - |  410 | `	 * request so there is no bucket recycling and ASan tracks each object's` |
|          - |  411 | `	 * real lifetime. Chunks are freed through MemBackendPoolFree's big-block` |
|          - |  412 | `	 * branch either way — one copy of the alloc+tag logic. */` |
|  134504010 |  413 | `	if( SXMEM_POOL_BYPASS_ACTIVE \|\| nByte + sizeof(SyMemHeader) >= SXMEM_POOL_MAXALLOC ){` |
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
|  134504010 |  424 | `	nBucket = 0;` |
|  134504010 |  425 | `	nBucketSize = SXMEM_POOL_MINALLOC;` |
|  693092534 |  426 | `	while( nByte + sizeof(SyMemHeader) > nBucketSize  ){` |
|  558588529 |  427 | `		nBucketSize <<= 1;` |
|  558588529 |  428 | `		nBucket++;` |
|          5 |  429 | `	}` |
|  134504010 |  430 | `	pBucket = pBackend->apPool[nBucket];` |
|  134504010 |  431 | `	if( pBucket == 0 ){` |
|          - |  432 | `		sxi32 rc;` |
|     238395 |  433 | `		rc = MemPoolBucketAlloc(&(*pBackend),nBucket);` |
|     238395 |  434 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  435 | `			return 0;` |
|          - |  436 | `		}` |
|     238395 |  437 | `		pBucket = pBackend->apPool[nBucket];` |
|     119195 |  438 | `	}` |
|          - |  439 | `	/* Remove from the free list */` |
|  134504010 |  440 | `	pNext = pBucket->pNext;` |
|  134504010 |  441 | `	pBackend->apPool[nBucket] = pNext;` |
|          - |  442 | `	/* Record bucket&magic number */` |
|  134504010 |  443 | `	pBucket->nBucket = (((sxu32)SXMEM_POOL_MAGIC << 16) \| nBucket);` |
|  134504010 |  444 | `	return (void *)&pBucket[1];` |
|   67252981 |  445 | `}` |
|  134504005 |  446 | `PH7_PRIVATE void * SyMemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|          5 |  447 | `{` |
|          - |  448 | `	void *pChunk;` |
|          - |  449 | `#if defined(UNTRUST)` |
|          - |  450 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  451 | `		return 0;` |
|          - |  452 | `	}` |
|          - |  453 | `#endif` |
|  134504010 |  454 | `	if( pBackend->pMutexMethods ){` |
|       3895 |  455 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       1945 |  456 | `	}` |
|  134504010 |  457 | `	pChunk = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|  134504010 |  458 | `	if( pBackend->pMutexMethods ){` |
|       3895 |  459 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       1945 |  460 | `	}` |
|  134504010 |  461 | `	return pChunk;` |
|          5 |  462 | `}` |
|   93810213 |  463 | `static sxi32 MemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|          5 |  464 | `{` |
|          - |  465 | `	SyMemHeader *pHeader;` |
|          - |  466 | `	sxu32 nBucket;` |
|          - |  467 | `	/* Get the corresponding bucket */` |
|   93810218 |  468 | `	pHeader = (SyMemHeader *)(((char *)pChunk) - sizeof(SyMemHeader));` |
|          - |  469 | `	/* Sanity check to avoid misuse */` |
|   93810218 |  470 | `	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){` |
|          3 |  471 | `		return SXERR_CORRUPT;` |
|          - |  472 | `	}` |
|   93810216 |  473 | `	nBucket = pHeader->nBucket & 0xFFFF;` |
|   93810216 |  474 | `	if( nBucket == SXU16_HIGH ){` |
|          - |  475 | `		/* Free the big block */` |
|        ! 0 |  476 | `		MemBackendFree(&(*pBackend),pHeader);` |
|   93810216 |  477 | `	}else if( nBucket >= SXMEM_POOL_NBUCKETS + SXMEM_POOL_INCR ){` |
|          - |  478 | `		/* Corrupted or misused bucket index */` |
|        ! 0 |  479 | `		return SXERR_CORRUPT;` |
|        ! 0 |  480 | `	}else{` |
|          - |  481 | `		/* Return to the free list */` |
|   93810216 |  482 | `		pHeader->pNext = pBackend->apPool[nBucket];` |
|   93810216 |  483 | `		pBackend->apPool[nBucket] = pHeader;` |
|          - |  484 | `	}` |
|   93810216 |  485 | `	return SXRET_OK;` |
|   46906085 |  486 | `}` |
|   93810213 |  487 | `PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|          5 |  488 | `{` |
|          - |  489 | `	sxi32 rc;` |
|          - |  490 | `#if defined(UNTRUST)` |
|          - |  491 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pChunk == 0 ){` |
|          - |  492 | `		return SXERR_CORRUPT;` |
|          - |  493 | `	}` |
|          - |  494 | `#endif` |
|   93810218 |  495 | `	if( pBackend->pMutexMethods ){` |
|       3445 |  496 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       1720 |  497 | `	}` |
|   93810218 |  498 | `	rc = MemBackendPoolFree(&(*pBackend),pChunk);` |
|   93810218 |  499 | `	if( pBackend->pMutexMethods ){` |
|       3445 |  500 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       1720 |  501 | `	}` |
|   93810218 |  502 | `	return rc;` |
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
|       3890 |  561 | `PH7_PRIVATE sxi32 SyMemBackendInit(SyMemBackend *pBackend,ProcMemError xMemErr,void * pUserData)` |
|          5 |  562 | `{` |
|          - |  563 | `#if defined(UNTRUST)` |
|          - |  564 | `	if( pBackend == 0 ){` |
|          - |  565 | `		return SXERR_EMPTY;` |
|          - |  566 | `	}` |
|          - |  567 | `#endif` |
|          - |  568 | `	/* Zero the allocator first */` |
|       3895 |  569 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|       3895 |  570 | `	pBackend->xMemError = xMemErr;` |
|       3895 |  571 | `	pBackend->pUserData = pUserData;` |
|          - |  572 | `	/* Switch to the OS memory allocator */` |
|       3895 |  573 | `	pBackend->pMethods = &sOSAllocMethods;` |
|       3895 |  574 | `	if( pBackend->pMethods->xInit ){` |
|          - |  575 | `		/* Initialize the backend  */` |
|        ! 0 |  576 | `		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){` |
|        ! 0 |  577 | `			return SXERR_ABORT;` |
|          - |  578 | `		}` |
|        ! 0 |  579 | `	}` |
|          - |  580 | `#if defined(UNTRUST)` |
|          - |  581 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|          - |  582 | `#endif` |
|       3895 |  583 | `	return SXRET_OK;` |
|       1950 |  584 | `}` |
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
|       7778 |  613 | `PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend *pBackend,SyMemBackend *pParent)` |
|          5 |  614 | `{` |
|          - |  615 | `	sxu8 bInheritMutex;` |
|          - |  616 | `#if defined(UNTRUST)` |
|          - |  617 | `	if( pBackend == 0 \|\| SXMEM_BACKEND_CORRUPT(pParent) ){` |
|          - |  618 | `		return SXERR_CORRUPT;` |
|          - |  619 | `	}` |
|          - |  620 | `#endif` |
|          - |  621 | `	/* Zero the allocator first */` |
|       7783 |  622 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|       7783 |  623 | `	pBackend->pMethods  = pParent->pMethods;` |
|       7783 |  624 | `	pBackend->xMemError = pParent->xMemError;` |
|       7783 |  625 | `	pBackend->pUserData = pParent->pUserData;` |
|       7783 |  626 | `	pBackend->nMaxRequest = pParent->nMaxRequest;` |
|       7783 |  627 | `	bInheritMutex = pParent->pMutexMethods ? TRUE : FALSE;` |
|       7783 |  628 | `	if( bInheritMutex ){` |
|       3895 |  629 | `		pBackend->pMutexMethods = pParent->pMutexMethods;` |
|          - |  630 | `		/* Create a private mutex */` |
|       3895 |  631 | `		pBackend->pMutex = pBackend->pMutexMethods->xNew(SXMUTEX_TYPE_FAST);` |
|       3895 |  632 | `		if( pBackend->pMutex ==  0){` |
|        ! 0 |  633 | `			return SXERR_OS;` |
|          - |  634 | `		}` |
|       1945 |  635 | `	}` |
|          - |  636 | `#if defined(UNTRUST)` |
|          - |  637 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|          - |  638 | `#endif` |
|       7783 |  639 | `	return SXRET_OK;` |
|       3894 |  640 | `}` |
|       8240 |  641 | `static sxi32 MemBackendRelease(SyMemBackend *pBackend)` |
|          5 |  642 | `{` |
|          - |  643 | `	SyMemBlock *pBlock,*pNext;` |
|          - |  644 |  |
|       8245 |  645 | `	pBlock = pBackend->pBlocks;` |
|    3113027 |  646 | `	for(;;){` |
|    6226059 |  647 | `		if( pBackend->nBlock == 0 ){` |
|       1084 |  648 | `			break;` |
|          - |  649 | `		}` |
|    6224979 |  650 | `		pNext  = pBlock->pNext;` |
|    6224979 |  651 | `		pBackend->pMethods->xFree(pBlock);` |
|    6224979 |  652 | `		pBlock = pNext;` |
|    6224979 |  653 | `		pBackend->nBlock--;` |
|          - |  654 | `		/* LOOP ONE */` |
|    6224979 |  655 | `		if( pBackend->nBlock == 0 ){` |
|       4751 |  656 | `			break;` |
|          - |  657 | `		}` |
|    6220233 |  658 | `		pNext  = pBlock->pNext;` |
|    6220233 |  659 | `		pBackend->pMethods->xFree(pBlock);` |
|    6220233 |  660 | `		pBlock = pNext;` |
|    6220233 |  661 | `		pBackend->nBlock--;` |
|          - |  662 | `		/* LOOP TWO */` |
|    6220233 |  663 | `		if( pBackend->nBlock == 0 ){` |
|       1881 |  664 | `			break;` |
|          - |  665 | `		}` |
|    6218357 |  666 | `		pNext  = pBlock->pNext;` |
|    6218357 |  667 | `		pBackend->pMethods->xFree(pBlock);` |
|    6218357 |  668 | `		pBlock = pNext;` |
|    6218357 |  669 | `		pBackend->nBlock--;` |
|          - |  670 | `		/* LOOP THREE */` |
|    6218357 |  671 | `		if( pBackend->nBlock == 0 ){` |
|        542 |  672 | `			break;` |
|          - |  673 | `		}` |
|    6217819 |  674 | `		pNext  = pBlock->pNext;` |
|    6217819 |  675 | `		pBackend->pMethods->xFree(pBlock);` |
|    6217819 |  676 | `		pBlock = pNext;` |
|    6217819 |  677 | `		pBackend->nBlock--;` |
|          - |  678 | `		/* LOOP FOUR */` |
|          5 |  679 | `	}` |
|       8245 |  680 | `	if( pBackend->pMethods->xRelease ){` |
|        ! 0 |  681 | `		pBackend->pMethods->xRelease(pBackend->pMethods->pUserData);` |
|        ! 0 |  682 | `	}` |
|       8245 |  683 | `	pBackend->pMethods = 0;` |
|       8245 |  684 | `	pBackend->pBlocks  = 0;` |
|          - |  685 | `#if defined(UNTRUST)` |
|          - |  686 | `	pBackend->nMagic = 0x2626;` |
|          - |  687 | `#endif` |
|       8245 |  688 | `	return SXRET_OK;` |
|          5 |  689 | `}` |
|       8240 |  690 | `PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend *pBackend)` |
|          5 |  691 | `{` |
|          - |  692 | `#if defined(UNTRUST)` |
|          - |  693 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  694 | `		return SXERR_INVALID;` |
|          - |  695 | `	}` |
|          - |  696 | `#endif` |
|       8245 |  697 | `	if( pBackend->pMutexMethods ){` |
|        460 |  698 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|        228 |  699 | `	}` |
|       8245 |  700 | `	(void)MemBackendRelease(&(*pBackend));` |
|       8245 |  701 | `	if( pBackend->pMutexMethods ){` |
|        460 |  702 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|        460 |  703 | `		SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|        228 |  704 | `	}` |
|       8245 |  705 | `	return SXRET_OK;` |
|          5 |  706 | `}` |
|     931912 |  707 | `PH7_PRIVATE void * SyMemBackendDup(SyMemBackend *pBackend,const void *pSrc,sxu32 nSize)` |
|          5 |  708 | `{` |
|          - |  709 | `	void *pNew;` |
|          - |  710 | `#if defined(UNTRUST)` |
|          - |  711 | `	if( pSrc == 0 \|\| nSize <= 0 ){` |
|          - |  712 | `		return 0;` |
|          - |  713 | `	}` |
|          - |  714 | `#endif` |
|     931917 |  715 | `	pNew = SyMemBackendAlloc(&(*pBackend),nSize);` |
|     931917 |  716 | `	if( pNew ){` |
|     931917 |  717 | `		SyMemcpy(pSrc,pNew,nSize);` |
|     466181 |  718 | `	}` |
|     931917 |  719 | `	return pNew;` |
|          5 |  720 | `}` |
|    9202942 |  721 | `PH7_PRIVATE char * SyMemBackendStrDup(SyMemBackend *pBackend,const char *zSrc,sxu32 nSize)` |
|          5 |  722 | `{` |
|          - |  723 | `	char *zDest;` |
|    9202947 |  724 | `	zDest = (char *)SyMemBackendAlloc(&(*pBackend),nSize + 1);` |
|    9202947 |  725 | `	if( zDest ){` |
|    9202947 |  726 | `		Systrcpy(zDest,nSize+1,zSrc,nSize);` |
|    4601471 |  727 | `	}` |
|    9202947 |  728 | `	return zDest;` |
|          5 |  729 | `}` |
|    2657528 |  730 | `PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize)` |
|          5 |  731 | `{` |
|          - |  732 | `#if defined(UNTRUST)` |
|          - |  733 | `	if( pBlob == 0 \|\| pBuffer == 0 \|\| nSize < 1 ){` |
|          - |  734 | `		return SXERR_EMPTY;` |
|          - |  735 | `	}` |
|          - |  736 | `#endif` |
|    2657533 |  737 | `	pBlob->pBlob = pBuffer;` |
|    2657533 |  738 | `	pBlob->mByte = nSize;` |
|    2657533 |  739 | `	pBlob->nByte = 0;` |
|    2657533 |  740 | `	pBlob->pAllocator = 0;` |
|    2657533 |  741 | `	pBlob->nFlags = SXBLOB_LOCKED\|SXBLOB_STATIC;` |
|    2657533 |  742 | `	return SXRET_OK;` |
|          5 |  743 | `}` |
|   42004700 |  744 | `PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator)` |
|          5 |  745 | `{` |
|          - |  746 | `#if defined(UNTRUST)` |
|          - |  747 | `	if( pBlob == 0  ){` |
|          - |  748 | `		return SXERR_EMPTY;` |
|          - |  749 | `	}` |
|          - |  750 | `#endif` |
|   42004705 |  751 | `	pBlob->pBlob = 0;` |
|   42004705 |  752 | `	pBlob->mByte = pBlob->nByte	= 0;` |
|   42004705 |  753 | `	pBlob->pAllocator = &(*pAllocator);` |
|   42004705 |  754 | `	pBlob->nFlags = 0;` |
|   42004705 |  755 | `	return SXRET_OK;` |
|          5 |  756 | `}` |
|    4667547 |  757 | `PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte)` |
|          5 |  758 | `{` |
|          - |  759 | `#if defined(UNTRUST)` |
|          - |  760 | `	if( pBlob == 0  ){` |
|          - |  761 | `		return SXERR_EMPTY;` |
|          - |  762 | `	}` |
|          - |  763 | `#endif` |
|    4667552 |  764 | `	pBlob->pBlob = (void *)pData;` |
|    4667552 |  765 | `	pBlob->nByte = nByte;` |
|    4667552 |  766 | `	pBlob->mByte = 0;` |
|    4667552 |  767 | `	pBlob->nFlags \|= SXBLOB_RDONLY;` |
|    4667552 |  768 | `	return SXRET_OK;` |
|          5 |  769 | `}` |
|          - |  770 | `#ifndef SXBLOB_MIN_GROWTH` |
|          - |  771 | `#define SXBLOB_MIN_GROWTH 16` |
|          - |  772 | `#endif` |
|   35935852 |  773 | `static sxi32 BlobPrepareGrow(SyBlob *pBlob,sxu32 *pByte)` |
|          5 |  774 | `{` |
|          - |  775 | `	sxu32 nByte;` |
|          - |  776 | `	void *pNew;` |
|   35935857 |  777 | `	nByte = *pByte;` |
|   35935857 |  778 | `	if( pBlob->nFlags & (SXBLOB_LOCKED\|SXBLOB_STATIC) ){` |
|   21260725 |  779 | `		if ( SyBlobFreeSpace(pBlob) < nByte ){` |
|        ! 0 |  780 | `			*pByte = SyBlobFreeSpace(pBlob);` |
|        ! 0 |  781 | `			if( (*pByte) == 0 ){` |
|        ! 0 |  782 | `				return SXERR_SHORT;` |
|          - |  783 | `			}` |
|        ! 0 |  784 | `		}` |
|   21260725 |  785 | `		return SXRET_OK;` |
|          - |  786 | `	}` |
|   14675137 |  787 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|          - |  788 | `		/* Make a copy of the read-only item */` |
|     931899 |  789 | `		if( pBlob->nByte > 0 ){` |
|     931899 |  790 | `			pNew = SyMemBackendDup(pBlob->pAllocator,pBlob->pBlob,pBlob->nByte);` |
|     931899 |  791 | `			if( pNew == 0 ){` |
|        ! 0 |  792 | `				return SXERR_MEM;` |
|          - |  793 | `			}` |
|     931899 |  794 | `			pBlob->pBlob = pNew;` |
|     931899 |  795 | `			pBlob->mByte = pBlob->nByte;` |
|     466177 |  796 | `		}else{` |
|        ! 0 |  797 | `			pBlob->pBlob = 0;` |
|        ! 0 |  798 | `			pBlob->mByte = 0;` |
|          - |  799 | `		}` |
|          - |  800 | `		/* Remove the read-only flag */` |
|     931899 |  801 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|     466172 |  802 | `	}` |
|   14675137 |  803 | `	if( SyBlobFreeSpace(pBlob) >= nByte ){` |
|    2372873 |  804 | `		return SXRET_OK;` |
|          - |  805 | `	}` |
|   12302269 |  806 | `	if( pBlob->mByte > 0 ){` |
|    1131073 |  807 | `		nByte = nByte + pBlob->mByte * 2 + SXBLOB_MIN_GROWTH;` |
|   11737853 |  808 | `	}else if ( nByte < SXBLOB_MIN_GROWTH ){` |
|    8329767 |  809 | `		nByte = SXBLOB_MIN_GROWTH;` |
|    4165172 |  810 | `	}` |
|   12302269 |  811 | `	pNew = SyMemBackendRealloc(pBlob->pAllocator,pBlob->pBlob,nByte);` |
|   12302269 |  812 | `	if( pNew == 0 ){` |
|        ! 0 |  813 | `		return SXERR_MEM;` |
|          - |  814 | `	}` |
|   12302269 |  815 | `	pBlob->pBlob = pNew;` |
|   12302269 |  816 | `	pBlob->mByte = nByte;` |
|   12302269 |  817 | `	return SXRET_OK;` |
|   17969099 |  818 | `}` |
|   36015694 |  819 | `PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize)` |
|          5 |  820 | `{` |
|          - |  821 | `	sxu8 *zBlob;` |
|          - |  822 | `	sxi32 rc;` |
|   36015699 |  823 | `	if( nSize < 1 ){` |
|      79847 |  824 | `		return SXRET_OK;` |
|          - |  825 | `	}` |
|   35935857 |  826 | `	rc = BlobPrepareGrow(&(*pBlob),&nSize);` |
|   35935857 |  827 | `	if( SXRET_OK != rc ){` |
|        ! 0 |  828 | `		return rc;` |
|          - |  829 | `	}` |
|   35935857 |  830 | `	if( pData ){` |
|   35935811 |  831 | `		zBlob = (sxu8 *)pBlob->pBlob ;` |
|   35935811 |  832 | `		zBlob = &zBlob[pBlob->nByte];` |
|   35935811 |  833 | `		pBlob->nByte += nSize;` |
|  134875220 |  834 | `		SX_MACRO_FAST_MEMCPY(pData,zBlob,nSize);` |
|   17969071 |  835 | `	}` |
|   35935857 |  836 | `	return SXRET_OK;` |
|   18009020 |  837 | `}` |
|     960517 |  838 | `PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob)` |
|          5 |  839 | `{` |
|          - |  840 | `	sxi32 rc;` |
|          - |  841 | `	sxu32 n;` |
|     960522 |  842 | `	n = pBlob->nByte;` |
|     960522 |  843 | `	rc = SyBlobAppend(&(*pBlob),(const void *)"\0",sizeof(char));` |
|     960522 |  844 | `	if (rc == SXRET_OK ){` |
|     960522 |  845 | `		pBlob->nByte = n;` |
|     480695 |  846 | `	}` |
|     960522 |  847 | `	return rc;` |
|          5 |  848 | `}` |
|    4457915 |  849 | `PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest)` |
|          5 |  850 | `{` |
|    4457920 |  851 | `	sxi32 rc = SXRET_OK;` |
|          - |  852 | `#ifdef UNTRUST` |
|          - |  853 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|          - |  854 | `		return SXERR_EMPTY;` |
|          - |  855 | `	}` |
|          - |  856 | `#endif` |
|    4457920 |  857 | `	if( pSrc->nByte > 0 ){` |
|    4313884 |  858 | `		rc = SyBlobAppend(&(*pDest),pSrc->pBlob,pSrc->nByte);` |
|    2157164 |  859 | `	}` |
|    4457920 |  860 | `	return rc;` |
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
|   10785051 |  881 | `PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob)` |
|          5 |  882 | `{` |
|   10785056 |  883 | `	pBlob->nByte = 0;` |
|   10785056 |  884 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|       5839 |  885 | `		pBlob->pBlob = 0;` |
|       5839 |  886 | `		pBlob->mByte = 0;` |
|       5839 |  887 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|       2917 |  888 | `	}` |
|   10785056 |  889 | `	return SXRET_OK;` |
|          5 |  890 | `}` |
|   23490202 |  891 | `PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob)` |
|          5 |  892 | `{` |
|   23490207 |  893 | `	if( (pBlob->nFlags & (SXBLOB_STATIC\|SXBLOB_RDONLY)) == 0 && pBlob->mByte > 0 ){` |
|    7656663 |  894 | `		SyMemBackendFree(pBlob->pAllocator,pBlob->pBlob);` |
|    3829212 |  895 | `	}` |
|   23490207 |  896 | `	pBlob->pBlob = 0;` |
|   23490207 |  897 | `	pBlob->nByte = pBlob->mByte = 0;` |
|   23490207 |  898 | `	pBlob->nFlags = 0;` |
|   23490207 |  899 | `	return SXRET_OK;` |
|          5 |  900 | `}` |
|          - |  901 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     202830 |  902 | `PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft)` |
|          5 |  903 | `{` |
|     202835 |  904 | `	const char *zIn = (const char *)pBlob;` |
|          - |  905 | `	const char *zEnd;` |
|          - |  906 | `	sxi32 rc;` |
|     202835 |  907 | `	if( pLen > nLen ){` |
|       6859 |  908 | `		return SXERR_NOTFOUND;` |
|          - |  909 | `	}` |
|     195981 |  910 | `	zEnd = &zIn[nLen-pLen];` |
|    1838651 |  911 | `	for(;;){` |
|    3674828 |  912 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|    3627131 |  913 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|    3557559 |  914 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|    3516342 |  915 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|          5 |  916 | `	}` |
|      31989 |  917 | `	return SXERR_NOTFOUND;` |
|     101420 |  918 | `}` |
|          - |  919 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|          - |  920 |  |
