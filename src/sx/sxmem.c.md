# src/sx/sxmem.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 435/510 lines (85.29%)

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
|   35402229 |   18 | `static void * SyOSHeapAlloc(sxu32 nByte)` |
|          5 |   19 | `{` |
|          - |   20 | `	void *pNew;` |
|          - |   21 | `#if defined(__WINNT__)` |
|          5 |   22 | `	pNew = HeapAlloc(GetProcessHeap(),0,nByte);` |
|          - |   23 | `#else` |
|   35402229 |   24 | `	pNew = malloc((size_t)nByte);` |
|          - |   25 | `#endif` |
|   35402234 |   26 | `	return pNew;` |
|          5 |   27 | `}` |
|    2249817 |   28 | `static void * SyOSHeapRealloc(void *pOld,sxu32 nByte)` |
|          5 |   29 | `{` |
|          - |   30 | `	void *pNew;` |
|          - |   31 | `#if defined(__WINNT__)` |
|          5 |   32 | `	pNew = HeapReAlloc(GetProcessHeap(),0,pOld,nByte);` |
|          - |   33 | `#else` |
|    2249817 |   34 | `	pNew = realloc(pOld,(size_t)nByte);` |
|          - |   35 | `#endif` |
|    2249822 |   36 | `	return pNew;` |
|          5 |   37 | `}` |
|   35398753 |   38 | `static void SyOSHeapFree(void *pPtr)` |
|          5 |   39 | `{` |
|          - |   40 | `#if defined(__WINNT__)` |
|          5 |   41 | `	HeapFree(GetProcessHeap(),0,pPtr);` |
|          - |   42 | `#else` |
|   35398753 |   43 | `	free(pPtr);` |
|          - |   44 | `#endif` |
|   35398758 |   45 | `}` |
|          - |   46 |  |
|          - |   47 |  |
|   87698627 |   48 | `PH7_PRIVATE void SyZero(void *pSrc,sxu32 nSize)` |
|          5 |   49 | `{` |
|   87698632 |   50 | `	register unsigned char *zSrc = (unsigned char *)pSrc;` |
|          - |   51 | `	unsigned char *zEnd;` |
|          - |   52 | `#if defined(UNTRUST)` |
|          - |   53 | `	if( zSrc == 0 \|\| nSize <= 0 ){` |
|          - |   54 | `		return ;` |
|          - |   55 | `	}` |
|          - |   56 | `#endif` |
|   87698632 |   57 | `	zEnd = &zSrc[nSize];` |
| 1311345464 |   58 | `	for(;;){` |
| 2622687912 |   59 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 2534989623 |   60 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 2534989515 |   61 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 2534989323 |   62 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
|          5 |   63 | `	}` |
|   87698632 |   64 | `}` |
|  154005766 |   65 | `PH7_PRIVATE sxi32 SyMemcmp(const void *pB1,const void *pB2,sxu32 nSize)` |
|          5 |   66 | `{` |
|          - |   67 | `	sxi32 rc;` |
|  154005771 |   68 | `	if( nSize <= 0 ){` |
|        562 |   69 | `		return 0;` |
|          - |   70 | `	}` |
|  154005213 |   71 | `	if( pB1 == 0 \|\| pB2 == 0 ){` |
|        ! 0 |   72 | `		return pB1 != 0 ? 1 : (pB2 == 0 ? 0 : -1);` |
|          - |   73 | `	}` |
|  216601869 |   74 | `	SX_MACRO_FAST_CMP(pB1,pB2,nSize,rc);` |
|  154005213 |   75 | `	return rc;` |
|   77003638 |   76 | `}` |
|   12902683 |   77 | `PH7_PRIVATE sxu32 SyMemcpy(const void *pSrc,void *pDest,sxu32 nLen)` |
|          5 |   78 | `{` |
|          - |   79 | `#if defined(UNTRUST)` |
|          - |   80 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|          - |   81 | `		return 0;` |
|          - |   82 | `	}` |
|          - |   83 | `#endif` |
|   12902688 |   84 | `	if( pSrc == (const void *)pDest ){` |
|        ! 0 |   85 | `		return nLen;` |
|          - |   86 | `	}` |
|  106208954 |   87 | `	SX_MACRO_FAST_MEMCPY(pSrc,pDest,nLen);` |
|   12902688 |   88 | `	return nLen;` |
|    6451679 |   89 | `}` |
|          - |   90 | `/* Size prefix stored ahead of every OS allocation. Padded to pointer size so` |
|          - |   91 | ` * the returned payload (and the SyMemBlock/SyMemHeader the backend lays on` |
|          - |   92 | ` * top of it) keeps the allocator's natural alignment — a bare sxu32 prefix` |
|          - |   93 | ` * left every chunk 4-misaligned on 64-bit platforms. */` |
|          - |   94 | `typedef union MemOSHeader MemOSHeader;` |
|          - |   95 | `union MemOSHeader {` |
|          - |   96 | `	sxu32 nBytes;` |
|          - |   97 | `	void *pAlign;` |
|          - |   98 | `};` |
|   35402229 |   99 | `static void * MemOSAlloc(sxu32 nBytes)` |
|          5 |  100 | `{` |
|          - |  101 | `	MemOSHeader *pChunk;` |
|   35402234 |  102 | `	pChunk = (MemOSHeader *)SyOSHeapAlloc(nBytes + sizeof(MemOSHeader));` |
|   35402234 |  103 | `	if( pChunk == 0 ){` |
|        ! 0 |  104 | `		return 0;` |
|          - |  105 | `	}` |
|   35402234 |  106 | `	pChunk->nBytes = nBytes;` |
|   35402234 |  107 | `	return (void *)&pChunk[1];` |
|   17701162 |  108 | `}` |
|    2249817 |  109 | `static void * MemOSRealloc(void *pOld,sxu32 nBytes)` |
|          5 |  110 | `{` |
|          - |  111 | `	MemOSHeader *pOldChunk;` |
|          - |  112 | `	MemOSHeader *pChunk;` |
|    2249822 |  113 | `	pOldChunk = (MemOSHeader *)(((char *)pOld)-sizeof(MemOSHeader));` |
|    2249822 |  114 | `	if( pOldChunk->nBytes >= nBytes ){` |
|        ! 0 |  115 | `		return pOld;` |
|          - |  116 | `	}` |
|    2249822 |  117 | `	pChunk = (MemOSHeader *)SyOSHeapRealloc(pOldChunk,nBytes + sizeof(MemOSHeader));` |
|    2249822 |  118 | `	if( pChunk == 0 ){` |
|        ! 0 |  119 | `		return 0;` |
|          - |  120 | `	}` |
|    2249822 |  121 | `	pChunk->nBytes = nBytes;` |
|    2249822 |  122 | `	return (void *)&pChunk[1];` |
|    1125858 |  123 | `}` |
|   35398753 |  124 | `static void MemOSFree(void *pBlock)` |
|          5 |  125 | `{` |
|          - |  126 | `	void *pChunk;` |
|   35398758 |  127 | `	pChunk = (void *)(((char *)pBlock)-sizeof(MemOSHeader));` |
|   35398758 |  128 | `	SyOSHeapFree(pChunk);` |
|   35398758 |  129 | `}` |
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
|   35402229 |  146 | `static void * MemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|          5 |  147 | `{` |
|          - |  148 | `	SyMemBlock *pBlock;` |
|   35402234 |  149 | `	sxi32 nRetry = 0;` |
|          - |  150 |  |
|          - |  151 | `	/* Append an extra block so we can tracks allocated chunks and avoid memory` |
|          - |  152 | `	 * leaks.` |
|          - |  153 | `	 */` |
|   35402234 |  154 | `	nByte += sizeof(SyMemBlock);` |
|          - |  155 | `	/* Enforce the optional per-allocation cap (0 = unlimited). A capped failure` |
|          - |  156 | `	 * returns NULL just like a genuine OS failure, driving the normal SXERR_MEM` |
|          - |  157 | `	 * propagation; the retry callback is intentionally skipped (hard limit). */` |
|   35402234 |  158 | `	if( pBackend->nMaxRequest && nByte > pBackend->nMaxRequest ){` |
|        ! 0 |  159 | `		return 0;` |
|          - |  160 | `	}` |
|   17701157 |  161 | `	for(;;){` |
|   17701162 |  162 | `		pBlock = (SyMemBlock *)pBackend->pMethods->xAlloc(nByte);` |
|   35402229 |  163 | `		if( pBlock != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY` |
|          5 |  164 | `			\|\| SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|   17701162 |  165 | `				break;` |
|          - |  166 | `		}` |
|        ! 0 |  167 | `		nRetry++;` |
|        ! 0 |  168 | `	}` |
|   35402234 |  169 | `	if( pBlock  == 0 ){` |
|        ! 0 |  170 | `		return 0;` |
|          - |  171 | `	}` |
|   35402234 |  172 | `	pBlock->pNext = pBlock->pPrev = 0;` |
|          - |  173 | `	/* Link to the list of already tracked blocks */` |
|   35402234 |  174 | `	MACRO_LD_PUSH(pBackend->pBlocks,pBlock);` |
|          - |  175 | `#if defined(UNTRUST)` |
|          - |  176 | `	pBlock->nGuard = SXMEM_BACKEND_MAGIC;` |
|          - |  177 | `#endif` |
|   35402234 |  178 | `	pBackend->nBlock++;` |
|   35402234 |  179 | `	return (void *)&pBlock[1];` |
|   17701162 |  180 | `}` |
|   17518742 |  181 | `PH7_PRIVATE void * SyMemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|          5 |  182 | `{` |
|          - |  183 | `	void *pChunk;` |
|          - |  184 | `#if defined(UNTRUST)` |
|          - |  185 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  186 | `		return 0;` |
|          - |  187 | `	}` |
|          - |  188 | `#endif` |
|   17518747 |  189 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  190 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  191 | `	}` |
|   17518747 |  192 | `	pChunk = MemBackendAlloc(&(*pBackend),nByte);` |
|   17518747 |  193 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  194 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  195 | `	}` |
|   17518747 |  196 | `	return pChunk;` |
|          5 |  197 | `}` |
|   19994758 |  198 | `static void * MemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|          5 |  199 | `{` |
|          - |  200 | `	SyMemBlock *pBlock,*pNew,*pPrev,*pNext;` |
|   19994763 |  201 | `	sxu32 nRetry = 0;` |
|          - |  202 |  |
|   19994763 |  203 | `	if( pOld == 0 ){` |
|   17744946 |  204 | `		return MemBackendAlloc(&(*pBackend),nByte);` |
|          - |  205 | `	}` |
|    2249822 |  206 | `	pBlock = (SyMemBlock *)(((char *)pOld) - sizeof(SyMemBlock));` |
|          - |  207 | `#if defined(UNTRUST)` |
|          - |  208 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|          - |  209 | `		return 0;` |
|          - |  210 | `	}` |
|          - |  211 | `#endif` |
|    2249822 |  212 | `	nByte += sizeof(SyMemBlock);` |
|          - |  213 | `	/* Enforce the optional per-allocation cap (0 = unlimited); see MemBackendAlloc. */` |
|    2249822 |  214 | `	if( pBackend->nMaxRequest && nByte > pBackend->nMaxRequest ){` |
|        ! 0 |  215 | `		return 0;` |
|          - |  216 | `	}` |
|    2249822 |  217 | `	pPrev = pBlock->pPrev;` |
|    2249822 |  218 | `	pNext = pBlock->pNext;` |
|    1125853 |  219 | `	for(;;){` |
|    1125858 |  220 | `		pNew = (SyMemBlock *)pBackend->pMethods->xRealloc(pBlock,nByte);` |
|    2249822 |  221 | `		if( pNew != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY \|\|` |
|        ! 0 |  222 | `			SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|    1125858 |  223 | `				break;` |
|          - |  224 | `		}` |
|        ! 0 |  225 | `		nRetry++;` |
|        ! 0 |  226 | `	}` |
|    2249822 |  227 | `	if( pNew == 0 ){` |
|        ! 0 |  228 | `		return 0;` |
|          - |  229 | `	}` |
|    2249822 |  230 | `	if( pNew != pBlock ){` |
|    1947533 |  231 | `		if( pPrev == 0 ){` |
|     890012 |  232 | `			pBackend->pBlocks = pNew;` |
|     499971 |  233 | `		}else{` |
|    1057526 |  234 | `			pPrev->pNext = pNew;` |
|          - |  235 | `		}` |
|    1947533 |  236 | `		if( pNext ){` |
|    1947519 |  237 | `			pNext->pPrev = pNew;` |
|    1112378 |  238 | `		}` |
|          - |  239 | `#if defined(UNTRUST)` |
|          - |  240 | `		pNew->nGuard = SXMEM_BACKEND_MAGIC;` |
|          - |  241 | `#endif` |
|    1112385 |  242 | `	}` |
|    2249822 |  243 | `	return (void *)&pNew[1];` |
|    9998371 |  244 | `}` |
|   19994758 |  245 | `PH7_PRIVATE void * SyMemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|          5 |  246 | `{` |
|          - |  247 | `	void *pChunk;` |
|          - |  248 | `#if defined(UNTRUST)` |
|          - |  249 | `	if( SXMEM_BACKEND_CORRUPT(pBackend)  ){` |
|          - |  250 | `		return 0;` |
|          - |  251 | `	}` |
|          - |  252 | `#endif` |
|   19994763 |  253 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  254 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  255 | `	}` |
|   19994763 |  256 | `	pChunk = MemBackendRealloc(&(*pBackend),pOld,nByte);` |
|   19994763 |  257 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  258 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  259 | `	}` |
|   19994763 |  260 | `	return pChunk;` |
|          5 |  261 | `}` |
|   23002491 |  262 | `static sxi32 MemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|          5 |  263 | `{` |
|          - |  264 | `	SyMemBlock *pBlock;` |
|   23002496 |  265 | `	pBlock = (SyMemBlock *)(((char *)pChunk) - sizeof(SyMemBlock));` |
|          - |  266 | `#if defined(UNTRUST)` |
|          - |  267 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|          - |  268 | `		return SXERR_CORRUPT;` |
|          - |  269 | `	}` |
|          - |  270 | `#endif` |
|          - |  271 | `	/* Unlink from the list of active blocks */` |
|   23002496 |  272 | `	if( pBackend->nBlock > 0 ){` |
|          - |  273 | `		/* Release the block */` |
|          - |  274 | `#if defined(UNTRUST)` |
|          - |  275 | `		/* Mark as stale block */` |
|          - |  276 | `		pBlock->nGuard = 0x635B;` |
|          - |  277 | `#endif` |
|   23002496 |  278 | `		MACRO_LD_REMOVE(pBackend->pBlocks,pBlock);` |
|   23002496 |  279 | `		pBackend->nBlock--;` |
|   23002496 |  280 | `		pBackend->pMethods->xFree(pBlock);` |
|   11501288 |  281 | `	}` |
|   23002496 |  282 | `	return SXRET_OK;` |
|          5 |  283 | `}` |
|   23002491 |  284 | `PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|          5 |  285 | `{` |
|          - |  286 | `	sxi32 rc;` |
|          - |  287 | `#if defined(UNTRUST)` |
|          - |  288 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  289 | `		return SXERR_CORRUPT;` |
|          - |  290 | `	}` |
|          - |  291 | `#endif` |
|   23002496 |  292 | `	if( pChunk == 0 ){` |
|        ! 0 |  293 | `		return SXRET_OK;` |
|          - |  294 | `	}` |
|   23002496 |  295 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  296 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  297 | `	}` |
|   23002496 |  298 | `	rc = MemBackendFree(&(*pBackend),pChunk);` |
|   23002496 |  299 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  300 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  301 | `	}` |
|   23002496 |  302 | `	return rc;` |
|   11501293 |  303 | `}` |
|          - |  304 | `#if defined(PH7_ENABLE_THREADS)` |
|       3848 |  305 | `PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods)` |
|          5 |  306 | `{` |
|          - |  307 | `	SyMutex *pMutex;` |
|          - |  308 | `#if defined(UNTRUST)` |
|          - |  309 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pMethods == 0 \|\| pMethods->xNew == 0){` |
|          - |  310 | `		return SXERR_CORRUPT;` |
|          - |  311 | `	}` |
|          - |  312 | `#endif` |
|       3853 |  313 | `	pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|       3853 |  314 | `	if( pMutex == 0 ){` |
|        ! 0 |  315 | `		return SXERR_OS;` |
|          - |  316 | `	}` |
|          - |  317 | `	/* Attach the mutex to the memory backend */` |
|       3853 |  318 | `	pBackend->pMutex = pMutex;` |
|       3853 |  319 | `	pBackend->pMutexMethods = pMethods;` |
|       3853 |  320 | `	return SXRET_OK;` |
|       1929 |  321 | `}` |
|       3848 |  322 | `PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend)` |
|          5 |  323 | `{` |
|          - |  324 | `#if defined(UNTRUST)` |
|          - |  325 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  326 | `		return SXERR_CORRUPT;` |
|          - |  327 | `	}` |
|          - |  328 | `#endif` |
|       3853 |  329 | `	if( pBackend->pMutex == 0 ){` |
|          - |  330 | `		/* There is no mutex subsystem at all */` |
|        ! 0 |  331 | `		return SXRET_OK;` |
|          - |  332 | `	}` |
|       3853 |  333 | `	SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|       3853 |  334 | `	pBackend->pMutexMethods = 0;` |
|       3853 |  335 | `	pBackend->pMutex = 0;` |
|       3853 |  336 | `	return SXRET_OK;` |
|       1929 |  337 | `}` |
|          - |  338 | `#endif` |
|          - |  339 | `/*` |
|          - |  340 | ` * Memory pool allocator` |
|          - |  341 | ` */` |
|          - |  342 | `#define SXMEM_POOL_MAGIC		0xDEAD` |
|          - |  343 | `#define SXMEM_POOL_MAXALLOC		(1<<(SXMEM_POOL_NBUCKETS+SXMEM_POOL_INCR))` |
|          - |  344 | `#define SXMEM_POOL_MINALLOC		(1<<(SXMEM_POOL_INCR))` |
|          - |  345 | `/* When SXMEM_POOL_BYPASS is defined (sanitizer builds) the bucket-recycling` |
|          - |  346 | ` * path is compiled but never taken — MemBackendPoolAlloc forces the big-block` |
|          - |  347 | ` * branch — so ASan sees one real allocation per request. A compile-time` |
|          - |  348 | ` * constant (not #ifdef scattered through the alloc body) keeps a single copy` |
|          - |  349 | ` * of the alloc/tag/free logic; production builds fold the constant to 0 and` |
|          - |  350 | ` * lose nothing. */` |
|          - |  351 | `#if defined(SXMEM_POOL_BYPASS)` |
|          - |  352 | `# define SXMEM_POOL_BYPASS_ACTIVE 1` |
|          - |  353 | `#else` |
|          - |  354 | `# define SXMEM_POOL_BYPASS_ACTIVE 0` |
|          - |  355 | `#endif` |
|     138546 |  356 | `static sxi32 MemPoolBucketAlloc(SyMemBackend *pBackend,sxu32 nBucket)` |
|          5 |  357 | `{` |
|          - |  358 | `	char *zBucket,*zBucketEnd;` |
|          - |  359 | `	SyMemHeader *pHeader;` |
|          - |  360 | `	sxu32 nBucketSize;` |
|          - |  361 |  |
|          - |  362 | `	/* Allocate one big block first */` |
|     138551 |  363 | `	zBucket = (char *)MemBackendAlloc(&(*pBackend),SXMEM_POOL_MAXALLOC);` |
|     138551 |  364 | `	if( zBucket == 0 ){` |
|        ! 0 |  365 | `		return SXERR_MEM;` |
|          - |  366 | `	}` |
|     138551 |  367 | `	zBucketEnd = &zBucket[SXMEM_POOL_MAXALLOC];` |
|          - |  368 | `	/* Divide the big block into mini bucket pool */` |
|     138551 |  369 | `	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);` |
|     138551 |  370 | `	pBackend->apPool[nBucket] = pHeader = (SyMemHeader *)zBucket;` |
|   14590224 |  371 | `	for(;;){` |
|   29180453 |  372 | `		if( &zBucket[nBucketSize] >= zBucketEnd ){` |
|     138551 |  373 | `			break;` |
|          - |  374 | `		}` |
|   29041907 |  375 | `		pHeader->pNext = (SyMemHeader *)&zBucket[nBucketSize];` |
|          - |  376 | `		/* Advance the cursor to the next available chunk */` |
|   29041907 |  377 | `		pHeader = pHeader->pNext;` |
|   29041907 |  378 | `		zBucket += nBucketSize;` |
|          5 |  379 | `	}` |
|     138551 |  380 | `	pHeader->pNext = 0;` |
|          - |  381 |  |
|     138551 |  382 | `	return SXRET_OK;` |
|      69278 |  383 | `}` |
|   65807110 |  384 | `static void * MemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|          5 |  385 | `{` |
|          - |  386 | `	SyMemHeader *pBucket,*pNext;` |
|          - |  387 | `	sxu32 nBucketSize;` |
|          - |  388 | `	sxu32 nBucket;` |
|          - |  389 |  |
|          - |  390 | `	/* SXMEM_POOL_BYPASS (sanitizer builds): force the big-block path for every` |
|          - |  391 | `	 * request so there is no bucket recycling and ASan tracks each object's` |
|          - |  392 | `	 * real lifetime. Chunks are freed through MemBackendPoolFree's big-block` |
|          - |  393 | `	 * branch either way — one copy of the alloc+tag logic. */` |
|   65807115 |  394 | `	if( SXMEM_POOL_BYPASS_ACTIVE \|\| nByte + sizeof(SyMemHeader) >= SXMEM_POOL_MAXALLOC ){` |
|          - |  395 | `		/* Allocate a big chunk directly */` |
|        ! 0 |  396 | `		pBucket = (SyMemHeader *)MemBackendAlloc(&(*pBackend),nByte+sizeof(SyMemHeader));` |
|        ! 0 |  397 | `		if( pBucket == 0 ){` |
|        ! 0 |  398 | `			return 0;` |
|          - |  399 | `		}` |
|          - |  400 | `		/* Record as big block */` |
|        ! 0 |  401 | `		pBucket->nBucket = ((sxu32)SXMEM_POOL_MAGIC << 16) \| SXU16_HIGH;` |
|        ! 0 |  402 | `		return (void *)(pBucket+1);` |
|          - |  403 | `	}` |
|          - |  404 | `	/* Locate the appropriate bucket */` |
|   65807115 |  405 | `	nBucket = 0;` |
|   65807115 |  406 | `	nBucketSize = SXMEM_POOL_MINALLOC;` |
|  331993427 |  407 | `	while( nByte + sizeof(SyMemHeader) > nBucketSize  ){` |
|  266186317 |  408 | `		nBucketSize <<= 1;` |
|  266186317 |  409 | `		nBucket++;` |
|          5 |  410 | `	}` |
|   65807115 |  411 | `	pBucket = pBackend->apPool[nBucket];` |
|   65807115 |  412 | `	if( pBucket == 0 ){` |
|          - |  413 | `		sxi32 rc;` |
|     138551 |  414 | `		rc = MemPoolBucketAlloc(&(*pBackend),nBucket);` |
|     138551 |  415 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  416 | `			return 0;` |
|          - |  417 | `		}` |
|     138551 |  418 | `		pBucket = pBackend->apPool[nBucket];` |
|      69273 |  419 | `	}` |
|          - |  420 | `	/* Remove from the free list */` |
|   65807115 |  421 | `	pNext = pBucket->pNext;` |
|   65807115 |  422 | `	pBackend->apPool[nBucket] = pNext;` |
|          - |  423 | `	/* Record bucket&magic number */` |
|   65807115 |  424 | `	pBucket->nBucket = (((sxu32)SXMEM_POOL_MAGIC << 16) \| nBucket);` |
|   65807115 |  425 | `	return (void *)&pBucket[1];` |
|   32903560 |  426 | `}` |
|   65807110 |  427 | `PH7_PRIVATE void * SyMemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|          5 |  428 | `{` |
|          - |  429 | `	void *pChunk;` |
|          - |  430 | `#if defined(UNTRUST)` |
|          - |  431 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  432 | `		return 0;` |
|          - |  433 | `	}` |
|          - |  434 | `#endif` |
|   65807115 |  435 | `	if( pBackend->pMutexMethods ){` |
|       3853 |  436 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       1924 |  437 | `	}` |
|   65807115 |  438 | `	pChunk = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|   65807115 |  439 | `	if( pBackend->pMutexMethods ){` |
|       3853 |  440 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       1924 |  441 | `	}` |
|   65807115 |  442 | `	return pChunk;` |
|          5 |  443 | `}` |
|   43882272 |  444 | `static sxi32 MemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|          5 |  445 | `{` |
|          - |  446 | `	SyMemHeader *pHeader;` |
|          - |  447 | `	sxu32 nBucket;` |
|          - |  448 | `	/* Get the corresponding bucket */` |
|   43882277 |  449 | `	pHeader = (SyMemHeader *)(((char *)pChunk) - sizeof(SyMemHeader));` |
|          - |  450 | `	/* Sanity check to avoid misuse */` |
|   43882277 |  451 | `	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){` |
|          3 |  452 | `		return SXERR_CORRUPT;` |
|          - |  453 | `	}` |
|   43882275 |  454 | `	nBucket = pHeader->nBucket & 0xFFFF;` |
|   43882275 |  455 | `	if( nBucket == SXU16_HIGH ){` |
|          - |  456 | `		/* Free the big block */` |
|        ! 0 |  457 | `		MemBackendFree(&(*pBackend),pHeader);` |
|   43882275 |  458 | `	}else if( nBucket >= SXMEM_POOL_NBUCKETS + SXMEM_POOL_INCR ){` |
|          - |  459 | `		/* Corrupted or misused bucket index */` |
|        ! 0 |  460 | `		return SXERR_CORRUPT;` |
|        ! 0 |  461 | `	}else{` |
|          - |  462 | `		/* Return to the free list */` |
|   43882275 |  463 | `		pHeader->pNext = pBackend->apPool[nBucket];` |
|   43882275 |  464 | `		pBackend->apPool[nBucket] = pHeader;` |
|          - |  465 | `	}` |
|   43882275 |  466 | `	return SXRET_OK;` |
|   21941141 |  467 | `}` |
|   43882272 |  468 | `PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|          5 |  469 | `{` |
|          - |  470 | `	sxi32 rc;` |
|          - |  471 | `#if defined(UNTRUST)` |
|          - |  472 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pChunk == 0 ){` |
|          - |  473 | `		return SXERR_CORRUPT;` |
|          - |  474 | `	}` |
|          - |  475 | `#endif` |
|   43882277 |  476 | `	if( pBackend->pMutexMethods ){` |
|       3481 |  477 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       1738 |  478 | `	}` |
|   43882277 |  479 | `	rc = MemBackendPoolFree(&(*pBackend),pChunk);` |
|   43882277 |  480 | `	if( pBackend->pMutexMethods ){` |
|       3481 |  481 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       1738 |  482 | `	}` |
|   43882277 |  483 | `	return rc;` |
|          5 |  484 | `}` |
|          - |  485 | `#if 0` |
|          - |  486 | `static void * MemBackendPoolRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|          - |  487 | `{` |
|          - |  488 | `	sxu32 nBucket,nBucketSize;` |
|          - |  489 | `	SyMemHeader *pHeader;` |
|          - |  490 | `	void * pNew;` |
|          - |  491 |  |
|          - |  492 | `	if( pOld == 0 ){` |
|          - |  493 | `		/* Allocate a new pool */` |
|          - |  494 | `		pNew = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|          - |  495 | `		return pNew;` |
|          - |  496 | `	}` |
|          - |  497 | `	/* Get the corresponding bucket */` |
|          - |  498 | `	pHeader = (SyMemHeader *)(((char *)pOld) - sizeof(SyMemHeader));` |
|          - |  499 | `	/* Sanity check to avoid misuse */` |
|          - |  500 | `	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){` |
|          - |  501 | `		return 0;` |
|          - |  502 | `	}` |
|          - |  503 | `	nBucket = pHeader->nBucket & 0xFFFF;` |
|          - |  504 | `	if( nBucket == SXU16_HIGH ){` |
|          - |  505 | `		/* Big block */` |
|          - |  506 | `		return MemBackendRealloc(&(*pBackend),pHeader,nByte);` |
|          - |  507 | `	}` |
|          - |  508 | `	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);` |
|          - |  509 | `	if( nBucketSize >= nByte + sizeof(SyMemHeader) ){` |
|          - |  510 | `		/* The old bucket can honor the requested size */` |
|          - |  511 | `		return pOld;` |
|          - |  512 | `	}` |
|          - |  513 | `	/* Allocate a new pool */` |
|          - |  514 | `	pNew = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|          - |  515 | `	if( pNew == 0 ){` |
|          - |  516 | `		return 0;` |
|          - |  517 | `	}` |
|          - |  518 | `	/* Copy the old data into the new block */` |
|          - |  519 | `	SyMemcpy(pOld,pNew,nBucketSize);` |
|          - |  520 | `	/* Free the stale block */` |
|          - |  521 | `	MemBackendPoolFree(&(*pBackend),pOld);` |
|          - |  522 | `	return pNew;` |
|          - |  523 | `}` |
|          - |  524 | `PH7_PRIVATE void * SyMemBackendPoolRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|          - |  525 | `{` |
|          - |  526 | `	void *pChunk;` |
|          - |  527 | `#if defined(UNTRUST)` |
|          - |  528 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  529 | `		return 0;` |
|          - |  530 | `	}` |
|          - |  531 | `#endif` |
|          - |  532 | `	if( pBackend->pMutexMethods ){` |
|          - |  533 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|          - |  534 | `	}` |
|          - |  535 | `	pChunk = MemBackendPoolRealloc(&(*pBackend),pOld,nByte);` |
|          - |  536 | `	if( pBackend->pMutexMethods ){` |
|          - |  537 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|          - |  538 | `	}` |
|          - |  539 | `	return pChunk;` |
|          - |  540 | `}` |
|          - |  541 | `#endif` |
|       3848 |  542 | `PH7_PRIVATE sxi32 SyMemBackendInit(SyMemBackend *pBackend,ProcMemError xMemErr,void * pUserData)` |
|          5 |  543 | `{` |
|          - |  544 | `#if defined(UNTRUST)` |
|          - |  545 | `	if( pBackend == 0 ){` |
|          - |  546 | `		return SXERR_EMPTY;` |
|          - |  547 | `	}` |
|          - |  548 | `#endif` |
|          - |  549 | `	/* Zero the allocator first */` |
|       3853 |  550 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|       3853 |  551 | `	pBackend->xMemError = xMemErr;` |
|       3853 |  552 | `	pBackend->pUserData = pUserData;` |
|          - |  553 | `	/* Switch to the OS memory allocator */` |
|       3853 |  554 | `	pBackend->pMethods = &sOSAllocMethods;` |
|       3853 |  555 | `	if( pBackend->pMethods->xInit ){` |
|          - |  556 | `		/* Initialize the backend  */` |
|        ! 0 |  557 | `		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){` |
|        ! 0 |  558 | `			return SXERR_ABORT;` |
|          - |  559 | `		}` |
|        ! 0 |  560 | `	}` |
|          - |  561 | `#if defined(UNTRUST)` |
|          - |  562 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|          - |  563 | `#endif` |
|       3853 |  564 | `	return SXRET_OK;` |
|       1929 |  565 | `}` |
|        ! 0 |  566 | `PH7_PRIVATE sxi32 SyMemBackendInitFromOthers(SyMemBackend *pBackend,const SyMemMethods *pMethods,ProcMemError xMemErr,void * pUserData)` |
|        ! 0 |  567 | `{` |
|          - |  568 | `#if defined(UNTRUST)` |
|          - |  569 | `	if( pBackend == 0 \|\| pMethods == 0){` |
|          - |  570 | `		return SXERR_EMPTY;` |
|          - |  571 | `	}` |
|          - |  572 | `#endif` |
|        ! 0 |  573 | `	if( pMethods->xAlloc == 0 \|\| pMethods->xRealloc == 0 \|\| pMethods->xFree == 0 \|\| pMethods->xChunkSize == 0 ){` |
|          - |  574 | `		/* mandatory methods are missing */` |
|        ! 0 |  575 | `		return SXERR_INVALID;` |
|          - |  576 | `	}` |
|          - |  577 | `	/* Zero the allocator first */` |
|        ! 0 |  578 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|        ! 0 |  579 | `	pBackend->xMemError = xMemErr;` |
|        ! 0 |  580 | `	pBackend->pUserData = pUserData;` |
|          - |  581 | `	/* Switch to the host application memory allocator */` |
|        ! 0 |  582 | `	pBackend->pMethods = pMethods;` |
|        ! 0 |  583 | `	if( pBackend->pMethods->xInit ){` |
|          - |  584 | `		/* Initialize the backend  */` |
|        ! 0 |  585 | `		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){` |
|        ! 0 |  586 | `			return SXERR_ABORT;` |
|          - |  587 | `		}` |
|        ! 0 |  588 | `	}` |
|          - |  589 | `#if defined(UNTRUST)` |
|          - |  590 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|          - |  591 | `#endif` |
|        ! 0 |  592 | `	return SXRET_OK;` |
|        ! 0 |  593 | `}` |
|       7692 |  594 | `PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend *pBackend,SyMemBackend *pParent)` |
|          5 |  595 | `{` |
|          - |  596 | `	sxu8 bInheritMutex;` |
|          - |  597 | `#if defined(UNTRUST)` |
|          - |  598 | `	if( pBackend == 0 \|\| SXMEM_BACKEND_CORRUPT(pParent) ){` |
|          - |  599 | `		return SXERR_CORRUPT;` |
|          - |  600 | `	}` |
|          - |  601 | `#endif` |
|          - |  602 | `	/* Zero the allocator first */` |
|       7697 |  603 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|       7697 |  604 | `	pBackend->pMethods  = pParent->pMethods;` |
|       7697 |  605 | `	pBackend->xMemError = pParent->xMemError;` |
|       7697 |  606 | `	pBackend->pUserData = pParent->pUserData;` |
|       7697 |  607 | `	pBackend->nMaxRequest = pParent->nMaxRequest;` |
|       7697 |  608 | `	bInheritMutex = pParent->pMutexMethods ? TRUE : FALSE;` |
|       7697 |  609 | `	if( bInheritMutex ){` |
|       3853 |  610 | `		pBackend->pMutexMethods = pParent->pMutexMethods;` |
|          - |  611 | `		/* Create a private mutex */` |
|       3853 |  612 | `		pBackend->pMutex = pBackend->pMutexMethods->xNew(SXMUTEX_TYPE_FAST);` |
|       3853 |  613 | `		if( pBackend->pMutex ==  0){` |
|        ! 0 |  614 | `			return SXERR_OS;` |
|          - |  615 | `		}` |
|       1924 |  616 | `	}` |
|          - |  617 | `#if defined(UNTRUST)` |
|          - |  618 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|          - |  619 | `#endif` |
|       7697 |  620 | `	return SXRET_OK;` |
|       3851 |  621 | `}` |
|       8064 |  622 | `static sxi32 MemBackendRelease(SyMemBackend *pBackend)` |
|          5 |  623 | `{` |
|          - |  624 | `	SyMemBlock *pBlock,*pNext;` |
|          - |  625 |  |
|       8069 |  626 | `	pBlock = pBackend->pBlocks;` |
|    1552443 |  627 | `	for(;;){` |
|    3104891 |  628 | `		if( pBackend->nBlock == 0 ){` |
|       1612 |  629 | `			break;` |
|          - |  630 | `		}` |
|    3103283 |  631 | `		pNext  = pBlock->pNext;` |
|    3103283 |  632 | `		pBackend->pMethods->xFree(pBlock);` |
|    3103283 |  633 | `		pBlock = pNext;` |
|    3103283 |  634 | `		pBackend->nBlock--;` |
|          - |  635 | `		/* LOOP ONE */` |
|    3103283 |  636 | `		if( pBackend->nBlock == 0 ){` |
|       4863 |  637 | `			break;` |
|          - |  638 | `		}` |
|    3098425 |  639 | `		pNext  = pBlock->pNext;` |
|    3098425 |  640 | `		pBackend->pMethods->xFree(pBlock);` |
|    3098425 |  641 | `		pBlock = pNext;` |
|    3098425 |  642 | `		pBackend->nBlock--;` |
|          - |  643 | `		/* LOOP TWO */` |
|    3098425 |  644 | `		if( pBackend->nBlock == 0 ){` |
|        683 |  645 | `			break;` |
|          - |  646 | `		}` |
|    3097747 |  647 | `		pNext  = pBlock->pNext;` |
|    3097747 |  648 | `		pBackend->pMethods->xFree(pBlock);` |
|    3097747 |  649 | `		pBlock = pNext;` |
|    3097747 |  650 | `		pBackend->nBlock--;` |
|          - |  651 | `		/* LOOP THREE */` |
|    3097747 |  652 | `		if( pBackend->nBlock == 0 ){` |
|        924 |  653 | `			break;` |
|          - |  654 | `		}` |
|    3096827 |  655 | `		pNext  = pBlock->pNext;` |
|    3096827 |  656 | `		pBackend->pMethods->xFree(pBlock);` |
|    3096827 |  657 | `		pBlock = pNext;` |
|    3096827 |  658 | `		pBackend->nBlock--;` |
|          - |  659 | `		/* LOOP FOUR */` |
|          5 |  660 | `	}` |
|       8069 |  661 | `	if( pBackend->pMethods->xRelease ){` |
|        ! 0 |  662 | `		pBackend->pMethods->xRelease(pBackend->pMethods->pUserData);` |
|        ! 0 |  663 | `	}` |
|       8069 |  664 | `	pBackend->pMethods = 0;` |
|       8069 |  665 | `	pBackend->pBlocks  = 0;` |
|          - |  666 | `#if defined(UNTRUST)` |
|          - |  667 | `	pBackend->nMagic = 0x2626;` |
|          - |  668 | `#endif` |
|       8069 |  669 | `	return SXRET_OK;` |
|          5 |  670 | `}` |
|       8064 |  671 | `PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend *pBackend)` |
|          5 |  672 | `{` |
|          - |  673 | `#if defined(UNTRUST)` |
|          - |  674 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  675 | `		return SXERR_INVALID;` |
|          - |  676 | `	}` |
|          - |  677 | `#endif` |
|       8069 |  678 | `	if( pBackend->pMutexMethods ){` |
|        376 |  679 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|        186 |  680 | `	}` |
|       8069 |  681 | `	(void)MemBackendRelease(&(*pBackend));` |
|       8069 |  682 | `	if( pBackend->pMutexMethods ){` |
|        376 |  683 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|        376 |  684 | `		SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|        186 |  685 | `	}` |
|       8069 |  686 | `	return SXRET_OK;` |
|          5 |  687 | `}` |
|     747144 |  688 | `PH7_PRIVATE void * SyMemBackendDup(SyMemBackend *pBackend,const void *pSrc,sxu32 nSize)` |
|          5 |  689 | `{` |
|          - |  690 | `	void *pNew;` |
|          - |  691 | `#if defined(UNTRUST)` |
|          - |  692 | `	if( pSrc == 0 \|\| nSize <= 0 ){` |
|          - |  693 | `		return 0;` |
|          - |  694 | `	}` |
|          - |  695 | `#endif` |
|     747149 |  696 | `	pNew = SyMemBackendAlloc(&(*pBackend),nSize);` |
|     747149 |  697 | `	if( pNew ){` |
|     747149 |  698 | `		SyMemcpy(pSrc,pNew,nSize);` |
|     373572 |  699 | `	}` |
|     747149 |  700 | `	return pNew;` |
|          5 |  701 | `}` |
|    4953726 |  702 | `PH7_PRIVATE char * SyMemBackendStrDup(SyMemBackend *pBackend,const char *zSrc,sxu32 nSize)` |
|          5 |  703 | `{` |
|          - |  704 | `	char *zDest;` |
|    4953731 |  705 | `	zDest = (char *)SyMemBackendAlloc(&(*pBackend),nSize + 1);` |
|    4953731 |  706 | `	if( zDest ){` |
|    4953731 |  707 | `		Systrcpy(zDest,nSize+1,zSrc,nSize);` |
|    2476863 |  708 | `	}` |
|    4953731 |  709 | `	return zDest;` |
|          5 |  710 | `}` |
|    1374944 |  711 | `PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize)` |
|          5 |  712 | `{` |
|          - |  713 | `#if defined(UNTRUST)` |
|          - |  714 | `	if( pBlob == 0 \|\| pBuffer == 0 \|\| nSize < 1 ){` |
|          - |  715 | `		return SXERR_EMPTY;` |
|          - |  716 | `	}` |
|          - |  717 | `#endif` |
|    1374949 |  718 | `	pBlob->pBlob = pBuffer;` |
|    1374949 |  719 | `	pBlob->mByte = nSize;` |
|    1374949 |  720 | `	pBlob->nByte = 0;` |
|    1374949 |  721 | `	pBlob->pAllocator = 0;` |
|    1374949 |  722 | `	pBlob->nFlags = SXBLOB_LOCKED\|SXBLOB_STATIC;` |
|    1374949 |  723 | `	return SXRET_OK;` |
|          5 |  724 | `}` |
|   20920173 |  725 | `PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator)` |
|          5 |  726 | `{` |
|          - |  727 | `#if defined(UNTRUST)` |
|          - |  728 | `	if( pBlob == 0  ){` |
|          - |  729 | `		return SXERR_EMPTY;` |
|          - |  730 | `	}` |
|          - |  731 | `#endif` |
|   20920178 |  732 | `	pBlob->pBlob = 0;` |
|   20920178 |  733 | `	pBlob->mByte = pBlob->nByte	= 0;` |
|   20920178 |  734 | `	pBlob->pAllocator = &(*pAllocator);` |
|   20920178 |  735 | `	pBlob->nFlags = 0;` |
|   20920178 |  736 | `	return SXRET_OK;` |
|          5 |  737 | `}` |
|    3680742 |  738 | `PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte)` |
|          5 |  739 | `{` |
|          - |  740 | `#if defined(UNTRUST)` |
|          - |  741 | `	if( pBlob == 0  ){` |
|          - |  742 | `		return SXERR_EMPTY;` |
|          - |  743 | `	}` |
|          - |  744 | `#endif` |
|    3680747 |  745 | `	pBlob->pBlob = (void *)pData;` |
|    3680747 |  746 | `	pBlob->nByte = nByte;` |
|    3680747 |  747 | `	pBlob->mByte = 0;` |
|    3680747 |  748 | `	pBlob->nFlags \|= SXBLOB_RDONLY;` |
|    3680747 |  749 | `	return SXRET_OK;` |
|          5 |  750 | `}` |
|          - |  751 | `#ifndef SXBLOB_MIN_GROWTH` |
|          - |  752 | `#define SXBLOB_MIN_GROWTH 16` |
|          - |  753 | `#endif` |
|   21092862 |  754 | `static sxi32 BlobPrepareGrow(SyBlob *pBlob,sxu32 *pByte)` |
|          5 |  755 | `{` |
|          - |  756 | `	sxu32 nByte;` |
|          - |  757 | `	void *pNew;` |
|   21092867 |  758 | `	nByte = *pByte;` |
|   21092867 |  759 | `	if( pBlob->nFlags & (SXBLOB_LOCKED\|SXBLOB_STATIC) ){` |
|   10994121 |  760 | `		if ( SyBlobFreeSpace(pBlob) < nByte ){` |
|        ! 0 |  761 | `			*pByte = SyBlobFreeSpace(pBlob);` |
|        ! 0 |  762 | `			if( (*pByte) == 0 ){` |
|        ! 0 |  763 | `				return SXERR_SHORT;` |
|          - |  764 | `			}` |
|        ! 0 |  765 | `		}` |
|   10994121 |  766 | `		return SXRET_OK;` |
|          - |  767 | `	}` |
|   10098751 |  768 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|          - |  769 | `		/* Make a copy of the read-only item */` |
|     747149 |  770 | `		if( pBlob->nByte > 0 ){` |
|     747149 |  771 | `			pNew = SyMemBackendDup(pBlob->pAllocator,pBlob->pBlob,pBlob->nByte);` |
|     747149 |  772 | `			if( pNew == 0 ){` |
|        ! 0 |  773 | `				return SXERR_MEM;` |
|          - |  774 | `			}` |
|     747149 |  775 | `			pBlob->pBlob = pNew;` |
|     747149 |  776 | `			pBlob->mByte = pBlob->nByte;` |
|     373577 |  777 | `		}else{` |
|        ! 0 |  778 | `			pBlob->pBlob = 0;` |
|        ! 0 |  779 | `			pBlob->mByte = 0;` |
|          - |  780 | `		}` |
|          - |  781 | `		/* Remove the read-only flag */` |
|     747149 |  782 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|     373572 |  783 | `	}` |
|   10098751 |  784 | `	if( SyBlobFreeSpace(pBlob) >= nByte ){` |
|    1802261 |  785 | `		return SXRET_OK;` |
|          - |  786 | `	}` |
|    8296495 |  787 | `	if( pBlob->mByte > 0 ){` |
|     884906 |  788 | `		nByte = nByte + pBlob->mByte * 2 + SXBLOB_MIN_GROWTH;` |
|    7854989 |  789 | `	}else if ( nByte < SXBLOB_MIN_GROWTH ){` |
|    5598545 |  790 | `		nByte = SXBLOB_MIN_GROWTH;` |
|    2799176 |  791 | `	}` |
|    8296495 |  792 | `	pNew = SyMemBackendRealloc(pBlob->pAllocator,pBlob->pBlob,nByte);` |
|    8296495 |  793 | `	if( pNew == 0 ){` |
|        ! 0 |  794 | `		return SXERR_MEM;` |
|          - |  795 | `	}` |
|    8296495 |  796 | `	pBlob->pBlob = pNew;` |
|    8296495 |  797 | `	pBlob->mByte = nByte;` |
|    8296495 |  798 | `	return SXRET_OK;` |
|   10546521 |  799 | `}` |
|   21156520 |  800 | `PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize)` |
|          5 |  801 | `{` |
|          - |  802 | `	sxu8 *zBlob;` |
|          - |  803 | `	sxi32 rc;` |
|   21156525 |  804 | `	if( nSize < 1 ){` |
|      63663 |  805 | `		return SXRET_OK;` |
|          - |  806 | `	}` |
|   21092867 |  807 | `	rc = BlobPrepareGrow(&(*pBlob),&nSize);` |
|   21092867 |  808 | `	if( SXRET_OK != rc ){` |
|        ! 0 |  809 | `		return rc;` |
|          - |  810 | `	}` |
|   21092867 |  811 | `	if( pData ){` |
|   21092827 |  812 | `		zBlob = (sxu8 *)pBlob->pBlob ;` |
|   21092827 |  813 | `		zBlob = &zBlob[pBlob->nByte];` |
|   21092827 |  814 | `		pBlob->nByte += nSize;` |
|   74502247 |  815 | `		SX_MACRO_FAST_MEMCPY(pData,zBlob,nSize);` |
|   10546496 |  816 | `	}` |
|   21092867 |  817 | `	return SXRET_OK;` |
|   10578350 |  818 | `}` |
|     756073 |  819 | `PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob)` |
|          5 |  820 | `{` |
|          - |  821 | `	sxi32 rc;` |
|          - |  822 | `	sxu32 n;` |
|     756078 |  823 | `	n = pBlob->nByte;` |
|     756078 |  824 | `	rc = SyBlobAppend(&(*pBlob),(const void *)"\0",sizeof(char));` |
|     756078 |  825 | `	if (rc == SXRET_OK ){` |
|     756078 |  826 | `		pBlob->nByte = n;` |
|     378079 |  827 | `	}` |
|     756078 |  828 | `	return rc;` |
|          5 |  829 | `}` |
|    4089228 |  830 | `PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest)` |
|          5 |  831 | `{` |
|    4089233 |  832 | `	sxi32 rc = SXRET_OK;` |
|          - |  833 | `#ifdef UNTRUST` |
|          - |  834 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|          - |  835 | `		return SXERR_EMPTY;` |
|          - |  836 | `	}` |
|          - |  837 | `#endif` |
|    4089233 |  838 | `	if( pSrc->nByte > 0 ){` |
|    3985289 |  839 | `		rc = SyBlobAppend(&(*pDest),pSrc->pBlob,pSrc->nByte);` |
|    1992642 |  840 | `	}` |
|    4089233 |  841 | `	return rc;` |
|          5 |  842 | `}` |
|         10 |  843 | `PH7_PRIVATE sxi32 SyBlobCmp(SyBlob *pLeft,SyBlob *pRight)` |
|          1 |  844 | `{` |
|          - |  845 | `	sxi32 rc;` |
|          - |  846 | `#ifdef UNTRUST` |
|          - |  847 | `	if( pLeft == 0 \|\| pRight == 0 ){` |
|          - |  848 | `		return pLeft ? 1 : -1;` |
|          - |  849 | `	}` |
|          - |  850 | `#endif` |
|         11 |  851 | `	if( pLeft->nByte != pRight->nByte ){` |
|          - |  852 | `		/* Length differ */` |
|        ! 0 |  853 | `		return pLeft->nByte - pRight->nByte;` |
|          - |  854 | `	}` |
|         11 |  855 | `	if( pLeft->nByte == 0 ){` |
|        ! 0 |  856 | `		return 0;` |
|          - |  857 | `	}` |
|          - |  858 | `	/* Perform a standard memcmp() operation */` |
|         11 |  859 | `	rc = SyMemcmp(pLeft->pBlob,pRight->pBlob,pLeft->nByte);` |
|         11 |  860 | `	return rc;` |
|          6 |  861 | `}` |
|    7155306 |  862 | `PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob)` |
|          5 |  863 | `{` |
|    7155311 |  864 | `	pBlob->nByte = 0;` |
|    7155311 |  865 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|       5033 |  866 | `		pBlob->pBlob = 0;` |
|       5033 |  867 | `		pBlob->mByte = 0;` |
|       5033 |  868 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|       2514 |  869 | `	}` |
|    7155311 |  870 | `	return SXRET_OK;` |
|          5 |  871 | `}` |
|   17092311 |  872 | `PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob)` |
|          5 |  873 | `{` |
|   17092316 |  874 | `	if( (pBlob->nFlags & (SXBLOB_STATIC\|SXBLOB_RDONLY)) == 0 && pBlob->mByte > 0 ){` |
|    6083274 |  875 | `		SyMemBackendFree(pBlob->pAllocator,pBlob->pBlob);` |
|    3041677 |  876 | `	}` |
|   17092316 |  877 | `	pBlob->pBlob = 0;` |
|   17092316 |  878 | `	pBlob->nByte = pBlob->mByte = 0;` |
|   17092316 |  879 | `	pBlob->nFlags = 0;` |
|   17092316 |  880 | `	return SXRET_OK;` |
|          5 |  881 | `}` |
|          - |  882 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     181714 |  883 | `PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft)` |
|          5 |  884 | `{` |
|     181719 |  885 | `	const char *zIn = (const char *)pBlob;` |
|          - |  886 | `	const char *zEnd;` |
|          - |  887 | `	sxi32 rc;` |
|     181719 |  888 | `	if( pLen > nLen ){` |
|       6473 |  889 | `		return SXERR_NOTFOUND;` |
|          - |  890 | `	}` |
|     175251 |  891 | `	zEnd = &zIn[nLen-pLen];` |
|    1541143 |  892 | `	for(;;){` |
|    3080492 |  893 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|    3038257 |  894 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|    2975869 |  895 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|    2937968 |  896 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|          5 |  897 | `	}` |
|      29947 |  898 | `	return SXERR_NOTFOUND;` |
|      90862 |  899 | `}` |
|          - |  900 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|          - |  901 |  |
