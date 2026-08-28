# src/sx/sxmem.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 428/510 lines (83.92%)

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
|   53939921 |   18 | `static void * SyOSHeapAlloc(sxu32 nByte)` |
|          5 |   19 | `{` |
|          - |   20 | `	void *pNew;` |
|          - |   21 | `#if defined(__WINNT__)` |
|          5 |   22 | `	pNew = HeapAlloc(GetProcessHeap(),0,nByte);` |
|          - |   23 | `#else` |
|   53939921 |   24 | `	pNew = malloc((size_t)nByte);` |
|          - |   25 | `#endif` |
|   53939926 |   26 | `	return pNew;` |
|          5 |   27 | `}` |
|    3599640 |   28 | `static void * SyOSHeapRealloc(void *pOld,sxu32 nByte)` |
|          5 |   29 | `{` |
|          - |   30 | `	void *pNew;` |
|          - |   31 | `#if defined(__WINNT__)` |
|          5 |   32 | `	pNew = HeapReAlloc(GetProcessHeap(),0,pOld,nByte);` |
|          - |   33 | `#else` |
|    3599640 |   34 | `	pNew = realloc(pOld,(size_t)nByte);` |
|          - |   35 | `#endif` |
|    3599645 |   36 | `	return pNew;` |
|          5 |   37 | `}` |
|   53936401 |   38 | `static void SyOSHeapFree(void *pPtr)` |
|          5 |   39 | `{` |
|          - |   40 | `#if defined(__WINNT__)` |
|          5 |   41 | `	HeapFree(GetProcessHeap(),0,pPtr);` |
|          - |   42 | `#else` |
|   53936401 |   43 | `	free(pPtr);` |
|          - |   44 | `#endif` |
|   53936406 |   45 | `}` |
|          - |   46 |  |
|          - |   47 |  |
|  135344523 |   48 | `PH7_PRIVATE void SyZero(void *pSrc,sxu32 nSize)` |
|          5 |   49 | `{` |
|  135344528 |   50 | `	register unsigned char *zSrc = (unsigned char *)pSrc;` |
|          - |   51 | `	unsigned char *zEnd;` |
|          - |   52 | `#if defined(UNTRUST)` |
|          - |   53 | `	if( zSrc == 0 \|\| nSize <= 0 ){` |
|          - |   54 | `		return ;` |
|          - |   55 | `	}` |
|          - |   56 | `#endif` |
|  135344528 |   57 | `	zEnd = &zSrc[nSize];` |
| 2102448962 |   58 | `	for(;;){` |
| 4204895364 |   59 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 4069551267 |   60 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 4069551113 |   61 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 4069550905 |   62 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
|          5 |   63 | `	}` |
|  135344528 |   64 | `}` |
|  281989306 |   65 | `PH7_PRIVATE sxi32 SyMemcmp(const void *pB1,const void *pB2,sxu32 nSize)` |
|          5 |   66 | `{` |
|          - |   67 | `	sxi32 rc;` |
|  281989311 |   68 | `	if( nSize <= 0 ){` |
|        767 |   69 | `		return 0;` |
|          - |   70 | `	}` |
|  281988549 |   71 | `	if( pB1 == 0 \|\| pB2 == 0 ){` |
|        ! 0 |   72 | `		return pB1 != 0 ? 1 : (pB2 == 0 ? 0 : -1);` |
|          - |   73 | `	}` |
|  370058097 |   74 | `	SX_MACRO_FAST_CMP(pB1,pB2,nSize,rc);` |
|  281988549 |   75 | `	return rc;` |
|  140995416 |   76 | `}` |
|   13152789 |   77 | `PH7_PRIVATE sxu32 SyMemcpy(const void *pSrc,void *pDest,sxu32 nLen)` |
|          5 |   78 | `{` |
|          - |   79 | `#if defined(UNTRUST)` |
|          - |   80 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|          - |   81 | `		return 0;` |
|          - |   82 | `	}` |
|          - |   83 | `#endif` |
|   13152794 |   84 | `	if( pSrc == (const void *)pDest ){` |
|        ! 0 |   85 | `		return nLen;` |
|          - |   86 | `	}` |
|  108994792 |   87 | `	SX_MACRO_FAST_MEMCPY(pSrc,pDest,nLen);` |
|   13152794 |   88 | `	return nLen;` |
|    6576725 |   89 | `}` |
|          - |   90 | `/* Size prefix stored ahead of every OS allocation. Padded to pointer size so` |
|          - |   91 | ` * the returned payload (and the SyMemBlock/SyMemHeader the backend lays on` |
|          - |   92 | ` * top of it) keeps the allocator's natural alignment — a bare sxu32 prefix` |
|          - |   93 | ` * left every chunk 4-misaligned on 64-bit platforms. */` |
|          - |   94 | `typedef union MemOSHeader MemOSHeader;` |
|          - |   95 | `union MemOSHeader {` |
|          - |   96 | `	sxu32 nBytes;` |
|          - |   97 | `	void *pAlign;` |
|          - |   98 | `};` |
|   53939921 |   99 | `static void * MemOSAlloc(sxu32 nBytes)` |
|          5 |  100 | `{` |
|          - |  101 | `	MemOSHeader *pChunk;` |
|   53939926 |  102 | `	pChunk = (MemOSHeader *)SyOSHeapAlloc(nBytes + sizeof(MemOSHeader));` |
|   53939926 |  103 | `	if( pChunk == 0 ){` |
|        ! 0 |  104 | `		return 0;` |
|          - |  105 | `	}` |
|   53939926 |  106 | `	pChunk->nBytes = nBytes;` |
|   53939926 |  107 | `	return (void *)&pChunk[1];` |
|   26970008 |  108 | `}` |
|    3599640 |  109 | `static void * MemOSRealloc(void *pOld,sxu32 nBytes)` |
|          5 |  110 | `{` |
|          - |  111 | `	MemOSHeader *pOldChunk;` |
|          - |  112 | `	MemOSHeader *pChunk;` |
|    3599645 |  113 | `	pOldChunk = (MemOSHeader *)(((char *)pOld)-sizeof(MemOSHeader));` |
|    3599645 |  114 | `	if( pOldChunk->nBytes >= nBytes ){` |
|        ! 0 |  115 | `		return pOld;` |
|          - |  116 | `	}` |
|    3599645 |  117 | `	pChunk = (MemOSHeader *)SyOSHeapRealloc(pOldChunk,nBytes + sizeof(MemOSHeader));` |
|    3599645 |  118 | `	if( pChunk == 0 ){` |
|        ! 0 |  119 | `		return 0;` |
|          - |  120 | `	}` |
|    3599645 |  121 | `	pChunk->nBytes = nBytes;` |
|    3599645 |  122 | `	return (void *)&pChunk[1];` |
|    1800775 |  123 | `}` |
|   53936401 |  124 | `static void MemOSFree(void *pBlock)` |
|          5 |  125 | `{` |
|          - |  126 | `	void *pChunk;` |
|   53936406 |  127 | `	pChunk = (void *)(((char *)pBlock)-sizeof(MemOSHeader));` |
|   53936406 |  128 | `	SyOSHeapFree(pChunk);` |
|   53936406 |  129 | `}` |
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
|   53939921 |  146 | `static void * MemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|          5 |  147 | `{` |
|          - |  148 | `	SyMemBlock *pBlock;` |
|   53939926 |  149 | `	sxi32 nRetry = 0;` |
|          - |  150 |  |
|          - |  151 | `	/* Append an extra block so we can tracks allocated chunks and avoid memory` |
|          - |  152 | `	 * leaks.` |
|          - |  153 | `	 */` |
|   53939926 |  154 | `	nByte += sizeof(SyMemBlock);` |
|          - |  155 | `	/* Enforce the optional per-allocation cap (0 = unlimited). A capped failure` |
|          - |  156 | `	 * returns NULL just like a genuine OS failure, driving the normal SXERR_MEM` |
|          - |  157 | `	 * propagation; the retry callback is intentionally skipped (hard limit). */` |
|   53939926 |  158 | `	if( pBackend->nMaxRequest && nByte > pBackend->nMaxRequest ){` |
|        ! 0 |  159 | `		return 0;` |
|          - |  160 | `	}` |
|   26970003 |  161 | `	for(;;){` |
|   26970008 |  162 | `		pBlock = (SyMemBlock *)pBackend->pMethods->xAlloc(nByte);` |
|   53939921 |  163 | `		if( pBlock != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY` |
|          5 |  164 | `			\|\| SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|   26970008 |  165 | `				break;` |
|          - |  166 | `		}` |
|        ! 0 |  167 | `		nRetry++;` |
|        ! 0 |  168 | `	}` |
|   53939926 |  169 | `	if( pBlock  == 0 ){` |
|        ! 0 |  170 | `		return 0;` |
|          - |  171 | `	}` |
|   53939926 |  172 | `	pBlock->pNext = pBlock->pPrev = 0;` |
|          - |  173 | `	/* Link to the list of already tracked blocks */` |
|   53939926 |  174 | `	MACRO_LD_PUSH(pBackend->pBlocks,pBlock);` |
|          - |  175 | `#if defined(UNTRUST)` |
|          - |  176 | `	pBlock->nGuard = SXMEM_BACKEND_MAGIC;` |
|          - |  177 | `#endif` |
|   53939926 |  178 | `	pBackend->nBlock++;` |
|   53939926 |  179 | `	return (void *)&pBlock[1];` |
|   26970008 |  180 | `}` |
|   28162554 |  181 | `PH7_PRIVATE void * SyMemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|          5 |  182 | `{` |
|          - |  183 | `	void *pChunk;` |
|          - |  184 | `#if defined(UNTRUST)` |
|          - |  185 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  186 | `		return 0;` |
|          - |  187 | `	}` |
|          - |  188 | `#endif` |
|   28162559 |  189 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  190 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  191 | `	}` |
|   28162559 |  192 | `	pChunk = MemBackendAlloc(&(*pBackend),nByte);` |
|   28162559 |  193 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  194 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  195 | `	}` |
|   28162559 |  196 | `	return pChunk;` |
|          5 |  197 | `}` |
|   29170181 |  198 | `static void * MemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|          5 |  199 | `{` |
|          - |  200 | `	SyMemBlock *pBlock,*pNew,*pPrev,*pNext;` |
|   29170186 |  201 | `	sxu32 nRetry = 0;` |
|          - |  202 |  |
|   29170186 |  203 | `	if( pOld == 0 ){` |
|   25570546 |  204 | `		return MemBackendAlloc(&(*pBackend),nByte);` |
|          - |  205 | `	}` |
|    3599645 |  206 | `	pBlock = (SyMemBlock *)(((char *)pOld) - sizeof(SyMemBlock));` |
|          - |  207 | `#if defined(UNTRUST)` |
|          - |  208 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|          - |  209 | `		return 0;` |
|          - |  210 | `	}` |
|          - |  211 | `#endif` |
|    3599645 |  212 | `	nByte += sizeof(SyMemBlock);` |
|          - |  213 | `	/* Enforce the optional per-allocation cap (0 = unlimited); see MemBackendAlloc. */` |
|    3599645 |  214 | `	if( pBackend->nMaxRequest && nByte > pBackend->nMaxRequest ){` |
|        ! 0 |  215 | `		return 0;` |
|          - |  216 | `	}` |
|    3599645 |  217 | `	pPrev = pBlock->pPrev;` |
|    3599645 |  218 | `	pNext = pBlock->pNext;` |
|    1800770 |  219 | `	for(;;){` |
|    1800775 |  220 | `		pNew = (SyMemBlock *)pBackend->pMethods->xRealloc(pBlock,nByte);` |
|    3599645 |  221 | `		if( pNew != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY \|\|` |
|        ! 0 |  222 | `			SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|    1800775 |  223 | `				break;` |
|          - |  224 | `		}` |
|        ! 0 |  225 | `		nRetry++;` |
|        ! 0 |  226 | `	}` |
|    3599645 |  227 | `	if( pNew == 0 ){` |
|        ! 0 |  228 | `		return 0;` |
|          - |  229 | `	}` |
|    3599645 |  230 | `	if( pNew != pBlock ){` |
|    3098070 |  231 | `		if( pPrev == 0 ){` |
|    1198199 |  232 | `			pBackend->pBlocks = pNew;` |
|     669141 |  233 | `		}else{` |
|    1899876 |  234 | `			pPrev->pNext = pNew;` |
|          - |  235 | `		}` |
|    3098070 |  236 | `		if( pNext ){` |
|    3098058 |  237 | `			pNext->pPrev = pNew;` |
|    1779279 |  238 | `		}` |
|          - |  239 | `#if defined(UNTRUST)` |
|          - |  240 | `		pNew->nGuard = SXMEM_BACKEND_MAGIC;` |
|          - |  241 | `#endif` |
|    1779286 |  242 | `	}` |
|    3599645 |  243 | `	return (void *)&pNew[1];` |
|   14586088 |  244 | `}` |
|   29170181 |  245 | `PH7_PRIVATE void * SyMemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|          5 |  246 | `{` |
|          - |  247 | `	void *pChunk;` |
|          - |  248 | `#if defined(UNTRUST)` |
|          - |  249 | `	if( SXMEM_BACKEND_CORRUPT(pBackend)  ){` |
|          - |  250 | `		return 0;` |
|          - |  251 | `	}` |
|          - |  252 | `#endif` |
|   29170186 |  253 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  254 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  255 | `	}` |
|   29170186 |  256 | `	pChunk = MemBackendRealloc(&(*pBackend),pOld,nByte);` |
|   29170186 |  257 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  258 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  259 | `	}` |
|   29170186 |  260 | `	return pChunk;` |
|          5 |  261 | `}` |
|   33415197 |  262 | `static sxi32 MemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|          5 |  263 | `{` |
|          - |  264 | `	SyMemBlock *pBlock;` |
|   33415202 |  265 | `	pBlock = (SyMemBlock *)(((char *)pChunk) - sizeof(SyMemBlock));` |
|          - |  266 | `#if defined(UNTRUST)` |
|          - |  267 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|          - |  268 | `		return SXERR_CORRUPT;` |
|          - |  269 | `	}` |
|          - |  270 | `#endif` |
|          - |  271 | `	/* Unlink from the list of active blocks */` |
|   33415202 |  272 | `	if( pBackend->nBlock > 0 ){` |
|          - |  273 | `		/* Release the block */` |
|          - |  274 | `#if defined(UNTRUST)` |
|          - |  275 | `		/* Mark as stale block */` |
|          - |  276 | `		pBlock->nGuard = 0x635B;` |
|          - |  277 | `#endif` |
|   33415202 |  278 | `		MACRO_LD_REMOVE(pBackend->pBlocks,pBlock);` |
|   33415202 |  279 | `		pBackend->nBlock--;` |
|   33415202 |  280 | `		pBackend->pMethods->xFree(pBlock);` |
|   16707641 |  281 | `	}` |
|   33415202 |  282 | `	return SXRET_OK;` |
|          5 |  283 | `}` |
|   33415197 |  284 | `PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|          5 |  285 | `{` |
|          - |  286 | `	sxi32 rc;` |
|          - |  287 | `#if defined(UNTRUST)` |
|          - |  288 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  289 | `		return SXERR_CORRUPT;` |
|          - |  290 | `	}` |
|          - |  291 | `#endif` |
|   33415202 |  292 | `	if( pChunk == 0 ){` |
|        ! 0 |  293 | `		return SXRET_OK;` |
|          - |  294 | `	}` |
|   33415202 |  295 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  296 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  297 | `	}` |
|   33415202 |  298 | `	rc = MemBackendFree(&(*pBackend),pChunk);` |
|   33415202 |  299 | `	if( pBackend->pMutexMethods ){` |
|        ! 0 |  300 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|        ! 0 |  301 | `	}` |
|   33415202 |  302 | `	return rc;` |
|   16707646 |  303 | `}` |
|          - |  304 | `#if defined(PH7_ENABLE_THREADS)` |
|       3908 |  305 | `PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods)` |
|          5 |  306 | `{` |
|          - |  307 | `	SyMutex *pMutex;` |
|          - |  308 | `#if defined(UNTRUST)` |
|          - |  309 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pMethods == 0 \|\| pMethods->xNew == 0){` |
|          - |  310 | `		return SXERR_CORRUPT;` |
|          - |  311 | `	}` |
|          - |  312 | `#endif` |
|       3913 |  313 | `	pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|       3913 |  314 | `	if( pMutex == 0 ){` |
|        ! 0 |  315 | `		return SXERR_OS;` |
|          - |  316 | `	}` |
|          - |  317 | `	/* Attach the mutex to the memory backend */` |
|       3913 |  318 | `	pBackend->pMutex = pMutex;` |
|       3913 |  319 | `	pBackend->pMutexMethods = pMethods;` |
|       3913 |  320 | `	return SXRET_OK;` |
|       1959 |  321 | `}` |
|       3908 |  322 | `PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend)` |
|          5 |  323 | `{` |
|          - |  324 | `#if defined(UNTRUST)` |
|          - |  325 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  326 | `		return SXERR_CORRUPT;` |
|          - |  327 | `	}` |
|          - |  328 | `#endif` |
|       3913 |  329 | `	if( pBackend->pMutex == 0 ){` |
|          - |  330 | `		/* There is no mutex subsystem at all */` |
|        ! 0 |  331 | `		return SXRET_OK;` |
|          - |  332 | `	}` |
|       3913 |  333 | `	SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|       3913 |  334 | `	pBackend->pMutexMethods = 0;` |
|       3913 |  335 | `	pBackend->pMutex = 0;` |
|       3913 |  336 | `	return SXRET_OK;` |
|       1959 |  337 | `}` |
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
|     206826 |  356 | `static sxi32 MemPoolBucketAlloc(SyMemBackend *pBackend,sxu32 nBucket)` |
|          5 |  357 | `{` |
|          - |  358 | `	char *zBucket,*zBucketEnd;` |
|          - |  359 | `	SyMemHeader *pHeader;` |
|          - |  360 | `	sxu32 nBucketSize;` |
|          - |  361 |  |
|          - |  362 | `	/* Allocate one big block first */` |
|     206831 |  363 | `	zBucket = (char *)MemBackendAlloc(&(*pBackend),SXMEM_POOL_MAXALLOC);` |
|     206831 |  364 | `	if( zBucket == 0 ){` |
|        ! 0 |  365 | `		return SXERR_MEM;` |
|          - |  366 | `	}` |
|     206831 |  367 | `	zBucketEnd = &zBucket[SXMEM_POOL_MAXALLOC];` |
|          - |  368 | `	/* Divide the big block into mini bucket pool */` |
|     206831 |  369 | `	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);` |
|     206831 |  370 | `	pBackend->apPool[nBucket] = pHeader = (SyMemHeader *)zBucket;` |
|   21151360 |  371 | `	for(;;){` |
|   42302725 |  372 | `		if( &zBucket[nBucketSize] >= zBucketEnd ){` |
|     206831 |  373 | `			break;` |
|          - |  374 | `		}` |
|   42095899 |  375 | `		pHeader->pNext = (SyMemHeader *)&zBucket[nBucketSize];` |
|          - |  376 | `		/* Advance the cursor to the next available chunk */` |
|   42095899 |  377 | `		pHeader = pHeader->pNext;` |
|   42095899 |  378 | `		zBucket += nBucketSize;` |
|          5 |  379 | `	}` |
|     206831 |  380 | `	pHeader->pNext = 0;` |
|          - |  381 |  |
|     206831 |  382 | `	return SXRET_OK;` |
|     103418 |  383 | `}` |
|  109112978 |  384 | `static void * MemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|          5 |  385 | `{` |
|          - |  386 | `	SyMemHeader *pBucket,*pNext;` |
|          - |  387 | `	sxu32 nBucketSize;` |
|          - |  388 | `	sxu32 nBucket;` |
|          - |  389 |  |
|          - |  390 | `	/* SXMEM_POOL_BYPASS (sanitizer builds): force the big-block path for every` |
|          - |  391 | `	 * request so there is no bucket recycling and ASan tracks each object's` |
|          - |  392 | `	 * real lifetime. Chunks are freed through MemBackendPoolFree's big-block` |
|          - |  393 | `	 * branch either way — one copy of the alloc+tag logic. */` |
|  109112983 |  394 | `	if( SXMEM_POOL_BYPASS_ACTIVE \|\| nByte + sizeof(SyMemHeader) >= SXMEM_POOL_MAXALLOC ){` |
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
|  109112983 |  405 | `	nBucket = 0;` |
|  109112983 |  406 | `	nBucketSize = SXMEM_POOL_MINALLOC;` |
|  552520951 |  407 | `	while( nByte + sizeof(SyMemHeader) > nBucketSize  ){` |
|  443407973 |  408 | `		nBucketSize <<= 1;` |
|  443407973 |  409 | `		nBucket++;` |
|          5 |  410 | `	}` |
|  109112983 |  411 | `	pBucket = pBackend->apPool[nBucket];` |
|  109112983 |  412 | `	if( pBucket == 0 ){` |
|          - |  413 | `		sxi32 rc;` |
|     206831 |  414 | `		rc = MemPoolBucketAlloc(&(*pBackend),nBucket);` |
|     206831 |  415 | `		if( rc != SXRET_OK ){` |
|        ! 0 |  416 | `			return 0;` |
|          - |  417 | `		}` |
|     206831 |  418 | `		pBucket = pBackend->apPool[nBucket];` |
|     103413 |  419 | `	}` |
|          - |  420 | `	/* Remove from the free list */` |
|  109112983 |  421 | `	pNext = pBucket->pNext;` |
|  109112983 |  422 | `	pBackend->apPool[nBucket] = pNext;` |
|          - |  423 | `	/* Record bucket&magic number */` |
|  109112983 |  424 | `	pBucket->nBucket = (((sxu32)SXMEM_POOL_MAGIC << 16) \| nBucket);` |
|  109112983 |  425 | `	return (void *)&pBucket[1];` |
|   54556494 |  426 | `}` |
|  109112978 |  427 | `PH7_PRIVATE void * SyMemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|          5 |  428 | `{` |
|          - |  429 | `	void *pChunk;` |
|          - |  430 | `#if defined(UNTRUST)` |
|          - |  431 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  432 | `		return 0;` |
|          - |  433 | `	}` |
|          - |  434 | `#endif` |
|  109112983 |  435 | `	if( pBackend->pMutexMethods ){` |
|       3913 |  436 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       1954 |  437 | `	}` |
|  109112983 |  438 | `	pChunk = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|  109112983 |  439 | `	if( pBackend->pMutexMethods ){` |
|       3913 |  440 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       1954 |  441 | `	}` |
|  109112983 |  442 | `	return pChunk;` |
|          5 |  443 | `}` |
|   75176806 |  444 | `static sxi32 MemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|          5 |  445 | `{` |
|          - |  446 | `	SyMemHeader *pHeader;` |
|          - |  447 | `	sxu32 nBucket;` |
|          - |  448 | `	/* Get the corresponding bucket */` |
|   75176811 |  449 | `	pHeader = (SyMemHeader *)(((char *)pChunk) - sizeof(SyMemHeader));` |
|          - |  450 | `	/* Sanity check to avoid misuse */` |
|   75176811 |  451 | `	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){` |
|          3 |  452 | `		return SXERR_CORRUPT;` |
|          - |  453 | `	}` |
|   75176809 |  454 | `	nBucket = pHeader->nBucket & 0xFFFF;` |
|   75176809 |  455 | `	if( nBucket == SXU16_HIGH ){` |
|          - |  456 | `		/* Free the big block */` |
|        ! 0 |  457 | `		MemBackendFree(&(*pBackend),pHeader);` |
|   75176809 |  458 | `	}else if( nBucket >= SXMEM_POOL_NBUCKETS + SXMEM_POOL_INCR ){` |
|          - |  459 | `		/* Corrupted or misused bucket index */` |
|        ! 0 |  460 | `		return SXERR_CORRUPT;` |
|        ! 0 |  461 | `	}else{` |
|          - |  462 | `		/* Return to the free list */` |
|   75176809 |  463 | `		pHeader->pNext = pBackend->apPool[nBucket];` |
|   75176809 |  464 | `		pBackend->apPool[nBucket] = pHeader;` |
|          - |  465 | `	}` |
|   75176809 |  466 | `	return SXRET_OK;` |
|   37588408 |  467 | `}` |
|   75176806 |  468 | `PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|          5 |  469 | `{` |
|          - |  470 | `	sxi32 rc;` |
|          - |  471 | `#if defined(UNTRUST)` |
|          - |  472 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pChunk == 0 ){` |
|          - |  473 | `		return SXERR_CORRUPT;` |
|          - |  474 | `	}` |
|          - |  475 | `#endif` |
|   75176811 |  476 | `	if( pBackend->pMutexMethods ){` |
|       3525 |  477 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|       1760 |  478 | `	}` |
|   75176811 |  479 | `	rc = MemBackendPoolFree(&(*pBackend),pChunk);` |
|   75176811 |  480 | `	if( pBackend->pMutexMethods ){` |
|       3525 |  481 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|       1760 |  482 | `	}` |
|   75176811 |  483 | `	return rc;` |
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
|       3908 |  542 | `PH7_PRIVATE sxi32 SyMemBackendInit(SyMemBackend *pBackend,ProcMemError xMemErr,void * pUserData)` |
|          5 |  543 | `{` |
|          - |  544 | `#if defined(UNTRUST)` |
|          - |  545 | `	if( pBackend == 0 ){` |
|          - |  546 | `		return SXERR_EMPTY;` |
|          - |  547 | `	}` |
|          - |  548 | `#endif` |
|          - |  549 | `	/* Zero the allocator first */` |
|       3913 |  550 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|       3913 |  551 | `	pBackend->xMemError = xMemErr;` |
|       3913 |  552 | `	pBackend->pUserData = pUserData;` |
|          - |  553 | `	/* Switch to the OS memory allocator */` |
|       3913 |  554 | `	pBackend->pMethods = &sOSAllocMethods;` |
|       3913 |  555 | `	if( pBackend->pMethods->xInit ){` |
|          - |  556 | `		/* Initialize the backend  */` |
|        ! 0 |  557 | `		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){` |
|        ! 0 |  558 | `			return SXERR_ABORT;` |
|          - |  559 | `		}` |
|        ! 0 |  560 | `	}` |
|          - |  561 | `#if defined(UNTRUST)` |
|          - |  562 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|          - |  563 | `#endif` |
|       3913 |  564 | `	return SXRET_OK;` |
|       1959 |  565 | `}` |
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
|       7812 |  594 | `PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend *pBackend,SyMemBackend *pParent)` |
|          5 |  595 | `{` |
|          - |  596 | `	sxu8 bInheritMutex;` |
|          - |  597 | `#if defined(UNTRUST)` |
|          - |  598 | `	if( pBackend == 0 \|\| SXMEM_BACKEND_CORRUPT(pParent) ){` |
|          - |  599 | `		return SXERR_CORRUPT;` |
|          - |  600 | `	}` |
|          - |  601 | `#endif` |
|          - |  602 | `	/* Zero the allocator first */` |
|       7817 |  603 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|       7817 |  604 | `	pBackend->pMethods  = pParent->pMethods;` |
|       7817 |  605 | `	pBackend->xMemError = pParent->xMemError;` |
|       7817 |  606 | `	pBackend->pUserData = pParent->pUserData;` |
|       7817 |  607 | `	pBackend->nMaxRequest = pParent->nMaxRequest;` |
|       7817 |  608 | `	bInheritMutex = pParent->pMutexMethods ? TRUE : FALSE;` |
|       7817 |  609 | `	if( bInheritMutex ){` |
|       3913 |  610 | `		pBackend->pMutexMethods = pParent->pMutexMethods;` |
|          - |  611 | `		/* Create a private mutex */` |
|       3913 |  612 | `		pBackend->pMutex = pBackend->pMutexMethods->xNew(SXMUTEX_TYPE_FAST);` |
|       3913 |  613 | `		if( pBackend->pMutex ==  0){` |
|        ! 0 |  614 | `			return SXERR_OS;` |
|          - |  615 | `		}` |
|       1954 |  616 | `	}` |
|          - |  617 | `#if defined(UNTRUST)` |
|          - |  618 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|          - |  619 | `#endif` |
|       7817 |  620 | `	return SXRET_OK;` |
|       3911 |  621 | `}` |
|       8200 |  622 | `static sxi32 MemBackendRelease(SyMemBackend *pBackend)` |
|          5 |  623 | `{` |
|          - |  624 | `	SyMemBlock *pBlock,*pNext;` |
|          - |  625 |  |
|       8205 |  626 | `	pBlock = pBackend->pBlocks;` |
|    2568204 |  627 | `	for(;;){` |
|    5136413 |  628 | `		if( pBackend->nBlock == 0 ){` |
|       1950 |  629 | `			break;` |
|          - |  630 | `		}` |
|    5134467 |  631 | `		pNext  = pBlock->pNext;` |
|    5134467 |  632 | `		pBackend->pMethods->xFree(pBlock);` |
|    5134467 |  633 | `		pBlock = pNext;` |
|    5134467 |  634 | `		pBackend->nBlock--;` |
|          - |  635 | `		/* LOOP ONE */` |
|    5134467 |  636 | `		if( pBackend->nBlock == 0 ){` |
|       4763 |  637 | `			break;` |
|          - |  638 | `		}` |
|    5129709 |  639 | `		pNext  = pBlock->pNext;` |
|    5129709 |  640 | `		pBackend->pMethods->xFree(pBlock);` |
|    5129709 |  641 | `		pBlock = pNext;` |
|    5129709 |  642 | `		pBackend->nBlock--;` |
|          - |  643 | `		/* LOOP TWO */` |
|    5129709 |  644 | `		if( pBackend->nBlock == 0 ){` |
|        879 |  645 | `			break;` |
|          - |  646 | `		}` |
|    5128835 |  647 | `		pNext  = pBlock->pNext;` |
|    5128835 |  648 | `		pBackend->pMethods->xFree(pBlock);` |
|    5128835 |  649 | `		pBlock = pNext;` |
|    5128835 |  650 | `		pBackend->nBlock--;` |
|          - |  651 | `		/* LOOP THREE */` |
|    5128835 |  652 | `		if( pBackend->nBlock == 0 ){` |
|        626 |  653 | `			break;` |
|          - |  654 | `		}` |
|    5128213 |  655 | `		pNext  = pBlock->pNext;` |
|    5128213 |  656 | `		pBackend->pMethods->xFree(pBlock);` |
|    5128213 |  657 | `		pBlock = pNext;` |
|    5128213 |  658 | `		pBackend->nBlock--;` |
|          - |  659 | `		/* LOOP FOUR */` |
|          5 |  660 | `	}` |
|       8205 |  661 | `	if( pBackend->pMethods->xRelease ){` |
|        ! 0 |  662 | `		pBackend->pMethods->xRelease(pBackend->pMethods->pUserData);` |
|        ! 0 |  663 | `	}` |
|       8205 |  664 | `	pBackend->pMethods = 0;` |
|       8205 |  665 | `	pBackend->pBlocks  = 0;` |
|          - |  666 | `#if defined(UNTRUST)` |
|          - |  667 | `	pBackend->nMagic = 0x2626;` |
|          - |  668 | `#endif` |
|       8205 |  669 | `	return SXRET_OK;` |
|          5 |  670 | `}` |
|       8200 |  671 | `PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend *pBackend)` |
|          5 |  672 | `{` |
|          - |  673 | `#if defined(UNTRUST)` |
|          - |  674 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|          - |  675 | `		return SXERR_INVALID;` |
|          - |  676 | `	}` |
|          - |  677 | `#endif` |
|       8205 |  678 | `	if( pBackend->pMutexMethods ){` |
|        392 |  679 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|        194 |  680 | `	}` |
|       8205 |  681 | `	(void)MemBackendRelease(&(*pBackend));` |
|       8205 |  682 | `	if( pBackend->pMutexMethods ){` |
|        392 |  683 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|        392 |  684 | `		SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|        194 |  685 | `	}` |
|       8205 |  686 | `	return SXRET_OK;` |
|          5 |  687 | `}` |
|     780126 |  688 | `PH7_PRIVATE void * SyMemBackendDup(SyMemBackend *pBackend,const void *pSrc,sxu32 nSize)` |
|          5 |  689 | `{` |
|          - |  690 | `	void *pNew;` |
|          - |  691 | `#if defined(UNTRUST)` |
|          - |  692 | `	if( pSrc == 0 \|\| nSize <= 0 ){` |
|          - |  693 | `		return 0;` |
|          - |  694 | `	}` |
|          - |  695 | `#endif` |
|     780131 |  696 | `	pNew = SyMemBackendAlloc(&(*pBackend),nSize);` |
|     780131 |  697 | `	if( pNew ){` |
|     780131 |  698 | `		SyMemcpy(pSrc,pNew,nSize);` |
|     390063 |  699 | `	}` |
|     780131 |  700 | `	return pNew;` |
|          5 |  701 | `}` |
|    7293040 |  702 | `PH7_PRIVATE char * SyMemBackendStrDup(SyMemBackend *pBackend,const char *zSrc,sxu32 nSize)` |
|          5 |  703 | `{` |
|          - |  704 | `	char *zDest;` |
|    7293045 |  705 | `	zDest = (char *)SyMemBackendAlloc(&(*pBackend),nSize + 1);` |
|    7293045 |  706 | `	if( zDest ){` |
|    7293045 |  707 | `		Systrcpy(zDest,nSize+1,zSrc,nSize);` |
|    3646520 |  708 | `	}` |
|    7293045 |  709 | `	return zDest;` |
|          5 |  710 | `}` |
|    2346728 |  711 | `PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize)` |
|          5 |  712 | `{` |
|          - |  713 | `#if defined(UNTRUST)` |
|          - |  714 | `	if( pBlob == 0 \|\| pBuffer == 0 \|\| nSize < 1 ){` |
|          - |  715 | `		return SXERR_EMPTY;` |
|          - |  716 | `	}` |
|          - |  717 | `#endif` |
|    2346733 |  718 | `	pBlob->pBlob = pBuffer;` |
|    2346733 |  719 | `	pBlob->mByte = nSize;` |
|    2346733 |  720 | `	pBlob->nByte = 0;` |
|    2346733 |  721 | `	pBlob->pAllocator = 0;` |
|    2346733 |  722 | `	pBlob->nFlags = SXBLOB_LOCKED\|SXBLOB_STATIC;` |
|    2346733 |  723 | `	return SXRET_OK;` |
|          5 |  724 | `}` |
|   23999305 |  725 | `PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator)` |
|          5 |  726 | `{` |
|          - |  727 | `#if defined(UNTRUST)` |
|          - |  728 | `	if( pBlob == 0  ){` |
|          - |  729 | `		return SXERR_EMPTY;` |
|          - |  730 | `	}` |
|          - |  731 | `#endif` |
|   23999310 |  732 | `	pBlob->pBlob = 0;` |
|   23999310 |  733 | `	pBlob->mByte = pBlob->nByte	= 0;` |
|   23999310 |  734 | `	pBlob->pAllocator = &(*pAllocator);` |
|   23999310 |  735 | `	pBlob->nFlags = 0;` |
|   23999310 |  736 | `	return SXRET_OK;` |
|          5 |  737 | `}` |
|    3867142 |  738 | `PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte)` |
|          5 |  739 | `{` |
|          - |  740 | `#if defined(UNTRUST)` |
|          - |  741 | `	if( pBlob == 0  ){` |
|          - |  742 | `		return SXERR_EMPTY;` |
|          - |  743 | `	}` |
|          - |  744 | `#endif` |
|    3867147 |  745 | `	pBlob->pBlob = (void *)pData;` |
|    3867147 |  746 | `	pBlob->nByte = nByte;` |
|    3867147 |  747 | `	pBlob->mByte = 0;` |
|    3867147 |  748 | `	pBlob->nFlags \|= SXBLOB_RDONLY;` |
|    3867147 |  749 | `	return SXRET_OK;` |
|          5 |  750 | `}` |
|          - |  751 | `#ifndef SXBLOB_MIN_GROWTH` |
|          - |  752 | `#define SXBLOB_MIN_GROWTH 16` |
|          - |  753 | `#endif` |
|   31601878 |  754 | `static sxi32 BlobPrepareGrow(SyBlob *pBlob,sxu32 *pByte)` |
|          5 |  755 | `{` |
|          - |  756 | `	sxu32 nByte;` |
|          - |  757 | `	void *pNew;` |
|   31601883 |  758 | `	nByte = *pByte;` |
|   31601883 |  759 | `	if( pBlob->nFlags & (SXBLOB_LOCKED\|SXBLOB_STATIC) ){` |
|   18779039 |  760 | `		if ( SyBlobFreeSpace(pBlob) < nByte ){` |
|        ! 0 |  761 | `			*pByte = SyBlobFreeSpace(pBlob);` |
|        ! 0 |  762 | `			if( (*pByte) == 0 ){` |
|        ! 0 |  763 | `				return SXERR_SHORT;` |
|          - |  764 | `			}` |
|        ! 0 |  765 | `		}` |
|   18779039 |  766 | `		return SXRET_OK;` |
|          - |  767 | `	}` |
|   12822849 |  768 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|          - |  769 | `		/* Make a copy of the read-only item */` |
|     780131 |  770 | `		if( pBlob->nByte > 0 ){` |
|     780131 |  771 | `			pNew = SyMemBackendDup(pBlob->pAllocator,pBlob->pBlob,pBlob->nByte);` |
|     780131 |  772 | `			if( pNew == 0 ){` |
|        ! 0 |  773 | `				return SXERR_MEM;` |
|          - |  774 | `			}` |
|     780131 |  775 | `			pBlob->pBlob = pNew;` |
|     780131 |  776 | `			pBlob->mByte = pBlob->nByte;` |
|     390068 |  777 | `		}else{` |
|        ! 0 |  778 | `			pBlob->pBlob = 0;` |
|        ! 0 |  779 | `			pBlob->mByte = 0;` |
|          - |  780 | `		}` |
|          - |  781 | `		/* Remove the read-only flag */` |
|     780131 |  782 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|     390063 |  783 | `	}` |
|   12822849 |  784 | `	if( SyBlobFreeSpace(pBlob) >= nByte ){` |
|    2030118 |  785 | `		return SXRET_OK;` |
|          - |  786 | `	}` |
|   10792736 |  787 | `	if( pBlob->mByte > 0 ){` |
|     950169 |  788 | `		nByte = nByte + pBlob->mByte * 2 + SXBLOB_MIN_GROWTH;` |
|   10318604 |  789 | `	}else if ( nByte < SXBLOB_MIN_GROWTH ){` |
|    7385436 |  790 | `		nByte = SXBLOB_MIN_GROWTH;` |
|    3692617 |  791 | `	}` |
|   10792736 |  792 | `	pNew = SyMemBackendRealloc(pBlob->pAllocator,pBlob->pBlob,nByte);` |
|   10792736 |  793 | `	if( pNew == 0 ){` |
|        ! 0 |  794 | `		return SXERR_MEM;` |
|          - |  795 | `	}` |
|   10792736 |  796 | `	pBlob->pBlob = pNew;` |
|   10792736 |  797 | `	pBlob->mByte = nByte;` |
|   10792736 |  798 | `	return SXRET_OK;` |
|   15801029 |  799 | `}` |
|   31668130 |  800 | `PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize)` |
|          5 |  801 | `{` |
|          - |  802 | `	sxu8 *zBlob;` |
|          - |  803 | `	sxi32 rc;` |
|   31668135 |  804 | `	if( nSize < 1 ){` |
|      66257 |  805 | `		return SXRET_OK;` |
|          - |  806 | `	}` |
|   31601883 |  807 | `	rc = BlobPrepareGrow(&(*pBlob),&nSize);` |
|   31601883 |  808 | `	if( SXRET_OK != rc ){` |
|        ! 0 |  809 | `		return rc;` |
|          - |  810 | `	}` |
|   31601883 |  811 | `	if( pData ){` |
|   31601835 |  812 | `		zBlob = (sxu8 *)pBlob->pBlob ;` |
|   31601835 |  813 | `		zBlob = &zBlob[pBlob->nByte];` |
|   31601835 |  814 | `		pBlob->nByte += nSize;` |
|  102094946 |  815 | `		SX_MACRO_FAST_MEMCPY(pData,zBlob,nSize);` |
|   15801000 |  816 | `	}` |
|   31601883 |  817 | `	return SXRET_OK;` |
|   15834155 |  818 | `}` |
|     790471 |  819 | `PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob)` |
|          5 |  820 | `{` |
|          - |  821 | `	sxi32 rc;` |
|          - |  822 | `	sxu32 n;` |
|     790476 |  823 | `	n = pBlob->nByte;` |
|     790476 |  824 | `	rc = SyBlobAppend(&(*pBlob),(const void *)"\0",sizeof(char));` |
|     790476 |  825 | `	if (rc == SXRET_OK ){` |
|     790476 |  826 | `		pBlob->nByte = n;` |
|     395278 |  827 | `	}` |
|     790476 |  828 | `	return rc;` |
|          5 |  829 | `}` |
|    4213836 |  830 | `PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest)` |
|          5 |  831 | `{` |
|    4213841 |  832 | `	sxi32 rc = SXRET_OK;` |
|          - |  833 | `#ifdef UNTRUST` |
|          - |  834 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|          - |  835 | `		return SXERR_EMPTY;` |
|          - |  836 | `	}` |
|          - |  837 | `#endif` |
|    4213841 |  838 | `	if( pSrc->nByte > 0 ){` |
|    4077005 |  839 | `		rc = SyBlobAppend(&(*pDest),pSrc->pBlob,pSrc->nByte);` |
|    2038500 |  840 | `	}` |
|    4213841 |  841 | `	return rc;` |
|          5 |  842 | `}` |
|        ! 0 |  843 | `PH7_PRIVATE sxi32 SyBlobCmp(SyBlob *pLeft,SyBlob *pRight)` |
|        ! 0 |  844 | `{` |
|          - |  845 | `	sxi32 rc;` |
|          - |  846 | `#ifdef UNTRUST` |
|          - |  847 | `	if( pLeft == 0 \|\| pRight == 0 ){` |
|          - |  848 | `		return pLeft ? 1 : -1;` |
|          - |  849 | `	}` |
|          - |  850 | `#endif` |
|        ! 0 |  851 | `	if( pLeft->nByte != pRight->nByte ){` |
|          - |  852 | `		/* Length differ */` |
|        ! 0 |  853 | `		return pLeft->nByte - pRight->nByte;` |
|          - |  854 | `	}` |
|        ! 0 |  855 | `	if( pLeft->nByte == 0 ){` |
|        ! 0 |  856 | `		return 0;` |
|          - |  857 | `	}` |
|          - |  858 | `	/* Perform a standard memcmp() operation */` |
|        ! 0 |  859 | `	rc = SyMemcmp(pLeft->pBlob,pRight->pBlob,pLeft->nByte);` |
|        ! 0 |  860 | `	return rc;` |
|        ! 0 |  861 | `}` |
|    9380536 |  862 | `PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob)` |
|          5 |  863 | `{` |
|    9380541 |  864 | `	pBlob->nByte = 0;` |
|    9380541 |  865 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|       5355 |  866 | `		pBlob->pBlob = 0;` |
|       5355 |  867 | `		pBlob->mByte = 0;` |
|       5355 |  868 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|       2675 |  869 | `	}` |
|    9380541 |  870 | `	return SXRET_OK;` |
|          5 |  871 | `}` |
|   19254125 |  872 | `PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob)` |
|          5 |  873 | `{` |
|   19254130 |  874 | `	if( (pBlob->nFlags & (SXBLOB_STATIC\|SXBLOB_RDONLY)) == 0 && pBlob->mByte > 0 ){` |
|    6906336 |  875 | `		SyMemBackendFree(pBlob->pAllocator,pBlob->pBlob);` |
|    3453208 |  876 | `	}` |
|   19254130 |  877 | `	pBlob->pBlob = 0;` |
|   19254130 |  878 | `	pBlob->nByte = pBlob->mByte = 0;` |
|   19254130 |  879 | `	pBlob->nFlags = 0;` |
|   19254130 |  880 | `	return SXRET_OK;` |
|          5 |  881 | `}` |
|          - |  882 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     192342 |  883 | `PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft)` |
|          5 |  884 | `{` |
|     192347 |  885 | `	const char *zIn = (const char *)pBlob;` |
|          - |  886 | `	const char *zEnd;` |
|          - |  887 | `	sxi32 rc;` |
|     192347 |  888 | `	if( pLen > nLen ){` |
|       6625 |  889 | `		return SXERR_NOTFOUND;` |
|          - |  890 | `	}` |
|     185727 |  891 | `	zEnd = &zIn[nLen-pLen];` |
|    1677763 |  892 | `	for(;;){` |
|    3353699 |  893 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|    3308876 |  894 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|    3242957 |  895 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|    3202827 |  896 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|          5 |  897 | `	}` |
|      30727 |  898 | `	return SXERR_NOTFOUND;` |
|      96176 |  899 | `}` |
|          - |  900 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|          - |  901 |  |
