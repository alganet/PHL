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
|   129925163 |   18 | `static void * SyOSHeapAlloc(sxu32 nByte)` |
|           5 |   19 | `{` |
|           - |   20 | `	void *pNew;` |
|           - |   21 | `#if defined(__WINNT__)` |
|           5 |   22 | `	pNew = HeapAlloc(GetProcessHeap(),0,nByte);` |
|           - |   23 | `#else` |
|   129925163 |   24 | `	pNew = malloc((size_t)nByte);` |
|           - |   25 | `#endif` |
|   129925168 |   26 | `	return pNew;` |
|           5 |   27 | `}` |
|     5597530 |   28 | `static void * SyOSHeapRealloc(void *pOld,sxu32 nByte)` |
|           5 |   29 | `{` |
|           - |   30 | `	void *pNew;` |
|           - |   31 | `#if defined(__WINNT__)` |
|           5 |   32 | `	pNew = HeapReAlloc(GetProcessHeap(),0,pOld,nByte);` |
|           - |   33 | `#else` |
|     5597530 |   34 | `	pNew = realloc(pOld,(size_t)nByte);` |
|           - |   35 | `#endif` |
|     5597535 |   36 | `	return pNew;` |
|           5 |   37 | `}` |
|   129921515 |   38 | `static void SyOSHeapFree(void *pPtr)` |
|           5 |   39 | `{` |
|           - |   40 | `#if defined(__WINNT__)` |
|           5 |   41 | `	HeapFree(GetProcessHeap(),0,pPtr);` |
|           - |   42 | `#else` |
|   129921515 |   43 | `	free(pPtr);` |
|           - |   44 | `#endif` |
|   129921520 |   45 | `}` |
|           - |   46 |  |
|           - |   47 |  |
|   499906759 |   48 | `PH7_PRIVATE void SyZero(void *pSrc,sxu32 nSize)` |
|           5 |   49 | `{` |
|   499906764 |   50 | `	register unsigned char *zSrc = (unsigned char *)pSrc;` |
|           - |   51 | `	unsigned char *zEnd;` |
|           - |   52 | `#if defined(UNTRUST)` |
|           - |   53 | `	if( zSrc == 0 \|\| nSize <= 0 ){` |
|           - |   54 | `		return ;` |
|           - |   55 | `	}` |
|           - |   56 | `#endif` |
|   499906764 |   57 | `	zEnd = &zSrc[nSize];` |
|  6268203113 |   58 | `	for(;;){` |
| 12533808880 |   59 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 12033902645 |   60 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 12033902415 |   61 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
| 12033902187 |   62 | `		if( zSrc >= zEnd ){break;} zSrc[0] = 0; zSrc++;` |
|           5 |   63 | `	}` |
|   499906764 |   64 | `}` |
|   431795128 |   65 | `PH7_PRIVATE sxi32 SyMemcmp(const void *pB1,const void *pB2,sxu32 nSize)` |
|           5 |   66 | `{` |
|           - |   67 | `	sxi32 rc;` |
|   431795133 |   68 | `	if( nSize <= 0 ){` |
|       33999 |   69 | `		return 0;` |
|           - |   70 | `	}` |
|   431761139 |   71 | `	if( pB1 == 0 \|\| pB2 == 0 ){` |
|         ! 0 |   72 | `		return pB1 != 0 ? 1 : (pB2 == 0 ? 0 : -1);` |
|           - |   73 | `	}` |
|   527687300 |   74 | `	SX_MACRO_FAST_CMP(pB1,pB2,nSize,rc);` |
|   431761139 |   75 | `	return rc;` |
|   215902582 |   76 | `}` |
|    67535100 |   77 | `PH7_PRIVATE sxu32 SyMemcpy(const void *pSrc,void *pDest,sxu32 nLen)` |
|           5 |   78 | `{` |
|           - |   79 | `#if defined(UNTRUST)` |
|           - |   80 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|           - |   81 | `		return 0;` |
|           - |   82 | `	}` |
|           - |   83 | `#endif` |
|    67535105 |   84 | `	if( pSrc == (const void *)pDest ){` |
|         ! 0 |   85 | `		return nLen;` |
|           - |   86 | `	}` |
|   558734396 |   87 | `	SX_MACRO_FAST_MEMCPY(pSrc,pDest,nLen);` |
|    67535105 |   88 | `	return nLen;` |
|    33775065 |   89 | `}` |
|           - |   90 | `/* Size prefix stored ahead of every OS allocation. Padded to pointer size so` |
|           - |   91 | ` * the returned payload (and the SyMemBlock/SyMemHeader the backend lays on` |
|           - |   92 | ` * top of it) keeps the allocator's natural alignment — a bare sxu32 prefix` |
|           - |   93 | ` * left every chunk 4-misaligned on 64-bit platforms. */` |
|           - |   94 | `typedef union MemOSHeader MemOSHeader;` |
|           - |   95 | `union MemOSHeader {` |
|           - |   96 | `	sxu32 nBytes;` |
|           - |   97 | `	void *pAlign;` |
|           - |   98 | `};` |
|   129925163 |   99 | `static void * MemOSAlloc(sxu32 nBytes)` |
|           5 |  100 | `{` |
|           - |  101 | `	MemOSHeader *pChunk;` |
|   129925168 |  102 | `	pChunk = (MemOSHeader *)SyOSHeapAlloc(nBytes + sizeof(MemOSHeader));` |
|   129925168 |  103 | `	if( pChunk == 0 ){` |
|         ! 0 |  104 | `		return 0;` |
|           - |  105 | `	}` |
|   129925168 |  106 | `	pChunk->nBytes = nBytes;` |
|   129925168 |  107 | `	return (void *)&pChunk[1];` |
|    64966420 |  108 | `}` |
|     5597530 |  109 | `static void * MemOSRealloc(void *pOld,sxu32 nBytes)` |
|           5 |  110 | `{` |
|           - |  111 | `	MemOSHeader *pOldChunk;` |
|           - |  112 | `	MemOSHeader *pChunk;` |
|     5597535 |  113 | `	pOldChunk = (MemOSHeader *)(((char *)pOld)-sizeof(MemOSHeader));` |
|     5597535 |  114 | `	if( pOldChunk->nBytes >= nBytes ){` |
|         ! 0 |  115 | `		return pOld;` |
|           - |  116 | `	}` |
|     5597535 |  117 | `	pChunk = (MemOSHeader *)SyOSHeapRealloc(pOldChunk,nBytes + sizeof(MemOSHeader));` |
|     5597535 |  118 | `	if( pChunk == 0 ){` |
|         ! 0 |  119 | `		return 0;` |
|           - |  120 | `	}` |
|     5597535 |  121 | `	pChunk->nBytes = nBytes;` |
|     5597535 |  122 | `	return (void *)&pChunk[1];` |
|     2800209 |  123 | `}` |
|   129921515 |  124 | `static void MemOSFree(void *pBlock)` |
|           5 |  125 | `{` |
|           - |  126 | `	void *pChunk;` |
|   129921520 |  127 | `	pChunk = (void *)(((char *)pBlock)-sizeof(MemOSHeader));` |
|   129921520 |  128 | `	SyOSHeapFree(pChunk);` |
|   129921520 |  129 | `}` |
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
|   129925163 |  146 | `static void * MemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|           5 |  147 | `{` |
|           - |  148 | `	SyMemBlock *pBlock;` |
|   129925168 |  149 | `	sxi32 nRetry = 0;` |
|           - |  150 |  |
|           - |  151 | `	/* Append an extra block so we can tracks allocated chunks and avoid memory` |
|           - |  152 | `	 * leaks.` |
|           - |  153 | `	 */` |
|   129925168 |  154 | `	nByte += sizeof(SyMemBlock);` |
|           - |  155 | `	/* Enforce the optional per-allocation cap (0 = unlimited). A capped failure` |
|           - |  156 | `	 * returns NULL just like a genuine OS failure, driving the normal SXERR_MEM` |
|           - |  157 | `	 * propagation; the retry callback is intentionally skipped (hard limit). */` |
|   129925168 |  158 | `	if( pBackend->nMaxRequest && nByte > pBackend->nMaxRequest ){` |
|         ! 0 |  159 | `		return 0;` |
|           - |  160 | `	}` |
|    64966415 |  161 | `	for(;;){` |
|    64966420 |  162 | `		pBlock = (SyMemBlock *)pBackend->pMethods->xAlloc(nByte);` |
|   129925163 |  163 | `		if( pBlock != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY` |
|           5 |  164 | `			\|\| SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|    64966420 |  165 | `				break;` |
|           - |  166 | `		}` |
|         ! 0 |  167 | `		nRetry++;` |
|         ! 0 |  168 | `	}` |
|   129925168 |  169 | `	if( pBlock  == 0 ){` |
|         ! 0 |  170 | `		return 0;` |
|           - |  171 | `	}` |
|   129925168 |  172 | `	pBlock->pNext = pBlock->pPrev = 0;` |
|           - |  173 | `	/* Link to the list of already tracked blocks */` |
|   129925168 |  174 | `	MACRO_LD_PUSH(pBackend->pBlocks,pBlock);` |
|           - |  175 | `#if defined(UNTRUST)` |
|           - |  176 | `	pBlock->nGuard = SXMEM_BACKEND_MAGIC;` |
|           - |  177 | `#endif` |
|   129925168 |  178 | `	pBlock->nSize = nByte;` |
|   129925168 |  179 | `	pBackend->nMemUsed += nByte;` |
|   129925168 |  180 | `	if( pBackend->nMemUsed > pBackend->nMemPeak ){` |
|    35421609 |  181 | `		pBackend->nMemPeak = pBackend->nMemUsed;` |
|    17710779 |  182 | `	}` |
|   129925168 |  183 | `	pBackend->nBlock++;` |
|   129925168 |  184 | `	return (void *)&pBlock[1];` |
|    64966420 |  185 | `}` |
|    59113427 |  186 | `PH7_PRIVATE void * SyMemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|           5 |  187 | `{` |
|           - |  188 | `	void *pChunk;` |
|           - |  189 | `#if defined(UNTRUST)` |
|           - |  190 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|           - |  191 | `		return 0;` |
|           - |  192 | `	}` |
|           - |  193 | `#endif` |
|    59113432 |  194 | `	if( pBackend->pMutexMethods ){` |
|         ! 0 |  195 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|         ! 0 |  196 | `	}` |
|    59113432 |  197 | `	pChunk = MemBackendAlloc(&(*pBackend),nByte);` |
|    59113432 |  198 | `	if( pBackend->pMutexMethods ){` |
|         ! 0 |  199 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|         ! 0 |  200 | `	}` |
|    59113432 |  201 | `	return pChunk;` |
|           5 |  202 | `}` |
|    76120678 |  203 | `static void * MemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|           5 |  204 | `{` |
|           - |  205 | `	SyMemBlock *pBlock,*pNew,*pPrev,*pNext;` |
|    76120683 |  206 | `	sxu32 nRetry = 0;` |
|           - |  207 |  |
|    76120683 |  208 | `	if( pOld == 0 ){` |
|    70523153 |  209 | `		return MemBackendAlloc(&(*pBackend),nByte);` |
|           - |  210 | `	}` |
|     5597535 |  211 | `	pBlock = (SyMemBlock *)(((char *)pOld) - sizeof(SyMemBlock));` |
|           - |  212 | `#if defined(UNTRUST)` |
|           - |  213 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|           - |  214 | `		return 0;` |
|           - |  215 | `	}` |
|           - |  216 | `#endif` |
|     5597535 |  217 | `	nByte += sizeof(SyMemBlock);` |
|           - |  218 | `	/* Enforce the optional per-allocation cap (0 = unlimited); see MemBackendAlloc. */` |
|     5597535 |  219 | `	if( pBackend->nMaxRequest && nByte > pBackend->nMaxRequest ){` |
|         ! 0 |  220 | `		return 0;` |
|           - |  221 | `	}` |
|     5597535 |  222 | `	pPrev = pBlock->pPrev;` |
|     5597535 |  223 | `	pNext = pBlock->pNext;` |
|           - |  224 | `	{` |
|           - |  225 | `		/* Old size, captured before realloc may move/free the block; the` |
|           - |  226 | `		 * live-byte counter is adjusted by the delta only on success below. */` |
|     5597535 |  227 | `		sxu32 nOld = pBlock->nSize;` |
|     2800204 |  228 | `	for(;;){` |
|     2800209 |  229 | `		pNew = (SyMemBlock *)pBackend->pMethods->xRealloc(pBlock,nByte);` |
|     5597535 |  230 | `		if( pNew != 0 \|\| pBackend->xMemError == 0 \|\| nRetry > SXMEM_BACKEND_RETRY \|\|` |
|         ! 0 |  231 | `			SXERR_RETRY != pBackend->xMemError(pBackend->pUserData) ){` |
|     2800209 |  232 | `				break;` |
|           - |  233 | `		}` |
|         ! 0 |  234 | `		nRetry++;` |
|         ! 0 |  235 | `	}` |
|     5597535 |  236 | `	if( pNew == 0 ){` |
|         ! 0 |  237 | `		return 0;` |
|           - |  238 | `	}` |
|     5597535 |  239 | `	if( pNew != pBlock ){` |
|     4902492 |  240 | `		if( pPrev == 0 ){` |
|     2201693 |  241 | `			pBackend->pBlocks = pNew;` |
|     1180718 |  242 | `		}else{` |
|     2700804 |  243 | `			pPrev->pNext = pNew;` |
|           - |  244 | `		}` |
|     4902492 |  245 | `		if( pNext ){` |
|     4902478 |  246 | `			pNext->pPrev = pNew;` |
|     2775333 |  247 | `		}` |
|           - |  248 | `#if defined(UNTRUST)` |
|           - |  249 | `		pNew->nGuard = SXMEM_BACKEND_MAGIC;` |
|           - |  250 | `#endif` |
|     2775341 |  251 | `	}` |
|           - |  252 | `	/* Apply the size delta to the live-byte counter (underflow-guarded). */` |
|     5597535 |  253 | `	pBackend->nMemUsed = (pBackend->nMemUsed >= nOld) ? (pBackend->nMemUsed - nOld) : 0;` |
|     5597535 |  254 | `	pBackend->nMemUsed += nByte;` |
|     5597535 |  255 | `	if( pBackend->nMemUsed > pBackend->nMemPeak ){` |
|     2617795 |  256 | `		pBackend->nMemPeak = pBackend->nMemUsed;` |
|     1308907 |  257 | `	}` |
|     5597535 |  258 | `	pNew->nSize = nByte;` |
|     5597535 |  259 | `	return (void *)&pNew[1];` |
|           - |  260 | `	}` |
|    38064691 |  261 | `}` |
|    76120678 |  262 | `PH7_PRIVATE void * SyMemBackendRealloc(SyMemBackend *pBackend,void * pOld,sxu32 nByte)` |
|           5 |  263 | `{` |
|           - |  264 | `	void *pChunk;` |
|           - |  265 | `#if defined(UNTRUST)` |
|           - |  266 | `	if( SXMEM_BACKEND_CORRUPT(pBackend)  ){` |
|           - |  267 | `		return 0;` |
|           - |  268 | `	}` |
|           - |  269 | `#endif` |
|    76120683 |  270 | `	if( pBackend->pMutexMethods ){` |
|         ! 0 |  271 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|         ! 0 |  272 | `	}` |
|    76120683 |  273 | `	pChunk = MemBackendRealloc(&(*pBackend),pOld,nByte);` |
|    76120683 |  274 | `	if( pBackend->pMutexMethods ){` |
|         ! 0 |  275 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|         ! 0 |  276 | `	}` |
|    76120683 |  277 | `	return pChunk;` |
|           5 |  278 | `}` |
|    99412799 |  279 | `static sxi32 MemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|           5 |  280 | `{` |
|           - |  281 | `	SyMemBlock *pBlock;` |
|    99412804 |  282 | `	pBlock = (SyMemBlock *)(((char *)pChunk) - sizeof(SyMemBlock));` |
|           - |  283 | `#if defined(UNTRUST)` |
|           - |  284 | `	if( pBlock->nGuard != SXMEM_BACKEND_MAGIC ){` |
|           - |  285 | `		return SXERR_CORRUPT;` |
|           - |  286 | `	}` |
|           - |  287 | `#endif` |
|           - |  288 | `	/* Unlink from the list of active blocks */` |
|    99412804 |  289 | `	if( pBackend->nBlock > 0 ){` |
|           - |  290 | `		/* Release the block */` |
|           - |  291 | `#if defined(UNTRUST)` |
|           - |  292 | `		/* Mark as stale block */` |
|           - |  293 | `		pBlock->nGuard = 0x635B;` |
|           - |  294 | `#endif` |
|    99412804 |  295 | `		MACRO_LD_REMOVE(pBackend->pBlocks,pBlock);` |
|    99412804 |  296 | `		pBackend->nBlock--;` |
|   149115370 |  297 | `		pBackend->nMemUsed = (pBackend->nMemUsed >= pBlock->nSize)` |
|    99412799 |  298 | `			? (pBackend->nMemUsed - pBlock->nSize) : 0;` |
|    99412804 |  299 | `		pBackend->pMethods->xFree(pBlock);` |
|    49710233 |  300 | `	}` |
|    99412804 |  301 | `	return SXRET_OK;` |
|           5 |  302 | `}` |
|    99412799 |  303 | `PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend *pBackend,void * pChunk)` |
|           5 |  304 | `{` |
|           - |  305 | `	sxi32 rc;` |
|           - |  306 | `#if defined(UNTRUST)` |
|           - |  307 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|           - |  308 | `		return SXERR_CORRUPT;` |
|           - |  309 | `	}` |
|           - |  310 | `#endif` |
|    99412804 |  311 | `	if( pChunk == 0 ){` |
|         ! 0 |  312 | `		return SXRET_OK;` |
|           - |  313 | `	}` |
|    99412804 |  314 | `	if( pBackend->pMutexMethods ){` |
|         ! 0 |  315 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|         ! 0 |  316 | `	}` |
|    99412804 |  317 | `	rc = MemBackendFree(&(*pBackend),pChunk);` |
|    99412804 |  318 | `	if( pBackend->pMutexMethods ){` |
|         ! 0 |  319 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|         ! 0 |  320 | `	}` |
|    99412804 |  321 | `	return rc;` |
|    49710238 |  322 | `}` |
|           - |  323 | `#if defined(PH7_ENABLE_THREADS)` |
|        4142 |  324 | `PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods)` |
|           5 |  325 | `{` |
|           - |  326 | `	SyMutex *pMutex;` |
|           - |  327 | `#if defined(UNTRUST)` |
|           - |  328 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pMethods == 0 \|\| pMethods->xNew == 0){` |
|           - |  329 | `		return SXERR_CORRUPT;` |
|           - |  330 | `	}` |
|           - |  331 | `#endif` |
|        4147 |  332 | `	pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);` |
|        4147 |  333 | `	if( pMutex == 0 ){` |
|         ! 0 |  334 | `		return SXERR_OS;` |
|           - |  335 | `	}` |
|           - |  336 | `	/* Attach the mutex to the memory backend */` |
|        4147 |  337 | `	pBackend->pMutex = pMutex;` |
|        4147 |  338 | `	pBackend->pMutexMethods = pMethods;` |
|        4147 |  339 | `	return SXRET_OK;` |
|        2076 |  340 | `}` |
|        4142 |  341 | `PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend)` |
|           5 |  342 | `{` |
|           - |  343 | `#if defined(UNTRUST)` |
|           - |  344 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|           - |  345 | `		return SXERR_CORRUPT;` |
|           - |  346 | `	}` |
|           - |  347 | `#endif` |
|        4147 |  348 | `	if( pBackend->pMutex == 0 ){` |
|           - |  349 | `		/* There is no mutex subsystem at all */` |
|         ! 0 |  350 | `		return SXRET_OK;` |
|           - |  351 | `	}` |
|        4147 |  352 | `	SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|        4147 |  353 | `	pBackend->pMutexMethods = 0;` |
|        4147 |  354 | `	pBackend->pMutex = 0;` |
|        4147 |  355 | `	return SXRET_OK;` |
|        2076 |  356 | `}` |
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
|      288588 |  375 | `static sxi32 MemPoolBucketAlloc(SyMemBackend *pBackend,sxu32 nBucket)` |
|           5 |  376 | `{` |
|           - |  377 | `	char *zBucket,*zBucketEnd;` |
|           - |  378 | `	SyMemHeader *pHeader;` |
|           - |  379 | `	sxu32 nBucketSize;` |
|           - |  380 |  |
|           - |  381 | `	/* Allocate one big block first */` |
|      288593 |  382 | `	zBucket = (char *)MemBackendAlloc(&(*pBackend),SXMEM_POOL_MAXALLOC);` |
|      288593 |  383 | `	if( zBucket == 0 ){` |
|         ! 0 |  384 | `		return SXERR_MEM;` |
|           - |  385 | `	}` |
|      288593 |  386 | `	zBucketEnd = &zBucket[SXMEM_POOL_MAXALLOC];` |
|           - |  387 | `	/* Divide the big block into mini bucket pool */` |
|      288593 |  388 | `	nBucketSize = 1 << (nBucket + SXMEM_POOL_INCR);` |
|      288593 |  389 | `	pBackend->apPool[nBucket] = pHeader = (SyMemHeader *)zBucket;` |
|    28845432 |  390 | `	for(;;){` |
|    57690869 |  391 | `		if( &zBucket[nBucketSize] >= zBucketEnd ){` |
|      288593 |  392 | `			break;` |
|           - |  393 | `		}` |
|    57402281 |  394 | `		pHeader->pNext = (SyMemHeader *)&zBucket[nBucketSize];` |
|           - |  395 | `		/* Advance the cursor to the next available chunk */` |
|    57402281 |  396 | `		pHeader = pHeader->pNext;` |
|    57402281 |  397 | `		zBucket += nBucketSize;` |
|           5 |  398 | `	}` |
|      288593 |  399 | `	pHeader->pNext = 0;` |
|           - |  400 |  |
|      288593 |  401 | `	return SXRET_OK;` |
|      144299 |  402 | `}` |
|   212466798 |  403 | `static void * MemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|           5 |  404 | `{` |
|           - |  405 | `	SyMemHeader *pBucket,*pNext;` |
|           - |  406 | `	sxu32 nBucketSize;` |
|           - |  407 | `	sxu32 nBucket;` |
|           - |  408 |  |
|           - |  409 | `	/* SXMEM_POOL_BYPASS (sanitizer builds): force the big-block path for every` |
|           - |  410 | `	 * request so there is no bucket recycling and ASan tracks each object's` |
|           - |  411 | `	 * real lifetime. Chunks are freed through MemBackendPoolFree's big-block` |
|           - |  412 | `	 * branch either way — one copy of the alloc+tag logic. */` |
|   212466803 |  413 | `	if( SXMEM_POOL_BYPASS_ACTIVE \|\| nByte + sizeof(SyMemHeader) >= SXMEM_POOL_MAXALLOC ){` |
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
|   212466803 |  424 | `	nBucket = 0;` |
|   212466803 |  425 | `	nBucketSize = SXMEM_POOL_MINALLOC;` |
|  1080501473 |  426 | `	while( nByte + sizeof(SyMemHeader) > nBucketSize  ){` |
|   868034675 |  427 | `		nBucketSize <<= 1;` |
|   868034675 |  428 | `		nBucket++;` |
|           5 |  429 | `	}` |
|   212466803 |  430 | `	pBucket = pBackend->apPool[nBucket];` |
|   212466803 |  431 | `	if( pBucket == 0 ){` |
|           - |  432 | `		sxi32 rc;` |
|      288593 |  433 | `		rc = MemPoolBucketAlloc(&(*pBackend),nBucket);` |
|      288593 |  434 | `		if( rc != SXRET_OK ){` |
|         ! 0 |  435 | `			return 0;` |
|           - |  436 | `		}` |
|      288593 |  437 | `		pBucket = pBackend->apPool[nBucket];` |
|      144294 |  438 | `	}` |
|           - |  439 | `	/* Remove from the free list */` |
|   212466803 |  440 | `	pNext = pBucket->pNext;` |
|   212466803 |  441 | `	pBackend->apPool[nBucket] = pNext;` |
|           - |  442 | `	/* Record bucket&magic number */` |
|   212466803 |  443 | `	pBucket->nBucket = (((sxu32)SXMEM_POOL_MAGIC << 16) \| nBucket);` |
|   212466803 |  444 | `	return (void *)&pBucket[1];` |
|   106235515 |  445 | `}` |
|   212466798 |  446 | `PH7_PRIVATE void * SyMemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte)` |
|           5 |  447 | `{` |
|           - |  448 | `	void *pChunk;` |
|           - |  449 | `#if defined(UNTRUST)` |
|           - |  450 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|           - |  451 | `		return 0;` |
|           - |  452 | `	}` |
|           - |  453 | `#endif` |
|   212466803 |  454 | `	if( pBackend->pMutexMethods ){` |
|        4147 |  455 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|        2071 |  456 | `	}` |
|   212466803 |  457 | `	pChunk = MemBackendPoolAlloc(&(*pBackend),nByte);` |
|   212466803 |  458 | `	if( pBackend->pMutexMethods ){` |
|        4147 |  459 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|        2071 |  460 | `	}` |
|   212466803 |  461 | `	return pChunk;` |
|           5 |  462 | `}` |
|   163254022 |  463 | `static sxi32 MemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|           5 |  464 | `{` |
|           - |  465 | `	SyMemHeader *pHeader;` |
|           - |  466 | `	sxu32 nBucket;` |
|           - |  467 | `	/* Get the corresponding bucket */` |
|   163254027 |  468 | `	pHeader = (SyMemHeader *)(((char *)pChunk) - sizeof(SyMemHeader));` |
|           - |  469 | `	/* Sanity check to avoid misuse */` |
|   163254027 |  470 | `	if( (pHeader->nBucket >> 16) != SXMEM_POOL_MAGIC ){` |
|           3 |  471 | `		return SXERR_CORRUPT;` |
|           - |  472 | `	}` |
|   163254025 |  473 | `	nBucket = pHeader->nBucket & 0xFFFF;` |
|   163254025 |  474 | `	if( nBucket == SXU16_HIGH ){` |
|           - |  475 | `		/* Free the big block */` |
|         ! 0 |  476 | `		MemBackendFree(&(*pBackend),pHeader);` |
|   163254025 |  477 | `	}else if( nBucket >= SXMEM_POOL_NBUCKETS + SXMEM_POOL_INCR ){` |
|           - |  478 | `		/* Corrupted or misused bucket index */` |
|         ! 0 |  479 | `		return SXERR_CORRUPT;` |
|         ! 0 |  480 | `	}else{` |
|           - |  481 | `		/* Return to the free list */` |
|   163254025 |  482 | `		pHeader->pNext = pBackend->apPool[nBucket];` |
|   163254025 |  483 | `		pBackend->apPool[nBucket] = pHeader;` |
|           - |  484 | `	}` |
|   163254025 |  485 | `	return SXRET_OK;` |
|    81629127 |  486 | `}` |
|   163254022 |  487 | `PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend *pBackend,void * pChunk)` |
|           5 |  488 | `{` |
|           - |  489 | `	sxi32 rc;` |
|           - |  490 | `#if defined(UNTRUST)` |
|           - |  491 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) \|\| pChunk == 0 ){` |
|           - |  492 | `		return SXERR_CORRUPT;` |
|           - |  493 | `	}` |
|           - |  494 | `#endif` |
|   163254027 |  495 | `	if( pBackend->pMutexMethods ){` |
|        3659 |  496 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|        1827 |  497 | `	}` |
|   163254027 |  498 | `	rc = MemBackendPoolFree(&(*pBackend),pChunk);` |
|   163254027 |  499 | `	if( pBackend->pMutexMethods ){` |
|        3659 |  500 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|        1827 |  501 | `	}` |
|   163254027 |  502 | `	return rc;` |
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
|        4142 |  561 | `PH7_PRIVATE sxi32 SyMemBackendInit(SyMemBackend *pBackend,ProcMemError xMemErr,void * pUserData)` |
|           5 |  562 | `{` |
|           - |  563 | `#if defined(UNTRUST)` |
|           - |  564 | `	if( pBackend == 0 ){` |
|           - |  565 | `		return SXERR_EMPTY;` |
|           - |  566 | `	}` |
|           - |  567 | `#endif` |
|           - |  568 | `	/* Zero the allocator first */` |
|        4147 |  569 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|        4147 |  570 | `	pBackend->xMemError = xMemErr;` |
|        4147 |  571 | `	pBackend->pUserData = pUserData;` |
|           - |  572 | `	/* Switch to the OS memory allocator */` |
|        4147 |  573 | `	pBackend->pMethods = &sOSAllocMethods;` |
|        4147 |  574 | `	if( pBackend->pMethods->xInit ){` |
|           - |  575 | `		/* Initialize the backend  */` |
|         ! 0 |  576 | `		if( SXRET_OK != pBackend->pMethods->xInit(pBackend->pMethods->pUserData) ){` |
|         ! 0 |  577 | `			return SXERR_ABORT;` |
|           - |  578 | `		}` |
|         ! 0 |  579 | `	}` |
|           - |  580 | `#if defined(UNTRUST)` |
|           - |  581 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|           - |  582 | `#endif` |
|        4147 |  583 | `	return SXRET_OK;` |
|        2076 |  584 | `}` |
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
|        8282 |  613 | `PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend *pBackend,SyMemBackend *pParent)` |
|           5 |  614 | `{` |
|           - |  615 | `	sxu8 bInheritMutex;` |
|           - |  616 | `#if defined(UNTRUST)` |
|           - |  617 | `	if( pBackend == 0 \|\| SXMEM_BACKEND_CORRUPT(pParent) ){` |
|           - |  618 | `		return SXERR_CORRUPT;` |
|           - |  619 | `	}` |
|           - |  620 | `#endif` |
|           - |  621 | `	/* Zero the allocator first */` |
|        8287 |  622 | `	SyZero(&(*pBackend),sizeof(SyMemBackend));` |
|        8287 |  623 | `	pBackend->pMethods  = pParent->pMethods;` |
|        8287 |  624 | `	pBackend->xMemError = pParent->xMemError;` |
|        8287 |  625 | `	pBackend->pUserData = pParent->pUserData;` |
|        8287 |  626 | `	pBackend->nMaxRequest = pParent->nMaxRequest;` |
|        8287 |  627 | `	bInheritMutex = pParent->pMutexMethods ? TRUE : FALSE;` |
|        8287 |  628 | `	if( bInheritMutex ){` |
|        4147 |  629 | `		pBackend->pMutexMethods = pParent->pMutexMethods;` |
|           - |  630 | `		/* Create a private mutex */` |
|        4147 |  631 | `		pBackend->pMutex = pBackend->pMutexMethods->xNew(SXMUTEX_TYPE_FAST);` |
|        4147 |  632 | `		if( pBackend->pMutex ==  0){` |
|         ! 0 |  633 | `			return SXERR_OS;` |
|           - |  634 | `		}` |
|        2071 |  635 | `	}` |
|           - |  636 | `#if defined(UNTRUST)` |
|           - |  637 | `	pBackend->nMagic = SXMEM_BACKEND_MAGIC;` |
|           - |  638 | `#endif` |
|        8287 |  639 | `	return SXRET_OK;` |
|        4146 |  640 | `}` |
|        8782 |  641 | `static sxi32 MemBackendRelease(SyMemBackend *pBackend)` |
|           5 |  642 | `{` |
|           - |  643 | `	SyMemBlock *pBlock,*pNext;` |
|           - |  644 |  |
|        8787 |  645 | `	pBlock = pBackend->pBlocks;` |
|     3816864 |  646 | `	for(;;){` |
|     7633733 |  647 | `		if( pBackend->nBlock == 0 ){` |
|        2036 |  648 | `			break;` |
|           - |  649 | `		}` |
|     7631701 |  650 | `		pNext  = pBlock->pNext;` |
|     7631701 |  651 | `		pBackend->pMethods->xFree(pBlock);` |
|     7631701 |  652 | `		pBlock = pNext;` |
|     7631701 |  653 | `		pBackend->nBlock--;` |
|           - |  654 | `		/* LOOP ONE */` |
|     7631701 |  655 | `		if( pBackend->nBlock == 0 ){` |
|        5235 |  656 | `			break;` |
|           - |  657 | `		}` |
|     7626471 |  658 | `		pNext  = pBlock->pNext;` |
|     7626471 |  659 | `		pBackend->pMethods->xFree(pBlock);` |
|     7626471 |  660 | `		pBlock = pNext;` |
|     7626471 |  661 | `		pBackend->nBlock--;` |
|           - |  662 | `		/* LOOP TWO */` |
|     7626471 |  663 | `		if( pBackend->nBlock == 0 ){` |
|         862 |  664 | `			break;` |
|           - |  665 | `		}` |
|     7625613 |  666 | `		pNext  = pBlock->pNext;` |
|     7625613 |  667 | `		pBackend->pMethods->xFree(pBlock);` |
|     7625613 |  668 | `		pBlock = pNext;` |
|     7625613 |  669 | `		pBackend->nBlock--;` |
|           - |  670 | `		/* LOOP THREE */` |
|     7625613 |  671 | `		if( pBackend->nBlock == 0 ){` |
|         666 |  672 | `			break;` |
|           - |  673 | `		}` |
|     7624951 |  674 | `		pNext  = pBlock->pNext;` |
|     7624951 |  675 | `		pBackend->pMethods->xFree(pBlock);` |
|     7624951 |  676 | `		pBlock = pNext;` |
|     7624951 |  677 | `		pBackend->nBlock--;` |
|           - |  678 | `		/* LOOP FOUR */` |
|           5 |  679 | `	}` |
|        8787 |  680 | `	if( pBackend->pMethods->xRelease ){` |
|         ! 0 |  681 | `		pBackend->pMethods->xRelease(pBackend->pMethods->pUserData);` |
|         ! 0 |  682 | `	}` |
|        8787 |  683 | `	pBackend->pMethods = 0;` |
|        8787 |  684 | `	pBackend->pBlocks  = 0;` |
|           - |  685 | `#if defined(UNTRUST)` |
|           - |  686 | `	pBackend->nMagic = 0x2626;` |
|           - |  687 | `#endif` |
|        8787 |  688 | `	return SXRET_OK;` |
|           5 |  689 | `}` |
|        8782 |  690 | `PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend *pBackend)` |
|           5 |  691 | `{` |
|           - |  692 | `#if defined(UNTRUST)` |
|           - |  693 | `	if( SXMEM_BACKEND_CORRUPT(pBackend) ){` |
|           - |  694 | `		return SXERR_INVALID;` |
|           - |  695 | `	}` |
|           - |  696 | `#endif` |
|        8787 |  697 | `	if( pBackend->pMutexMethods ){` |
|         498 |  698 | `		SyMutexEnter(pBackend->pMutexMethods,pBackend->pMutex);` |
|         247 |  699 | `	}` |
|        8787 |  700 | `	(void)MemBackendRelease(&(*pBackend));` |
|        8787 |  701 | `	if( pBackend->pMutexMethods ){` |
|         498 |  702 | `		SyMutexLeave(pBackend->pMutexMethods,pBackend->pMutex);` |
|         498 |  703 | `		SyMutexRelease(pBackend->pMutexMethods,pBackend->pMutex);` |
|         247 |  704 | `	}` |
|        8787 |  705 | `	return SXRET_OK;` |
|           5 |  706 | `}` |
|     1015905 |  707 | `PH7_PRIVATE void * SyMemBackendDup(SyMemBackend *pBackend,const void *pSrc,sxu32 nSize)` |
|           5 |  708 | `{` |
|           - |  709 | `	void *pNew;` |
|           - |  710 | `#if defined(UNTRUST)` |
|           - |  711 | `	if( pSrc == 0 \|\| nSize <= 0 ){` |
|           - |  712 | `		return 0;` |
|           - |  713 | `	}` |
|           - |  714 | `#endif` |
|     1015910 |  715 | `	pNew = SyMemBackendAlloc(&(*pBackend),nSize);` |
|     1015910 |  716 | `	if( pNew ){` |
|     1015910 |  717 | `		SyMemcpy(pSrc,pNew,nSize);` |
|      508440 |  718 | `	}` |
|     1015910 |  719 | `	return pNew;` |
|           5 |  720 | `}` |
|    11055774 |  721 | `PH7_PRIVATE char * SyMemBackendStrDup(SyMemBackend *pBackend,const char *zSrc,sxu32 nSize)` |
|           5 |  722 | `{` |
|           - |  723 | `	char *zDest;` |
|    11055779 |  724 | `	zDest = (char *)SyMemBackendAlloc(&(*pBackend),nSize + 1);` |
|    11055779 |  725 | `	if( zDest ){` |
|    11055779 |  726 | `		Systrcpy(zDest,nSize+1,zSrc,nSize);` |
|     5527887 |  727 | `	}` |
|    11055779 |  728 | `	return zDest;` |
|           5 |  729 | `}` |
|     3199892 |  730 | `PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize)` |
|           5 |  731 | `{` |
|           - |  732 | `#if defined(UNTRUST)` |
|           - |  733 | `	if( pBlob == 0 \|\| pBuffer == 0 \|\| nSize < 1 ){` |
|           - |  734 | `		return SXERR_EMPTY;` |
|           - |  735 | `	}` |
|           - |  736 | `#endif` |
|     3199897 |  737 | `	pBlob->pBlob = pBuffer;` |
|     3199897 |  738 | `	pBlob->mByte = nSize;` |
|     3199897 |  739 | `	pBlob->nByte = 0;` |
|     3199897 |  740 | `	pBlob->pAllocator = 0;` |
|     3199897 |  741 | `	pBlob->nFlags = SXBLOB_LOCKED\|SXBLOB_STATIC;` |
|     3199897 |  742 | `	return SXRET_OK;` |
|           5 |  743 | `}` |
|   294443891 |  744 | `PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator)` |
|           5 |  745 | `{` |
|           - |  746 | `#if defined(UNTRUST)` |
|           - |  747 | `	if( pBlob == 0  ){` |
|           - |  748 | `		return SXERR_EMPTY;` |
|           - |  749 | `	}` |
|           - |  750 | `#endif` |
|   294443896 |  751 | `	pBlob->pBlob = 0;` |
|   294443896 |  752 | `	pBlob->mByte = pBlob->nByte	= 0;` |
|   294443896 |  753 | `	pBlob->pAllocator = &(*pAllocator);` |
|   294443896 |  754 | `	pBlob->nFlags = 0;` |
|   294443896 |  755 | `	return SXRET_OK;` |
|           5 |  756 | `}` |
|    17795044 |  757 | `PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte)` |
|           5 |  758 | `{` |
|           - |  759 | `#if defined(UNTRUST)` |
|           - |  760 | `	if( pBlob == 0  ){` |
|           - |  761 | `		return SXERR_EMPTY;` |
|           - |  762 | `	}` |
|           - |  763 | `#endif` |
|    17795049 |  764 | `	pBlob->pBlob = (void *)pData;` |
|    17795049 |  765 | `	pBlob->nByte = nByte;` |
|    17795049 |  766 | `	pBlob->mByte = 0;` |
|    17795049 |  767 | `	pBlob->nFlags \|= SXBLOB_RDONLY;` |
|    17795049 |  768 | `	return SXRET_OK;` |
|           5 |  769 | `}` |
|           - |  770 | `#ifndef SXBLOB_MIN_GROWTH` |
|           - |  771 | `#define SXBLOB_MIN_GROWTH 16` |
|           - |  772 | `#endif` |
|    62225495 |  773 | `static sxi32 BlobPrepareGrow(SyBlob *pBlob,sxu32 *pByte)` |
|           5 |  774 | `{` |
|           - |  775 | `	sxu32 nByte;` |
|           - |  776 | `	void *pNew;` |
|    62225500 |  777 | `	nByte = *pByte;` |
|    62225500 |  778 | `	if( pBlob->nFlags & (SXBLOB_LOCKED\|SXBLOB_STATIC) ){` |
|    25196547 |  779 | `		if ( SyBlobFreeSpace(pBlob) < nByte ){` |
|         ! 0 |  780 | `			*pByte = SyBlobFreeSpace(pBlob);` |
|         ! 0 |  781 | `			if( (*pByte) == 0 ){` |
|         ! 0 |  782 | `				return SXERR_SHORT;` |
|           - |  783 | `			}` |
|         ! 0 |  784 | `		}` |
|    25196547 |  785 | `		return SXRET_OK;` |
|           - |  786 | `	}` |
|    37028958 |  787 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|           - |  788 | `		/* Make a copy of the read-only item */` |
|     1015830 |  789 | `		if( pBlob->nByte > 0 ){` |
|     1015830 |  790 | `			pNew = SyMemBackendDup(pBlob->pAllocator,pBlob->pBlob,pBlob->nByte);` |
|     1015830 |  791 | `			if( pNew == 0 ){` |
|         ! 0 |  792 | `				return SXERR_MEM;` |
|           - |  793 | `			}` |
|     1015830 |  794 | `			pBlob->pBlob = pNew;` |
|     1015830 |  795 | `			pBlob->mByte = pBlob->nByte;` |
|      508405 |  796 | `		}else{` |
|         ! 0 |  797 | `			pBlob->pBlob = 0;` |
|         ! 0 |  798 | `			pBlob->mByte = 0;` |
|           - |  799 | `		}` |
|           - |  800 | `		/* Remove the read-only flag */` |
|     1015830 |  801 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|      508400 |  802 | `	}` |
|    37028958 |  803 | `	if( SyBlobFreeSpace(pBlob) >= nByte ){` |
|     4551150 |  804 | `		return SXRET_OK;` |
|           - |  805 | `	}` |
|    32477813 |  806 | `	if( pBlob->mByte > 0 ){` |
|     1775885 |  807 | `		nByte = nByte + pBlob->mByte * 2 + SXBLOB_MIN_GROWTH;` |
|    31591312 |  808 | `	}else if ( nByte < SXBLOB_MIN_GROWTH ){` |
|    20809414 |  809 | `		nByte = SXBLOB_MIN_GROWTH;` |
|    10405427 |  810 | `	}` |
|    32477813 |  811 | `	pNew = SyMemBackendRealloc(pBlob->pAllocator,pBlob->pBlob,nByte);` |
|    32477813 |  812 | `	if( pNew == 0 ){` |
|         ! 0 |  813 | `		return SXERR_MEM;` |
|           - |  814 | `	}` |
|    32477813 |  815 | `	pBlob->pBlob = pNew;` |
|    32477813 |  816 | `	pBlob->mByte = nByte;` |
|    32477813 |  817 | `	return SXRET_OK;` |
|    31114847 |  818 | `}` |
|    62327123 |  819 | `PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize)` |
|           5 |  820 | `{` |
|           - |  821 | `	sxu8 *zBlob;` |
|           - |  822 | `	sxi32 rc;` |
|    62327128 |  823 | `	if( nSize < 1 ){` |
|      101633 |  824 | `		return SXRET_OK;` |
|           - |  825 | `	}` |
|    62225500 |  826 | `	rc = BlobPrepareGrow(&(*pBlob),&nSize);` |
|    62225500 |  827 | `	if( SXRET_OK != rc ){` |
|         ! 0 |  828 | `		return rc;` |
|           - |  829 | `	}` |
|    62225500 |  830 | `	if( pData ){` |
|    62225340 |  831 | `		zBlob = (sxu8 *)pBlob->pBlob ;` |
|    62225340 |  832 | `		zBlob = &zBlob[pBlob->nByte];` |
|    62225340 |  833 | `		pBlob->nByte += nSize;` |
|   288456416 |  834 | `		SX_MACRO_FAST_MEMCPY(pData,zBlob,nSize);` |
|    31114762 |  835 | `	}` |
|    62225500 |  836 | `	return SXRET_OK;` |
|    31165661 |  837 | `}` |
|     1079331 |  838 | `PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob)` |
|           5 |  839 | `{` |
|           - |  840 | `	sxi32 rc;` |
|           - |  841 | `	sxu32 n;` |
|     1079336 |  842 | `	n = pBlob->nByte;` |
|     1079336 |  843 | `	rc = SyBlobAppend(&(*pBlob),(const void *)"\0",sizeof(char));` |
|     1079336 |  844 | `	if (rc == SXRET_OK ){` |
|     1079336 |  845 | `		pBlob->nByte = n;` |
|      540268 |  846 | `	}` |
|     1079336 |  847 | `	return rc;` |
|           5 |  848 | `}` |
|     9997303 |  849 | `PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest)` |
|           5 |  850 | `{` |
|     9997308 |  851 | `	sxi32 rc = SXRET_OK;` |
|           - |  852 | `#ifdef UNTRUST` |
|           - |  853 | `	if( pSrc == 0 \|\| pDest == 0 ){` |
|           - |  854 | `		return SXERR_EMPTY;` |
|           - |  855 | `	}` |
|           - |  856 | `#endif` |
|     9997308 |  857 | `	if( pSrc->nByte > 0 ){` |
|     9804780 |  858 | `		rc = SyBlobAppend(&(*pDest),pSrc->pBlob,pSrc->nByte);` |
|     4903105 |  859 | `	}` |
|     9997308 |  860 | `	return rc;` |
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
|    22231914 |  881 | `PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob)` |
|           5 |  882 | `{` |
|    22231919 |  883 | `	pBlob->nByte = 0;` |
|    22231919 |  884 | `	if( pBlob->nFlags & SXBLOB_RDONLY ){` |
|        5873 |  885 | `		pBlob->pBlob = 0;` |
|        5873 |  886 | `		pBlob->mByte = 0;` |
|        5873 |  887 | `		pBlob->nFlags &= ~SXBLOB_RDONLY;` |
|        2934 |  888 | `	}` |
|    22231919 |  889 | `	return SXRET_OK;` |
|           5 |  890 | `}` |
|    94032060 |  891 | `PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob)` |
|           5 |  892 | `{` |
|    94032065 |  893 | `	if( (pBlob->nFlags & (SXBLOB_STATIC\|SXBLOB_RDONLY)) == 0 && pBlob->mByte > 0 ){` |
|    26195986 |  894 | `		SyMemBackendFree(pBlob->pAllocator,pBlob->pBlob);` |
|    13099809 |  895 | `	}` |
|    94032065 |  896 | `	pBlob->pBlob = 0;` |
|    94032065 |  897 | `	pBlob->nByte = pBlob->mByte = 0;` |
|    94032065 |  898 | `	pBlob->nFlags = 0;` |
|    94032065 |  899 | `	return SXRET_OK;` |
|           5 |  900 | `}` |
|           - |  901 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      214228 |  902 | `PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft)` |
|           5 |  903 | `{` |
|      214233 |  904 | `	const char *zIn = (const char *)pBlob;` |
|           - |  905 | `	const char *zEnd;` |
|           - |  906 | `	sxi32 rc;` |
|      214233 |  907 | `	if( pLen > nLen ){` |
|        6937 |  908 | `		return SXERR_NOTFOUND;` |
|           - |  909 | `	}` |
|      207301 |  910 | `	zEnd = &zIn[nLen-pLen];` |
|     1992514 |  911 | `	for(;;){` |
|     3982599 |  912 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|     3932141 |  913 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|     3859277 |  914 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|     3815373 |  915 | `		if( zIn > zEnd ){break;} SX_MACRO_FAST_CMP(zIn,pPattern,pLen,rc); if( rc == 0 ){ if( pOfft ){ *pOfft = (sxu32)(zIn - (const char *)pBlob);} return SXRET_OK; } zIn++;` |
|           5 |  916 | `	}` |
|       33041 |  917 | `	return SXERR_NOTFOUND;` |
|      107119 |  918 | `}` |
|           - |  919 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|           - |  920 |  |
