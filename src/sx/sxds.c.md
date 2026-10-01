# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 325/350 lines (92.86%)

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
|         - |   10 | `#include "sxhashtable.h"` |
|         - |   11 | `#include "sxhash.h"` |
|         - |   12 | `#include "sxstr.h"` |
|         - |   13 |  |
|         - |   14 | `/* Byte budget for a set's FIRST allocation -- see SySetPut. */` |
|         - |   15 | `#define SXSET_FIRST_BYTES 256` |
| 155315262 |   16 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   17 | `{` |
| 155315267 |   18 | `	pSet->nSize = 0 ;` |
| 155315267 |   19 | `	pSet->nUsed = 0;` |
| 155315267 |   20 | `	pSet->nCursor = 0;` |
| 155315267 |   21 | `	pSet->eSize = ElemSize;` |
| 155315267 |   22 | `	pSet->pAllocator = pAllocator;` |
| 155315267 |   23 | `	pSet->pBase =  0;` |
| 155315267 |   24 | `	pSet->pUserData = 0;` |
| 155315267 |   25 | `	return SXRET_OK;` |
|         5 |   26 | `}` |
|  83242091 |   27 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   28 | `{` |
|         - |   29 | `	unsigned char *zbase;` |
|  83242096 |   30 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   31 | `		void *pNew;` |
|         - |   32 | `		sxu32 nNew;` |
|  10195695 |   33 | `		if( pSet->pAllocator == 0 ){` |
|       ! 0 |   34 | `			return  SXERR_LOCKED;` |
|         - |   35 | `		}` |
|         - |   36 | `		/* The FIRST growth is sized in BYTES, not in slots. It used to be eight` |
|         - |   37 | ``		 * slots whatever they cost -- `nSize = 4` followed by the unconditional`` |
|         - |   38 | ``		 * `* 2` of the doubling step -- which is a generous opening bid for a`` |
|         - |   39 | `		 * container whose element is large and whose population is usually one or` |
|         - |   40 | `		 * two. The engine's biggest such set is a function's declared arguments` |
|         - |   41 | `		 * (192 bytes each): 1,679 of them were live at the peak of a 40-file lint` |
|         - |   42 | `		 * run, every one holding eight slots, 2.6 MB to describe a few thousand` |
|         - |   43 | `		 * parameters. A quarter-kilobyte opening keeps all eight for the small` |
|         - |   44 | `		 * elements that fill them -- an instruction is 32 bytes, a pointer is 8 --` |
|         - |   45 | `		 * and hands the large ones only what they are likely to use. Everything` |
|         - |   46 | `		 * after the first doubles, as before. */` |
|  10195695 |   47 | `		nNew = pSet->nSize;` |
|  10195695 |   48 | `		if( nNew > 0 ){` |
|    761148 |   49 | `			nNew *= 2;` |
|    379990 |   50 | `		}else{` |
|   9434552 |   51 | `			nNew = pSet->eSize > 0 ? SXSET_FIRST_BYTES / pSet->eSize : 8;` |
|   9434552 |   52 | `			if( nNew < 1 ){ nNew = 1; }` |
|   9434524 |   53 | `			else if( nNew > 8 ){ nNew = 8; }` |
|         - |   54 | `		}` |
|  10195695 |   55 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * nNew);` |
|  10195695 |   56 | `		if( pNew == 0 ){` |
|       ! 0 |   57 | `			return SXERR_MEM;` |
|         - |   58 | `		}` |
|  10195695 |   59 | `		pSet->pBase = pNew;` |
|  10195695 |   60 | `		pSet->nSize = nNew;` |
|   5093723 |   61 | `	}` |
|  83242096 |   62 | `	zbase = (unsigned char *)pSet->pBase;` |
|  83242096 |   63 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  83242096 |   64 | `	pSet->nUsed++;` |
|  83242096 |   65 | `	return SXRET_OK;` |
|  41568496 |   66 | `}` |
|  12015495 |   67 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   68 | `{` |
|  12015500 |   69 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   70 | `		return SXERR_LOCKED;` |
|         - |   71 | `	}` |
|  12015500 |   72 | `	if( nItem < 8 ){` |
|       ! 0 |   73 | `		nItem = 8;` |
|       ! 0 |   74 | `	}` |
|  12015500 |   75 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|  12015500 |   76 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   77 | `		return SXERR_MEM;` |
|         - |   78 | `	}` |
|  12015500 |   79 | `	pSet->nSize = nItem;` |
|  12015500 |   80 | `	return SXRET_OK;` |
|   5999204 |   81 | `}` |
|  13141638 |   82 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   83 | `{` |
|  13141643 |   84 | `	pSet->nUsed   = 0;` |
|  13141643 |   85 | `	pSet->nCursor = 0;` |
|  13141643 |   86 | `	return SXRET_OK;` |
|         5 |   87 | `}` |
|    154002 |   88 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   89 | `{` |
|    154007 |   90 | `	pSet->nCursor = 0;` |
|    154007 |   91 | `	return SXRET_OK;` |
|         5 |   92 | `}` |
|    161006 |   93 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   94 | `{` |
|         - |   95 | `	register unsigned char *zSrc;` |
|    161011 |   96 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   97 | `		/* Reset cursor */` |
|     71488 |   98 | `		pSet->nCursor = 0;` |
|     71488 |   99 | `		return SXERR_EOF;` |
|         - |  100 | `	}` |
|     89528 |  101 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     89528 |  102 | `	if( ppEntry ){` |
|     89528 |  103 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     44643 |  104 | `	}` |
|     89528 |  105 | `	pSet->nCursor++;` |
|     89528 |  106 | `	return SXRET_OK;` |
|     80272 |  107 | `}` |
|         - |  108 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|       ! 0 |  109 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet)` |
|       ! 0 |  110 | `{` |
|         - |  111 | `	register unsigned char *zSrc;` |
|       ! 0 |  112 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|       ! 0 |  113 | `		return 0;` |
|         - |  114 | `	}` |
|       ! 0 |  115 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|       ! 0 |  116 | `	return (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|       ! 0 |  117 | `}` |
|         - |  118 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - |  119 | `/*` |
|         - |  120 | ` * Give back the slack a doubling set is holding.` |
|         - |  121 | ` *` |
|         - |  122 | ` * A set that grew by doubling ends between half full and full, so a container` |
|         - |  123 | ` * that will never be appended to again is holding up to as much again as it` |
|         - |  124 | ` * uses. That is the shape of compiled BYTECODE: a function's instructions are` |
|         - |  125 | ` * emitted once, at compile time, and then only read -- and they were the single` |
|         - |  126 | ` * largest thing on the heap of an ecosystem-gate lint run after the value table,` |
|         - |  127 | ` * 11.1 MB across 2,965 containers.` |
|         - |  128 | ` *` |
|         - |  129 | ` * Only for a set nothing will append to: it leaves nUsed == nSize, so the very` |
|         - |  130 | ` * next SySetPut reallocs. A failed shrink is not an error -- the set keeps the` |
|         - |  131 | ` * larger buffer it already has.` |
|         - |  132 | ` */` |
|    203817 |  133 | `PH7_PRIVATE void SySetShrinkToFit(SySet *pSet)` |
|         5 |  134 | `{` |
|         - |  135 | `	void *pNew;` |
|    203822 |  136 | `	if( pSet->pAllocator == 0 \|\| pSet->nSize <= pSet->nUsed ){` |
|      7019 |  137 | `		return;` |
|         - |  138 | `	}` |
|    196808 |  139 | `	if( pSet->nUsed < 1 ){` |
|         - |  140 | `		/* Nothing to keep. Spelled out rather than routed through SySetRelease,` |
|         - |  141 | `		 * which leaves nSize standing over a NULL base -- harmless for a set` |
|         - |  142 | `		 * nobody touches again, a NULL write for one that is put to. */` |
|       ! 0 |  143 | `		SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|       ! 0 |  144 | `		pSet->pBase = 0;` |
|       ! 0 |  145 | `		pSet->nSize = 0;` |
|       ! 0 |  146 | `		return;` |
|         - |  147 | `	}` |
|    196808 |  148 | `	pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nUsed);` |
|    196808 |  149 | `	if( pNew == 0 ){` |
|       ! 0 |  150 | `		return;   /* keep what we have */` |
|         - |  151 | `	}` |
|    196808 |  152 | `	pSet->pBase = pNew;` |
|    196808 |  153 | `	pSet->nSize = pSet->nUsed;` |
|    101765 |  154 | `}` |
|    166413 |  155 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  156 | `{` |
|    166418 |  157 | `	if( nNewSize < pSet->nUsed ){` |
|      3517 |  158 | `		pSet->nUsed = nNewSize;` |
|      1662 |  159 | `	}` |
|    166418 |  160 | `	return SXRET_OK;` |
|         5 |  161 | `}` |
|  35966829 |  162 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  163 | `{` |
|  35966834 |  164 | `	sxi32 rc = SXRET_OK;` |
|  35966834 |  165 | `	if( pSet->pAllocator && pSet->pBase ){` |
|  10870203 |  166 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   5430335 |  167 | `	}` |
|  35966834 |  168 | `	pSet->pBase = 0;` |
|  35966834 |  169 | `	pSet->nUsed = 0;` |
|  35966834 |  170 | `	pSet->nCursor = 0;` |
|  35966834 |  171 | `	return rc;` |
|         5 |  172 | `}` |
|  16727898 |  173 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  174 | `{` |
|         - |  175 | `	const char *zBase;` |
|  16727903 |  176 | `	if( pSet->nUsed <= 0 ){` |
|      6856 |  177 | `		return 0;` |
|         - |  178 | `	}` |
|  16721052 |  179 | `	zBase = (const char *)pSet->pBase;` |
|  16721052 |  180 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   8354520 |  181 | `}` |
|   4622229 |  182 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  183 | `{` |
|         - |  184 | `	const char *zBase;` |
|         - |  185 | `	void *pData;` |
|   4622234 |  186 | `	if( pSet->nUsed <= 0 ){` |
|    746017 |  187 | `		return 0;` |
|         - |  188 | `	}` |
|   3876222 |  189 | `	zBase = (const char *)pSet->pBase;` |
|   3876222 |  190 | `	pSet->nUsed--;` |
|   3876222 |  191 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   3876222 |  192 | `	return pData;` |
|   2310104 |  193 | `}` |
|         - |  194 | `/* Private hash entry.` |
|         - |  195 | ` *` |
|         - |  196 | ` * Fifty-six bytes, which is what the memory pool's 64-byte bucket holds once its` |
|         - |  197 | ` * eight-byte header is in it. It used to be seventy-two -- eight of them padding` |
|         - |  198 | ` * around a badly ordered public prefix (fixed in SyHashEntry itself) and eight a` |
|         - |  199 | ` * BACKWARD collision link -- and seventy-two lands in the next bucket up, so every` |
|         - |  200 | ` * hash entry in the engine occupied a hundred and twenty-eight bytes to hold` |
|         - |  201 | ` * seventy-two. On the ecosystem gate's phpcs step 54,000 of them are live at the` |
|         - |  202 | ` * heap's high-water mark, which was 6.9 MB of a 76 MB peak for 3.9 MB of entries.` |
|         - |  203 | ` *` |
|         - |  204 | ` * The collision chain is singly linked as a result: an unlink walks its bucket to` |
|         - |  205 | ` * find the predecessor. Buckets hold SXHASH_FILL_FACTOR entries on average, so that` |
|         - |  206 | ` * is three pointer compares on a DELETE -- against sixty-four bytes on every entry` |
|         - |  207 | ` * that ever exists. The linear-traversal list (pNext/pPrev) stays doubly linked:` |
|         - |  208 | ` * SyHashForEachReverse and get_defined_vars() walk it backward. */` |
|         - |  209 | `struct SyHashEntry_Pr` |
|         - |  210 | `{` |
|         - |  211 | `	/* The public SyHashEntry prefix, in its order -- this struct is cast to it. */` |
|         - |  212 | `	const void *pKey; /* Hash key */` |
|         - |  213 | `	void *pUserData;  /* User private data */` |
|         - |  214 | `	sxu32 nKeyLen;    /* Key length */` |
|         - |  215 | `	/* Private fields */` |
|         - |  216 | `	sxu32 nHash;` |
|         - |  217 | `	SyHash *pHash;` |
|         - |  218 | `	SyHashEntry_Pr *pNext,*pPrev; /* Next and previous entry in the list */` |
|         - |  219 | `	SyHashEntry_Pr *pNextCollide; /* Collision chain, forward only (see above) */` |
|         - |  220 | `};` |
|         - |  221 | `#define INVALID_HASH(H) ((H)->apBucket == 0)` |
|  11608773 |  222 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  223 | `{` |
|         - |  224 | `	SyHashEntry_Pr **apNew;` |
|         - |  225 | `#if defined(UNTRUST)` |
|         - |  226 | `	if( pHash == 0 ){` |
|         - |  227 | `		return SXERR_EMPTY;` |
|         - |  228 | `	}` |
|         - |  229 | `#endif` |
|         - |  230 | `	/* Allocate a new table */` |
|  11608778 |  231 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|  11608778 |  232 | `	if( apNew == 0 ){` |
|       ! 0 |  233 | `		return SXERR_MEM;` |
|         - |  234 | `	}` |
|  11608778 |  235 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * SXHASH_BUCKET_SIZE);` |
|  11608778 |  236 | `	pHash->pAllocator = &(*pAllocator);` |
|  11608778 |  237 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|  11608778 |  238 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|  11608778 |  239 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|  11608778 |  240 | `	pHash->nEntry = 0;` |
|  11608778 |  241 | `	pHash->apBucket = apNew;` |
|  11608778 |  242 | `	pHash->nBucketSize = SXHASH_BUCKET_SIZE;` |
|  11608778 |  243 | `	return SXRET_OK;` |
|   5799852 |  244 | `}` |
|   5461676 |  245 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  246 | `{` |
|         - |  247 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  248 | `#if defined(UNTRUST)` |
|         - |  249 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  250 | `		return SXERR_EMPTY;` |
|         - |  251 | `	}` |
|         - |  252 | `#endif` |
|   5461681 |  253 | `	pEntry = pHash->pList;` |
|   8307876 |  254 | `	for(;;){` |
|  16616304 |  255 | `		if( pHash->nEntry == 0 ){` |
|   5461681 |  256 | `			break;` |
|         - |  257 | `		}` |
|  11154628 |  258 | `		pNext = pEntry->pNext;` |
|  11154628 |  259 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|  11154628 |  260 | `		pEntry = pNext;` |
|  11154628 |  261 | `		pHash->nEntry--;` |
|         5 |  262 | `	}` |
|   5461681 |  263 | `	if( pHash->apBucket ){` |
|   5461681 |  264 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|   2730335 |  265 | `	}` |
|   5461681 |  266 | `	pHash->apBucket = 0;` |
|   5461681 |  267 | `	pHash->nBucketSize = 0;` |
|   5461681 |  268 | `	pHash->pAllocator = 0;` |
|   5461681 |  269 | `	return SXRET_OK;` |
|         5 |  270 | `}` |
|         - |  271 | `#if defined(PHL_HASH_CENSUS)` |
|         - |  272 | `/*` |
|         - |  273 | ` * Which frame a site address is taken from -- see build-aux/hashcensus.sh -c.` |
|         - |  274 | ` *` |
|         - |  275 | ` * By default a row names the line that called SyHashGet, which is what a change is` |
|         - |  276 | ` * aimed at. But a row can be a whole SUBSYSTEM's worth of work funnelled through one` |
|         - |  277 | ` * line: PH7_VmExtractClass has 221 callers and PH7_VmExtractVarCached's misses, the` |
|         - |  278 | ` * foreach step, OP_STORE and three frame-setup doors all reach the frame table` |
|         - |  279 | ` * through the same SyHashGet. Building with PHL_HCENSUS_CALLER lifts every row one` |
|         - |  280 | ` * frame, so it names the CALLER instead, and the two runs together decompose a large` |
|         - |  281 | ` * row into the doors that actually make it. That is how the 146th session found that` |
|         - |  282 | ` * 60% of the engine's lookups were six doors and not one (PERF.md §2).` |
|         - |  283 | ` *` |
|         - |  284 | ` * It needs -fno-omit-frame-pointer (gcc will not walk up without one) and` |
|         - |  285 | ` * -Wno-frame-address (gcc warns about a nonzero argument on principle); the script` |
|         - |  286 | ` * passes both. Never in a shipping build -- like everything else in this file it is` |
|         - |  287 | ` * compiled out unless PHL_HASH_CENSUS is defined.` |
|         - |  288 | ` */` |
|         - |  289 | `#if defined(PHL_HCENSUS_CALLER)` |
|         - |  290 | `#define PHL_HCENSUS_SITE() __builtin_return_address(1)` |
|         - |  291 | `#else` |
|         - |  292 | `#define PHL_HCENSUS_SITE() __builtin_return_address(0)` |
|         - |  293 | `#endif` |
|         - |  294 | `/*` |
|         - |  295 | ` * PHL_HASH_CENSUS -- which CALL SITE spends the engine's name hashing.` |
|         - |  296 | ` * ---------------------------------------------------------------------------` |
|         - |  297 | ` * Compiled out entirely unless PHL_HASH_CENSUS is defined; see PERF.md §7, and` |
|         - |  298 | ` * build-aux/hashcensus.sh, which builds it and resolves what it prints.` |
|         - |  299 | ` *` |
|         - |  300 | ` * The heap census (src/sx/sxmem.c) answers "where are the bytes and who asked` |
|         - |  301 | ` * for them"; this answers the same question for SyHashGet, which on a real` |
|         - |  302 | ` * workload is the single largest subsystem in a profile and which the sampler` |
|         - |  303 | ` * can only attribute one frame deep, for the samples that happened to land in` |
|         - |  304 | ` * it. These counts are exact, and they do not care that this box is loaded` |
|         - |  305 | ` * (PERF.md §7) -- a lookup either happened or it did not.` |
|         - |  306 | ` *` |
|         - |  307 | ` * One record per return address, so a site is a place in the SOURCE and not a` |
|         - |  308 | ` * table: two lookups against the same hash table from two lines are two rows,` |
|         - |  309 | ` * which is what a change has to be aimed at. Addresses are emitted relative to` |
|         - |  310 | ` * the PIE load base (the ADDRESS of __executable_start is that base at run` |
|         - |  311 | ` * time), so addr2line takes them exactly as printed -- the heap census's` |
|         - |  312 | ` * convention, for the same reason.` |
|         - |  313 | ` *` |
|         - |  314 | ` * The table is fixed-size and static: it must not allocate through the` |
|         - |  315 | ` * allocator whose tables it is measuring. Nothing is ever deleted from it (a` |
|         - |  316 | ` * site is a code address and there are a few thousand), so a full table would` |
|         - |  317 | ` * spin -- it refuses to record instead, and says so.` |
|         - |  318 | ` */` |
|         - |  319 | `#include <stdio.h>` |
|         - |  320 | `#include <stdlib.h>` |
|         - |  321 |  |
|         - |  322 | `extern char __executable_start[];   /* its ADDRESS is the PIE load base */` |
|         - |  323 |  |
|         - |  324 | `typedef struct phl_hcensus_rec phl_hcensus_rec;` |
|         - |  325 | `struct phl_hcensus_rec {` |
|         - |  326 | `	void *pSite;     /* PHL_HCENSUS_SITE() at SyHashGet; 0 = free slot */` |
|         - |  327 | `	sxu64 nCall;     /* lookups made from here */` |
|         - |  328 | `	sxu64 nKeyByte;  /* key bytes hashed for them (0 for one the table answered` |
|         - |  329 | `	                  * without hashing -- an empty table, or an empty key) */` |
|         - |  330 | `	sxu64 nHit;      /* how many found an entry */` |
|         - |  331 | `	sxu64 nCI;       /* how many were against a case-insensitive table */` |
|         - |  332 | `};` |
|         - |  333 | `#define PHL_HCENSUS_SLOTS 8192` |
|         - |  334 | `static struct {` |
|         - |  335 | `	int bReady;      /* 0 = untouched, 1 = live */` |
|         - |  336 | `	int bFull;       /* the table filled; recording stopped */` |
|         - |  337 | `	phl_hcensus_rec aRec[PHL_HCENSUS_SLOTS];` |
|         - |  338 | `	sxu64 nCall,nKeyByte,nHit;` |
|         - |  339 | `} sHCensus;` |
|         - |  340 |  |
|         - |  341 | `static void HCensusDump(void)` |
|         - |  342 | `{` |
|         - |  343 | `	const char *zOut = getenv("PHL_HCENSUS_OUT");` |
|         - |  344 | `	FILE *pOut = zOut ? fopen(zOut,"w") : stderr;` |
|         - |  345 | `	sxu32 i;` |
|         - |  346 | `	if( pOut == 0 ){` |
|         - |  347 | `		pOut = stderr;` |
|         - |  348 | `	}` |
|         - |  349 | `	fprintf(pOut,"# lookups %llu bytes %llu hits %llu%s\n",` |
|         - |  350 | `		(unsigned long long)sHCensus.nCall,(unsigned long long)sHCensus.nKeyByte,` |
|         - |  351 | `		(unsigned long long)sHCensus.nHit,sHCensus.bFull ? "  TRUNCATED" : "");` |
|         - |  352 | `	for( i = 0 ; i < PHL_HCENSUS_SLOTS ; ++i ){` |
|         - |  353 | `		phl_hcensus_rec *pRec = &sHCensus.aRec[i];` |
|         - |  354 | `		if( pRec->pSite == 0 ){` |
|         - |  355 | `			continue;` |
|         - |  356 | `		}` |
|         - |  357 | `		fprintf(pOut,"SITE 0x%lx %llu %llu %llu %llu\n",` |
|         - |  358 | `			(unsigned long)((char *)pRec->pSite - __executable_start),` |
|         - |  359 | `			(unsigned long long)pRec->nCall,(unsigned long long)pRec->nKeyByte,` |
|         - |  360 | `			(unsigned long long)pRec->nHit,(unsigned long long)pRec->nCI);` |
|         - |  361 | `	}` |
|         - |  362 | `	if( pOut != stderr ){` |
|         - |  363 | `		fclose(pOut);` |
|         - |  364 | `	}` |
|         - |  365 | `}` |
|         - |  366 | `static phl_hcensus_rec * HCensusSlot(void *pSite)` |
|         - |  367 | `{` |
|         - |  368 | `	/* Fibonacci scramble: the low bits of a code address are not a key. */` |
|         - |  369 | `	sxu64 x = (sxu64)(sxuptr)pSite;` |
|         - |  370 | `	sxu32 i,n;` |
|         - |  371 | `	x ^= x >> 33; x *= (sxu64)0xff51afd7ed558ccdULL; x ^= x >> 29;` |
|         - |  372 | `	i = (sxu32)x & (PHL_HCENSUS_SLOTS - 1);` |
|         - |  373 | `	for( n = 0 ; n < PHL_HCENSUS_SLOTS ; ++n ){` |
|         - |  374 | `		if( sHCensus.aRec[i].pSite == pSite ){` |
|         - |  375 | `			return &sHCensus.aRec[i];` |
|         - |  376 | `		}` |
|         - |  377 | `		if( sHCensus.aRec[i].pSite == 0 ){` |
|         - |  378 | `			sHCensus.aRec[i].pSite = pSite;` |
|         - |  379 | `			return &sHCensus.aRec[i];` |
|         - |  380 | `		}` |
|         - |  381 | `		i = (i + 1) & (PHL_HCENSUS_SLOTS - 1);` |
|         - |  382 | `	}` |
|         - |  383 | `	sHCensus.bFull = 1;   /* said out loud in the dump rather than counted wrong */` |
|         - |  384 | `	return 0;` |
|         - |  385 | `}` |
|         - |  386 | `static void HCensusNote(void *pSite,SyHash *pHash,sxu32 nKeyLen,int bHit)` |
|         - |  387 | `{` |
|         - |  388 | `	phl_hcensus_rec *pRec;` |
|         - |  389 | `	if( !sHCensus.bReady ){` |
|         - |  390 | `		sHCensus.bReady = 1;` |
|         - |  391 | `		atexit(HCensusDump);` |
|         - |  392 | `	}` |
|         - |  393 | `	pRec = HCensusSlot(pSite);` |
|         - |  394 | `	if( pRec == 0 ){` |
|         - |  395 | `		return;` |
|         - |  396 | `	}` |
|         - |  397 | `	pRec->nCall++;` |
|         - |  398 | `	pRec->nKeyByte += nKeyLen;` |
|         - |  399 | `	pRec->nHit += bHit ? 1 : 0;` |
|         - |  400 | `	pRec->nCI += (pHash->xHash == SyStrHash) ? 1 : 0;` |
|         - |  401 | `	sHCensus.nCall++;` |
|         - |  402 | `	sHCensus.nKeyByte += nKeyLen;` |
|         - |  403 | `	sHCensus.nHit += bHit ? 1 : 0;` |
|         - |  404 | `}` |
|         - |  405 | `#endif /* PHL_HASH_CENSUS */` |
| 137914962 |  406 | `static SyHashEntry_Pr * HashGetEntryHashed(SyHash *pHash,const void *pKey,sxu32 nKeyLen,sxu32 nHash)` |
|         5 |  407 | `{` |
|         - |  408 | `	SyHashEntry_Pr *pEntry;` |
|         - |  409 |  |
| 137914967 |  410 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
| 134225519 |  411 | `	for(;;){` |
| 271142863 |  412 | `		if( pEntry == 0 ){` |
|  57582281 |  413 | `			break;` |
|         - |  414 | `		}` |
| 253788140 |  415 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|  80361526 |  416 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|  80332691 |  417 | `				return pEntry;` |
|         - |  418 | `		}` |
| 133227901 |  419 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  420 | `	}` |
|         - |  421 | `	/* Entry not found */` |
|  57582281 |  422 | `	return 0;` |
|  68714477 |  423 | `}` |
| 137793509 |  424 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  425 | `{` |
| 137793514 |  426 | `	return HashGetEntryHashed(&(*pHash),pKey,nKeyLen,pHash->xHash(pKey,nKeyLen));` |
|         5 |  427 | `}` |
| 137345900 |  428 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  429 | `{` |
|         - |  430 | `	SyHashEntry_Pr *pEntry;` |
|         - |  431 | `#if defined(PHL_HASH_CENSUS)` |
|         - |  432 | `	void *pCensusSite = PHL_HCENSUS_SITE();` |
|         - |  433 | `#endif` |
|         - |  434 | `#if defined(UNTRUST)` |
|         - |  435 | `	if( INVALID_HASH(pHash) ){` |
|         - |  436 | `		return 0;` |
|         - |  437 | `	}` |
|         - |  438 | `#endif` |
| 137345905 |  439 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  440 | `		/* Don't bother hashing,return immediately */` |
|         - |  441 | `#if defined(PHL_HASH_CENSUS)` |
|         - |  442 | `		HCensusNote(pCensusSite,pHash,0,0);` |
|         - |  443 | `#endif` |
|   6441016 |  444 | `		return 0;` |
|         - |  445 | `	}` |
| 130904894 |  446 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|         - |  447 | `#if defined(PHL_HASH_CENSUS)` |
|         - |  448 | `	HCensusNote(pCensusSite,pHash,nKeyLen,pEntry != 0);` |
|         - |  449 | `#endif` |
| 130904894 |  450 | `	if( pEntry == 0 ){` |
|  57556730 |  451 | `		return 0;` |
|         - |  452 | `	}` |
|  73348169 |  453 | `	return (SyHashEntry *)pEntry;` |
|  68425412 |  454 | `}` |
|         - |  455 | `/*` |
|         - |  456 | ` * The KEY hashed once, for a caller that is about to ask several questions with it.` |
|         - |  457 | ` *` |
|         - |  458 | ` * A property access asks two -- does the executing scope declare a private of this` |
|         - |  459 | ` * name, and where is the slot on this object -- and used to hash the same bytes for` |
|         - |  460 | ` * each. Hashing is what a lookup spends (PERF.md §5), so the caller hashes once here` |
|         - |  461 | ` * and hands the answer to SyHashGetHashed below.` |
|         - |  462 | ` *` |
|         - |  463 | ` * The hash belongs to the TABLE, not to the key: two tables with different xHash` |
|         - |  464 | ` * answer differently for the same bytes. Only pass a hash taken from this function,` |
|         - |  465 | ` * and only to a table that shares this one's xHash.` |
|         - |  466 | ` */` |
|   1097914 |  467 | `PH7_PRIVATE sxu32 SyHashKey(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  468 | `{` |
|         - |  469 | `	sxu32 nHash;` |
|         - |  470 | `#if defined(PHL_HASH_CENSUS)` |
|         - |  471 | `	void *pCensusSite = PHL_HCENSUS_SITE();` |
|         - |  472 | `#endif` |
|   1097919 |  473 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|         - |  474 | `#if defined(PHL_HASH_CENSUS)` |
|         - |  475 | `	/* Counted as a question asked at this site, because that is what it is: the` |
|         - |  476 | `	 * bytes are charged HERE, and the prehashed probes below then show none. A` |
|         - |  477 | `	 * census that only counted SyHashGet would make the work disappear. */` |
|         - |  478 | `	HCensusNote(pCensusSite,pHash,nKeyLen,1);` |
|         - |  479 | `#endif` |
|   1097919 |  480 | `	return nHash;` |
|         5 |  481 | `}` |
|         - |  482 | `/*` |
|         - |  483 | ` * SyHashGet with the key already hashed (SyHashKey). Hashes nothing.` |
|         - |  484 | ` */` |
|    121771 |  485 | `PH7_PRIVATE SyHashEntry * SyHashGetHashed(SyHash *pHash,const void *pKey,sxu32 nKeyLen,sxu32 nHash)` |
|         5 |  486 | `{` |
|         - |  487 | `	SyHashEntry_Pr *pEntry;` |
|         - |  488 | `#if defined(PHL_HASH_CENSUS)` |
|         - |  489 | `	void *pCensusSite = PHL_HCENSUS_SITE();` |
|         - |  490 | `#endif` |
|         - |  491 | `#if defined(UNTRUST)` |
|         - |  492 | `	if( INVALID_HASH(pHash) ){` |
|         - |  493 | `		return 0;` |
|         - |  494 | `	}` |
|         - |  495 | `	/* The one way to get this wrong is to hand over a hash taken for another` |
|         - |  496 | `	 * table. Answer NOT FOUND rather than whatever sits in the wrong bucket --` |
|         - |  497 | `	 * the same shape as the INVALID_HASH screen above, and like it this only` |
|         - |  498 | `	 * exists in an UNTRUST build. */` |
|         - |  499 | `	if( nKeyLen > 0 && nHash != pHash->xHash(pKey,nKeyLen) ){` |
|         - |  500 | `		return 0;` |
|         - |  501 | `	}` |
|         - |  502 | `#endif` |
|    121776 |  503 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  504 | `#if defined(PHL_HASH_CENSUS)` |
|         - |  505 | `		HCensusNote(pCensusSite,pHash,0,0);` |
|         - |  506 | `#endif` |
|       323 |  507 | `		return 0;` |
|         - |  508 | `	}` |
|    121458 |  509 | `	pEntry = HashGetEntryHashed(&(*pHash),pKey,nKeyLen,nHash);` |
|         - |  510 | `#if defined(PHL_HASH_CENSUS)` |
|         - |  511 | `	HCensusNote(pCensusSite,pHash,0,pEntry != 0);` |
|         - |  512 | `#endif` |
|    121458 |  513 | `	if( pEntry == 0 ){` |
|      7518 |  514 | `		return 0;` |
|         - |  515 | `	}` |
|    113945 |  516 | `	return (SyHashEntry *)pEntry;` |
|     60891 |  517 | `}` |
|   6898648 |  518 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  519 | `{` |
|         - |  520 | `	sxi32 rc;` |
|         - |  521 | `	/* Unlink from the collision chain. It is singly linked (see SyHashEntry_Pr), so` |
|         - |  522 | `	 * the predecessor is found by walking the bucket -- SXHASH_FILL_FACTOR entries` |
|         - |  523 | `	 * long on average. An entry that is not in its own bucket is a corrupted table` |
|         - |  524 | `	 * and the walk simply finds nothing rather than writing through a stale link. */` |
|         - |  525 | `	{` |
|   6898653 |  526 | `		SyHashEntry_Pr **ppSlot = &pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)];` |
|   6917897 |  527 | `		while( *ppSlot ){` |
|   6917897 |  528 | `			if( *ppSlot == pEntry ){` |
|   6898653 |  529 | `				*ppSlot = pEntry->pNextCollide;` |
|   6898653 |  530 | `				break;` |
|         - |  531 | `			}` |
|     19249 |  532 | `			ppSlot = &(*ppSlot)->pNextCollide;` |
|         5 |  533 | `		}` |
|         - |  534 | `	}` |
|         - |  535 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|   6898653 |  536 | `	if( pHash->pLast == pEntry ){` |
|     48396 |  537 | `		pHash->pLast = pEntry->pPrev;` |
|     24123 |  538 | `	}` |
|         - |  539 | `	/* ...and the embedded loop cursor, which the caller may be standing on: the` |
|         - |  540 | `	 * cursor is advanced BEFORE the body of a SyHashGetNextEntry() walk runs, so` |
|         - |  541 | `	 * a body that deletes the entry it is about to reach left pCurrent pointing` |
|         - |  542 | `	 * into the pool slot freed below. */` |
|   6898653 |  543 | `	if( pHash->pCurrent == pEntry ){` |
|     16613 |  544 | `		pHash->pCurrent = pEntry->pNext;` |
|      8280 |  545 | `	}` |
|   6898653 |  546 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|   6898653 |  547 | `	pHash->nEntry--;` |
|   6898653 |  548 | `	if( ppUserData ){` |
|         - |  549 | `		/* Write a pointer to the user data */` |
|       157 |  550 | `		*ppUserData = pEntry->pUserData;` |
|        77 |  551 | `	}` |
|         - |  552 | `	/* Release the entry */` |
|   6898653 |  553 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|   6898653 |  554 | `	return rc;` |
|         5 |  555 | `}` |
|   6888620 |  556 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  557 | `{` |
|         - |  558 | `	SyHashEntry_Pr *pEntry;` |
|         - |  559 | `	sxi32 rc;` |
|         - |  560 | `#if defined(UNTRUST)` |
|         - |  561 | `	if( INVALID_HASH(pHash) ){` |
|         - |  562 | `		return SXERR_CORRUPT;` |
|         - |  563 | `	}` |
|         - |  564 | `#endif` |
|   6888625 |  565 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   6888625 |  566 | `	if( pEntry == 0 ){` |
|     18040 |  567 | `		return SXERR_NOTFOUND;` |
|         - |  568 | `	}` |
|   6870587 |  569 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|   6870587 |  570 | `	return rc;` |
|   3444154 |  571 | `}` |
|     28066 |  572 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  573 | `{` |
|     28071 |  574 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  575 | `	sxi32 rc;` |
|         - |  576 | `#if defined(UNTRUST)` |
|         - |  577 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  578 | `		return SXERR_CORRUPT;` |
|         - |  579 | `	}` |
|         - |  580 | `#endif` |
|     28071 |  581 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     28071 |  582 | `	return rc;` |
|         5 |  583 | `}` |
|  16338356 |  584 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  585 | `{` |
|         - |  586 | `#if defined(UNTRUST)` |
|         - |  587 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  588 | `		return SXERR_CORRUPT;` |
|         - |  589 | `	}` |
|         - |  590 | `#endif` |
|  16338361 |  591 | `	pHash->pCurrent = pHash->pList;` |
|  16338361 |  592 | `	return SXRET_OK;` |
|         5 |  593 | `}` |
| 132310157 |  594 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  595 | `{` |
|         - |  596 | `	SyHashEntry_Pr *pEntry;` |
|         - |  597 | `#if defined(UNTRUST)` |
|         - |  598 | `	if( INVALID_HASH(pHash) ){` |
|         - |  599 | `		return 0;` |
|         - |  600 | `	}` |
|         - |  601 | `#endif` |
| 132310162 |  602 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|  16338049 |  603 | `		pHash->pCurrent = pHash->pList;` |
|  16338049 |  604 | `		return 0;` |
|         - |  605 | `	}` |
| 115972118 |  606 | `	pEntry = pHash->pCurrent;` |
|         - |  607 | `	/* Advance the cursor */` |
| 115972118 |  608 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  609 | `	/* Return the current entry */` |
| 115972118 |  610 | `	return (SyHashEntry *)pEntry;` |
|  66085910 |  611 | `}` |
|        80 |  612 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         3 |  613 | `{` |
|         - |  614 | `	SyHashEntry_Pr *pEntry;` |
|         - |  615 | `	sxi32 rc;` |
|         - |  616 | `	sxu32 n;` |
|         - |  617 | `#if defined(UNTRUST)` |
|         - |  618 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  619 | `		return 0;` |
|         - |  620 | `	}` |
|         - |  621 | `#endif` |
|        83 |  622 | `	pEntry = pHash->pList;` |
|    131891 |  623 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  624 | `		/* Invoke the callback */` |
|    131811 |  625 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|    131811 |  626 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  627 | `			return rc;` |
|         - |  628 | `		}` |
|         - |  629 | `		/* Point to the next entry */` |
|    131811 |  630 | `		pEntry = pEntry->pNext;` |
|     63986 |  631 | `	}` |
|        83 |  632 | `	return SXRET_OK;` |
|        43 |  633 | `}` |
|         - |  634 | `/*` |
|         - |  635 | ` * Like SyHashForEach but walks the entries from the tail (pLast) back to the` |
|         - |  636 | ` * head via pPrev. The frame's local-variable table is built with SyHashInsert` |
|         - |  637 | ` * (head-push), so its forward pList order is reverse-insertion (LIFO); walking` |
|         - |  638 | ` * it backward yields DECLARATION order, which is what php's get_defined_vars()` |
|         - |  639 | ` * reports. Kept as its own primitive so the shared head-push insert path — and` |
|         - |  640 | ` * the SyHashLastEntry()==pList head contract every RefObj install relies on —` |
|         - |  641 | ` * stays untouched.` |
|         - |  642 | ` */` |
|       144 |  643 | `PH7_PRIVATE sxi32 SyHashForEachReverse(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         5 |  644 | `{` |
|         - |  645 | `	SyHashEntry_Pr *pEntry;` |
|         - |  646 | `	sxi32 rc;` |
|         - |  647 | `	sxu32 n;` |
|         - |  648 | `#if defined(UNTRUST)` |
|         - |  649 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  650 | `		return 0;` |
|         - |  651 | `	}` |
|         - |  652 | `#endif` |
|       149 |  653 | `	pEntry = pHash->pLast;` |
|    129401 |  654 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  655 | `		/* Invoke the callback */` |
|    129257 |  656 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|    129257 |  657 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  658 | `			return rc;` |
|         - |  659 | `		}` |
|         - |  660 | `		/* Point to the previous entry */` |
|    129257 |  661 | `		pEntry = pEntry->pPrev;` |
|     64601 |  662 | `	}` |
|       149 |  663 | `	return SXRET_OK;` |
|        77 |  664 | `}` |
|    240105 |  665 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  666 | `{` |
|    240110 |  667 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  668 | `	SyHashEntry_Pr *pEntry;` |
|         - |  669 | `	SyHashEntry_Pr **apNew;` |
|         - |  670 | `	sxu32 n,iBucket;` |
|         - |  671 |  |
|         - |  672 | `	/* Allocate a new larger table */` |
|    240110 |  673 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|    240110 |  674 | `	if( apNew == 0 ){` |
|         - |  675 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  676 | `		return SXRET_OK;` |
|         - |  677 | `	}` |
|         - |  678 | `	/* Zero the new table */` |
|    240110 |  679 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  680 | `	/* Rehash all entries */` |
|  45565550 |  681 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|         - |  682 | `		/* Install in the new bucket */` |
|  45325445 |  683 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|  45325445 |  684 | `		pEntry->pNextCollide = apNew[iBucket];` |
|  45325445 |  685 | `		apNew[iBucket] = pEntry;` |
|         - |  686 | `		/* Point to the next entry */` |
|  45325445 |  687 | `		pEntry = pEntry->pNext;` |
|  22629509 |  688 | `	}` |
|         - |  689 | `	/* Release the old table and reflect the change */` |
|    240110 |  690 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|    240110 |  691 | `	pHash->apBucket = apNew;` |
|    240110 |  692 | `	pHash->nBucketSize = nNewSize;` |
|    240110 |  693 | `	return SXRET_OK;` |
|    119889 |  694 | `}` |
|  82096295 |  695 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  696 | `{` |
|  82096300 |  697 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  698 | `	/* Insert the entry in its corresponding bucket */` |
|  82096300 |  699 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|  82096300 |  700 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  701 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  702 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  703 | `	 * callers that need a FIFO traversal. */` |
|  82096300 |  704 | `	if( bTail && pHash->pLast != 0 ){` |
|  14884276 |  705 | `		pHash->pLast->pNext = pEntry;` |
|  14884276 |  706 | `		pEntry->pPrev = pHash->pLast;` |
|  14884276 |  707 | `		pHash->pLast = pEntry;` |
|   7437707 |  708 | `	}else{` |
|  67212029 |  709 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  710 | `	}` |
|  82096300 |  711 | `	if( pHash->nEntry == 0 ){` |
|         - |  712 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|   5428886 |  713 | `		pHash->pCurrent = pHash->pList;` |
|   5428886 |  714 | `		pHash->pLast = pEntry;` |
|   2711990 |  715 | `	}` |
|  82096300 |  716 | `	pHash->nEntry++;` |
|  82096300 |  717 | `	return SXRET_OK;` |
|         5 |  718 | `}` |
|  82096295 |  719 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  720 | `{` |
|         - |  721 | `	SyHashEntry_Pr *pEntry;` |
|         - |  722 | `	sxi32 rc;` |
|         - |  723 | `#if defined(UNTRUST)` |
|         - |  724 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  725 | `		return SXERR_CORRUPT;` |
|         - |  726 | `	}` |
|         - |  727 | `#endif` |
|  82096300 |  728 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|    240110 |  729 | `		rc = HashGrowTable(&(*pHash));` |
|    240110 |  730 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  731 | `			return rc;` |
|         - |  732 | `		}` |
|    119884 |  733 | `	}` |
|         - |  734 | `	/* Allocate a new hash entry */` |
|  82096300 |  735 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|  82096300 |  736 | `	if( pEntry == 0 ){` |
|       ! 0 |  737 | `		return SXERR_MEM;` |
|         - |  738 | `	}` |
|         - |  739 | `	/* Zero the entry */` |
|  82096300 |  740 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|  82096300 |  741 | `	pEntry->pHash = pHash;` |
|  82096300 |  742 | `	pEntry->pKey = pKey;` |
|  82096300 |  743 | `	pEntry->nKeyLen = nKeyLen;` |
|  82096300 |  744 | `	pEntry->pUserData = pUserData;` |
|  82096300 |  745 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  746 | `	/* Finally insert the entry in its corresponding bucket */` |
|  82096300 |  747 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|  82096300 |  748 | `	return rc;` |
|  40836445 |  749 | `}` |
|  64520847 |  750 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  751 | `{` |
|  64520852 |  752 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|         5 |  753 | `}` |
|         - |  754 | `/*` |
|         - |  755 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|         - |  756 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|         - |  757 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|         - |  758 | ` * object properties, where PHP preserves property-creation order.` |
|         - |  759 | ` */` |
|  17575448 |  760 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  761 | `{` |
|  17575453 |  762 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|         5 |  763 | `}` |
|         - |  764 | `/*` |
|         - |  765 | ` * The iteration list, walkable WITHOUT the hash's single embedded cursor: the` |
|         - |  766 | ` * head entry and, from any entry, its successor. A consumer that must survive` |
|         - |  767 | ` * re-entrancy (nested walks of one table) or a delete under its own feet keeps` |
|         - |  768 | ` * its own SyHashEntry* here instead of sharing pCurrent — see the instance` |
|         - |  769 | ` * attribute iterator in oo.c.` |
|         - |  770 | ` */` |
|    234738 |  771 | `PH7_PRIVATE SyHashEntry * SyHashFirstEntry(SyHash *pHash)` |
|         5 |  772 | `{` |
|         - |  773 | `#if defined(UNTRUST)` |
|         - |  774 | `	if( INVALID_HASH(pHash) ){` |
|         - |  775 | `		return 0;` |
|         - |  776 | `	}` |
|         - |  777 | `#endif` |
|    234743 |  778 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  779 | `}` |
|   1528832 |  780 | `PH7_PRIVATE SyHashEntry * SyHashEntryNext(SyHashEntry *pEntry)` |
|         5 |  781 | `{` |
|   1528837 |  782 | `	if( pEntry == 0 ){` |
|       ! 0 |  783 | `		return 0;` |
|         - |  784 | `	}` |
|   1528837 |  785 | `	return (SyHashEntry *)((SyHashEntry_Pr *)pEntry)->pNext;` |
|    764421 |  786 | `}` |
|   1340134 |  787 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  788 | `{` |
|         - |  789 | `#if defined(UNTRUST)` |
|         - |  790 | `	if( INVALID_HASH(pHash) ){` |
|         - |  791 | `		return 0;` |
|         - |  792 | `	}` |
|         - |  793 | `#endif` |
|         - |  794 | `	/* Last inserted entry */` |
|   1340139 |  795 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  796 | `}` |
|         - |  797 |  |
