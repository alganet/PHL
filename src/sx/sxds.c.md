# src/sx/sxds.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 342/368 lines (92.93%)

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
| 180901337 |   16 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize)` |
|         5 |   17 | `{` |
| 180901342 |   18 | `	pSet->nSize = 0 ;` |
| 180901342 |   19 | `	pSet->nUsed = 0;` |
| 180901342 |   20 | `	pSet->nCursor = 0;` |
| 180901342 |   21 | `	pSet->eSize = ElemSize;` |
| 180901342 |   22 | `	pSet->pAllocator = pAllocator;` |
| 180901342 |   23 | `	pSet->pBase =  0;` |
| 180901342 |   24 | `	pSet->pUserData = 0;` |
| 180901342 |   25 | `	return SXRET_OK;` |
|         5 |   26 | `}` |
|  96818122 |   27 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem)` |
|         5 |   28 | `{` |
|         - |   29 | `	unsigned char *zbase;` |
|  96818127 |   30 | `	if( pSet->nUsed >= pSet->nSize ){` |
|         - |   31 | `		void *pNew;` |
|         - |   32 | `		sxu32 nNew;` |
|  11093172 |   33 | `		if( pSet->pAllocator == 0 ){` |
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
|  11093172 |   47 | `		nNew = pSet->nSize;` |
|  11093172 |   48 | `		if( nNew > 0 ){` |
|    910415 |   49 | `			nNew *= 2;` |
|    454608 |   50 | `		}else{` |
|  10182762 |   51 | `			nNew = pSet->eSize > 0 ? SXSET_FIRST_BYTES / pSet->eSize : 8;` |
|  10182762 |   52 | `			if( nNew < 1 ){ nNew = 1; }` |
|  10182734 |   53 | `			else if( nNew > 8 ){ nNew = 8; }` |
|         - |   54 | `		}` |
|  11093172 |   55 | `		pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * nNew);` |
|  11093172 |   56 | `		if( pNew == 0 ){` |
|       ! 0 |   57 | `			return SXERR_MEM;` |
|         - |   58 | `		}` |
|  11093172 |   59 | `		pSet->pBase = pNew;` |
|  11093172 |   60 | `		pSet->nSize = nNew;` |
|   5543329 |   61 | `	}` |
|  96818127 |   62 | `	zbase = (unsigned char *)pSet->pBase;` |
|  96818127 |   63 | `	SX_MACRO_FAST_MEMCPY(pItem,&zbase[pSet->nUsed * pSet->eSize],pSet->eSize);` |
|  96818127 |   64 | `	pSet->nUsed++;` |
|  96818127 |   65 | `	return SXRET_OK;` |
|  48359987 |   66 | `}` |
|  14199754 |   67 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem)` |
|         5 |   68 | `{` |
|  14199759 |   69 | `	if( pSet->nSize > 0 ){` |
|       ! 0 |   70 | `		return SXERR_LOCKED;` |
|         - |   71 | `	}` |
|  14199759 |   72 | `	if( nItem < 8 ){` |
|       ! 0 |   73 | `		nItem = 8;` |
|       ! 0 |   74 | `	}` |
|  14199759 |   75 | `	pSet->pBase = SyMemBackendAlloc(pSet->pAllocator,pSet->eSize * nItem);` |
|  14199759 |   76 | `	if( pSet->pBase == 0 ){` |
|       ! 0 |   77 | `		return SXERR_MEM;` |
|         - |   78 | `	}` |
|  14199759 |   79 | `	pSet->nSize = nItem;` |
|  14199759 |   80 | `	return SXRET_OK;` |
|   7090554 |   81 | `}` |
|  14343509 |   82 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet)` |
|         5 |   83 | `{` |
|  14343514 |   84 | `	pSet->nUsed   = 0;` |
|  14343514 |   85 | `	pSet->nCursor = 0;` |
|  14343514 |   86 | `	return SXRET_OK;` |
|         5 |   87 | `}` |
|    160558 |   88 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet)` |
|         5 |   89 | `{` |
|    160563 |   90 | `	pSet->nCursor = 0;` |
|    160563 |   91 | `	return SXRET_OK;` |
|         5 |   92 | `}` |
|    167989 |   93 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry)` |
|         5 |   94 | `{` |
|         - |   95 | `	register unsigned char *zSrc;` |
|    167994 |   96 | `	if( pSet->nCursor >= pSet->nUsed ){` |
|         - |   97 | `		/* Reset cursor */` |
|     74759 |   98 | `		pSet->nCursor = 0;` |
|     74759 |   99 | `		return SXERR_EOF;` |
|         - |  100 | `	}` |
|     93240 |  101 | `	zSrc = (unsigned char *)SySetBasePtr(pSet);` |
|     93240 |  102 | `	if( ppEntry ){` |
|     93240 |  103 | `		*ppEntry = (void *)&zSrc[pSet->nCursor * pSet->eSize];` |
|     46497 |  104 | `	}` |
|     93240 |  105 | `	pSet->nCursor++;` |
|     93240 |  106 | `	return SXRET_OK;` |
|     83759 |  107 | `}` |
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
|    238655 |  133 | `PH7_PRIVATE void SySetShrinkToFit(SySet *pSet)` |
|         5 |  134 | `{` |
|         - |  135 | `	void *pNew;` |
|    238660 |  136 | `	if( pSet->pAllocator == 0 \|\| pSet->nSize <= pSet->nUsed ){` |
|      8245 |  137 | `		return;` |
|         - |  138 | `	}` |
|    230420 |  139 | `	if( pSet->nUsed < 1 ){` |
|         - |  140 | `		/* Nothing to keep. Spelled out rather than routed through SySetRelease,` |
|         - |  141 | `		 * which leaves nSize standing over a NULL base -- harmless for a set` |
|         - |  142 | `		 * nobody touches again, a NULL write for one that is put to. */` |
|       ! 0 |  143 | `		SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|       ! 0 |  144 | `		pSet->pBase = 0;` |
|       ! 0 |  145 | `		pSet->nSize = 0;` |
|       ! 0 |  146 | `		return;` |
|         - |  147 | `	}` |
|    230420 |  148 | `	pNew = SyMemBackendRealloc(pSet->pAllocator,pSet->pBase,pSet->eSize * pSet->nUsed);` |
|    230420 |  149 | `	if( pNew == 0 ){` |
|       ! 0 |  150 | `		return;   /* keep what we have */` |
|         - |  151 | `	}` |
|    230420 |  152 | `	pSet->pBase = pNew;` |
|    230420 |  153 | `	pSet->nSize = pSet->nUsed;` |
|    119180 |  154 | `}` |
|    694786 |  155 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize)` |
|         5 |  156 | `{` |
|    694791 |  157 | `	if( nNewSize < pSet->nUsed ){` |
|    504433 |  158 | `		pSet->nUsed = nNewSize;` |
|    252120 |  159 | `	}` |
|    694791 |  160 | `	return SXRET_OK;` |
|         5 |  161 | `}` |
|  39618445 |  162 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet)` |
|         5 |  163 | `{` |
|  39618450 |  164 | `	sxi32 rc = SXRET_OK;` |
|  39618450 |  165 | `	if( pSet->pAllocator && pSet->pBase ){` |
|  11871561 |  166 | `		rc = SyMemBackendFree(pSet->pAllocator,pSet->pBase);` |
|   5931906 |  167 | `	}` |
|  39618450 |  168 | `	pSet->pBase = 0;` |
|  39618450 |  169 | `	pSet->nUsed = 0;` |
|  39618450 |  170 | `	pSet->nCursor = 0;` |
|  39618450 |  171 | `	return rc;` |
|         5 |  172 | `}` |
|  18796381 |  173 | `PH7_PRIVATE void * SySetPeek(SySet *pSet)` |
|         5 |  174 | `{` |
|         - |  175 | `	const char *zBase;` |
|  18796386 |  176 | `	if( pSet->nUsed <= 0 ){` |
|      8539 |  177 | `		return 0;` |
|         - |  178 | `	}` |
|  18787852 |  179 | `	zBase = (const char *)pSet->pBase;` |
|  18787852 |  180 | `	return (void *)&zBase[(pSet->nUsed - 1) * pSet->eSize];` |
|   9388345 |  181 | `}` |
|   4840604 |  182 | `PH7_PRIVATE void * SySetPop(SySet *pSet)` |
|         5 |  183 | `{` |
|         - |  184 | `	const char *zBase;` |
|         - |  185 | `	void *pData;` |
|   4840609 |  186 | `	if( pSet->nUsed <= 0 ){` |
|    741769 |  187 | `		return 0;` |
|         - |  188 | `	}` |
|   4098845 |  189 | `	zBase = (const char *)pSet->pBase;` |
|   4098845 |  190 | `	pSet->nUsed--;` |
|   4098845 |  191 | `	pData =  (void *)&zBase[pSet->nUsed * pSet->eSize];` |
|   4098845 |  192 | `	return pData;` |
|   2419211 |  193 | `}` |
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
|         - |  222 | `/*` |
|         - |  223 | ` * Initialize a hash table whose expected population is KNOWN.` |
|         - |  224 | ` *` |
|         - |  225 | ` * SXHASH_BUCKET_SIZE is 16, which with SXHASH_FILL_FACTOR 3 is a table sized for` |
|         - |  226 | ` * forty-eight entries. That is the right default for the VM's own tables -- the` |
|         - |  227 | ` * function table, the class table, the constants -- and it is the wrong one for` |
|         - |  228 | ` * the table this engine builds most: an OBJECT's property table. The census of` |
|         - |  229 | ` * the ecosystem gate's phpcs step counts 30,349 of these against 27,835` |
|         - |  230 | ` * instances holding 128,024 property slots between them, which is **4.6` |
|         - |  231 | ` * properties per object** in a table built for forty-eight: 4.40 MB of bucket` |
|         - |  232 | ` * arrays, nearly all of them zeroes.` |
|         - |  233 | ` *` |
|         - |  234 | ` * A caller that already knows the count says so. Everything else is identical,` |
|         - |  235 | ` * including the growth rule, so a table that outgrows its estimate doubles` |
|         - |  236 | ` * exactly as it always did -- and one that was sized right never rehashes at` |
|         - |  237 | ` * all, which the 16-bucket default could not promise a class with fifty` |
|         - |  238 | ` * properties either.` |
|         - |  239 | ` *` |
|         - |  240 | `` * nBucket is rounded UP to a power of two (the bucket index is `nHash &`` |
|         - |  241 | `` * (nBucketSize - 1)`, so it must be one) and floored at 2.`` |
|         - |  242 | ` */` |
|  12737493 |  243 | `PH7_PRIVATE sxi32 SyHashInitSized(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp,sxu32 nBucket)` |
|         5 |  244 | `{` |
|         - |  245 | `	SyHashEntry_Pr **apNew;` |
|         - |  246 | `	sxu32 nSize;` |
|         - |  247 | `#if defined(UNTRUST)` |
|         - |  248 | `	if( pHash == 0 ){` |
|         - |  249 | `		return SXERR_EMPTY;` |
|         - |  250 | `	}` |
|         - |  251 | `#endif` |
|  12737498 |  252 | `	nSize = 2;` |
|  49072420 |  253 | `	while( nSize < nBucket ){` |
|  36334927 |  254 | `		nSize <<= 1;` |
|         5 |  255 | `	}` |
|         - |  256 | `	/* Allocate a new table */` |
|  12737498 |  257 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(&(*pAllocator),sizeof(SyHashEntry_Pr *) * nSize);` |
|  12737498 |  258 | `	if( apNew == 0 ){` |
|       ! 0 |  259 | `		return SXERR_MEM;` |
|         - |  260 | `	}` |
|  12737498 |  261 | `	SyZero((void *)apNew,sizeof(SyHashEntry_Pr *) * nSize);` |
|  12737498 |  262 | `	pHash->pAllocator = &(*pAllocator);` |
|  12737498 |  263 | `	pHash->xHash = xHash ? xHash : SyBinHash;` |
|  12737498 |  264 | `	pHash->xCmp = xCmp ? xCmp : SyMemcmp;` |
|  12737498 |  265 | `	pHash->pCurrent = pHash->pList = pHash->pLast = 0;` |
|  12737498 |  266 | `	pHash->nEntry = 0;` |
|  12737498 |  267 | `	pHash->apBucket = apNew;` |
|  12737498 |  268 | `	pHash->nBucketSize = nSize;` |
|  12737498 |  269 | `	return SXRET_OK;` |
|   6364102 |  270 | `}` |
|  11109014 |  271 | `PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp)` |
|         5 |  272 | `{` |
|  11109019 |  273 | `	return SyHashInitSized(&(*pHash),&(*pAllocator),xHash,xCmp,SXHASH_BUCKET_SIZE);` |
|         5 |  274 | `}` |
|   5510106 |  275 | `PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash)` |
|         5 |  276 | `{` |
|         - |  277 | `	SyHashEntry_Pr *pEntry,*pNext;` |
|         - |  278 | `#if defined(UNTRUST)` |
|         - |  279 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  280 | `		return SXERR_EMPTY;` |
|         - |  281 | `	}` |
|         - |  282 | `#endif` |
|   5510111 |  283 | `	pEntry = pHash->pList;` |
|   8414569 |  284 | `	for(;;){` |
|  16825077 |  285 | `		if( pHash->nEntry == 0 ){` |
|   5510111 |  286 | `			break;` |
|         - |  287 | `		}` |
|  11314971 |  288 | `		pNext = pEntry->pNext;` |
|  11314971 |  289 | `		SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|  11314971 |  290 | `		pEntry = pNext;` |
|  11314971 |  291 | `		pHash->nEntry--;` |
|         5 |  292 | `	}` |
|   5510111 |  293 | `	if( pHash->apBucket ){` |
|   5510111 |  294 | `		SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|   2754888 |  295 | `	}` |
|   5510111 |  296 | `	pHash->apBucket = 0;` |
|   5510111 |  297 | `	pHash->nBucketSize = 0;` |
|   5510111 |  298 | `	pHash->pAllocator = 0;` |
|   5510111 |  299 | `	return SXRET_OK;` |
|         5 |  300 | `}` |
|         - |  301 | `#if defined(PHL_HASH_CENSUS)` |
|         - |  302 | `/*` |
|         - |  303 | ` * Which frame a site address is taken from -- see build-aux/hashcensus.sh -c.` |
|         - |  304 | ` *` |
|         - |  305 | ` * By default a row names the line that called SyHashGet, which is what a change is` |
|         - |  306 | ` * aimed at. But a row can be a whole SUBSYSTEM's worth of work funnelled through one` |
|         - |  307 | ` * line: PH7_VmExtractClass has 221 callers and PH7_VmExtractVarCached's misses, the` |
|         - |  308 | ` * foreach step, OP_STORE and three frame-setup doors all reach the frame table` |
|         - |  309 | ` * through the same SyHashGet. Building with PHL_HCENSUS_CALLER lifts every row one` |
|         - |  310 | ` * frame, so it names the CALLER instead, and the two runs together decompose a large` |
|         - |  311 | ` * row into the doors that actually make it. That is how the 146th session found that` |
|         - |  312 | ` * 60% of the engine's lookups were six doors and not one.` |
|         - |  313 | ` *` |
|         - |  314 | ` * It needs -fno-omit-frame-pointer (gcc will not walk up without one) and` |
|         - |  315 | ` * -Wno-frame-address (gcc warns about a nonzero argument on principle); the script` |
|         - |  316 | ` * passes both. Never in a shipping build -- like everything else in this file it is` |
|         - |  317 | ` * compiled out unless PHL_HASH_CENSUS is defined.` |
|         - |  318 | ` */` |
|         - |  319 | `#if defined(PHL_HCENSUS_CALLER)` |
|         - |  320 | `#define PHL_HCENSUS_SITE() __builtin_return_address(1)` |
|         - |  321 | `#else` |
|         - |  322 | `#define PHL_HCENSUS_SITE() __builtin_return_address(0)` |
|         - |  323 | `#endif` |
|         - |  324 | `/*` |
|         - |  325 | ` * PHL_HASH_CENSUS -- which CALL SITE spends the engine's name hashing.` |
|         - |  326 | ` * ---------------------------------------------------------------------------` |
|         - |  327 | ` * Compiled out entirely unless PHL_HASH_CENSUS is defined; see` |
|         - |  328 | ` * build-aux/hashcensus.sh, which builds it and resolves what it prints.` |
|         - |  329 | ` *` |
|         - |  330 | ` * The heap census (src/sx/sxmem.c) answers "where are the bytes and who asked` |
|         - |  331 | ` * for them"; this answers the same question for SyHashGet, which on a real` |
|         - |  332 | ` * workload is the single largest subsystem in a profile and which the sampler` |
|         - |  333 | ` * can only attribute one frame deep, for the samples that happened to land in` |
|         - |  334 | ` * it. These counts are exact, and they do not care that this box is loaded` |
|         - |  335 | ` * at all -- a lookup either happened or it did not.` |
|         - |  336 | ` *` |
|         - |  337 | ` * One record per return address, so a site is a place in the SOURCE and not a` |
|         - |  338 | ` * table: two lookups against the same hash table from two lines are two rows,` |
|         - |  339 | ` * which is what a change has to be aimed at. Addresses are emitted relative to` |
|         - |  340 | ` * the PIE load base (the ADDRESS of __executable_start is that base at run` |
|         - |  341 | ` * time), so addr2line takes them exactly as printed -- the heap census's` |
|         - |  342 | ` * convention, for the same reason.` |
|         - |  343 | ` *` |
|         - |  344 | ` * The table is fixed-size and static: it must not allocate through the` |
|         - |  345 | ` * allocator whose tables it is measuring. Nothing is ever deleted from it (a` |
|         - |  346 | ` * site is a code address and there are a few thousand), so a full table would` |
|         - |  347 | ` * spin -- it refuses to record instead, and says so.` |
|         - |  348 | ` */` |
|         - |  349 | `#include <stdio.h>` |
|         - |  350 | `#include <stdlib.h>` |
|         - |  351 |  |
|         - |  352 | `extern char __executable_start[];   /* its ADDRESS is the PIE load base */` |
|         - |  353 |  |
|         - |  354 | `typedef struct phl_hcensus_rec phl_hcensus_rec;` |
|         - |  355 | `struct phl_hcensus_rec {` |
|         - |  356 | `	void *pSite;     /* PHL_HCENSUS_SITE() at SyHashGet; 0 = free slot */` |
|         - |  357 | `	sxu64 nCall;     /* lookups made from here */` |
|         - |  358 | `	sxu64 nKeyByte;  /* key bytes hashed for them (0 for one the table answered` |
|         - |  359 | `	                  * without hashing -- an empty table, or an empty key) */` |
|         - |  360 | `	sxu64 nHit;      /* how many found an entry */` |
|         - |  361 | `	sxu64 nCI;       /* how many were against a case-insensitive table */` |
|         - |  362 | `};` |
|         - |  363 | `#define PHL_HCENSUS_SLOTS 8192` |
|         - |  364 | `static struct {` |
|         - |  365 | `	int bReady;      /* 0 = untouched, 1 = live */` |
|         - |  366 | `	int bFull;       /* the table filled; recording stopped */` |
|         - |  367 | `	phl_hcensus_rec aRec[PHL_HCENSUS_SLOTS];` |
|         - |  368 | `	sxu64 nCall,nKeyByte,nHit;` |
|         - |  369 | `} sHCensus;` |
|         - |  370 |  |
|         - |  371 | `static void HCensusDump(void)` |
|         - |  372 | `{` |
|         - |  373 | `	const char *zOut = getenv("PHL_HCENSUS_OUT");` |
|         - |  374 | `	FILE *pOut = zOut ? fopen(zOut,"w") : stderr;` |
|         - |  375 | `	sxu32 i;` |
|         - |  376 | `	if( pOut == 0 ){` |
|         - |  377 | `		pOut = stderr;` |
|         - |  378 | `	}` |
|         - |  379 | `	fprintf(pOut,"# lookups %llu bytes %llu hits %llu%s\n",` |
|         - |  380 | `		(unsigned long long)sHCensus.nCall,(unsigned long long)sHCensus.nKeyByte,` |
|         - |  381 | `		(unsigned long long)sHCensus.nHit,sHCensus.bFull ? "  TRUNCATED" : "");` |
|         - |  382 | `	for( i = 0 ; i < PHL_HCENSUS_SLOTS ; ++i ){` |
|         - |  383 | `		phl_hcensus_rec *pRec = &sHCensus.aRec[i];` |
|         - |  384 | `		if( pRec->pSite == 0 ){` |
|         - |  385 | `			continue;` |
|         - |  386 | `		}` |
|         - |  387 | `		fprintf(pOut,"SITE 0x%lx %llu %llu %llu %llu\n",` |
|         - |  388 | `			(unsigned long)((char *)pRec->pSite - __executable_start),` |
|         - |  389 | `			(unsigned long long)pRec->nCall,(unsigned long long)pRec->nKeyByte,` |
|         - |  390 | `			(unsigned long long)pRec->nHit,(unsigned long long)pRec->nCI);` |
|         - |  391 | `	}` |
|         - |  392 | `	if( pOut != stderr ){` |
|         - |  393 | `		fclose(pOut);` |
|         - |  394 | `	}` |
|         - |  395 | `}` |
|         - |  396 | `static phl_hcensus_rec * HCensusSlot(void *pSite)` |
|         - |  397 | `{` |
|         - |  398 | `	/* Fibonacci scramble: the low bits of a code address are not a key. */` |
|         - |  399 | `	sxu64 x = (sxu64)(sxuptr)pSite;` |
|         - |  400 | `	sxu32 i,n;` |
|         - |  401 | `	x ^= x >> 33; x *= (sxu64)0xff51afd7ed558ccdULL; x ^= x >> 29;` |
|         - |  402 | `	i = (sxu32)x & (PHL_HCENSUS_SLOTS - 1);` |
|         - |  403 | `	for( n = 0 ; n < PHL_HCENSUS_SLOTS ; ++n ){` |
|         - |  404 | `		if( sHCensus.aRec[i].pSite == pSite ){` |
|         - |  405 | `			return &sHCensus.aRec[i];` |
|         - |  406 | `		}` |
|         - |  407 | `		if( sHCensus.aRec[i].pSite == 0 ){` |
|         - |  408 | `			sHCensus.aRec[i].pSite = pSite;` |
|         - |  409 | `			return &sHCensus.aRec[i];` |
|         - |  410 | `		}` |
|         - |  411 | `		i = (i + 1) & (PHL_HCENSUS_SLOTS - 1);` |
|         - |  412 | `	}` |
|         - |  413 | `	sHCensus.bFull = 1;   /* said out loud in the dump rather than counted wrong */` |
|         - |  414 | `	return 0;` |
|         - |  415 | `}` |
|         - |  416 | `static void HCensusNote(void *pSite,SyHash *pHash,sxu32 nKeyLen,int bHit)` |
|         - |  417 | `{` |
|         - |  418 | `	phl_hcensus_rec *pRec;` |
|         - |  419 | `	if( !sHCensus.bReady ){` |
|         - |  420 | `		sHCensus.bReady = 1;` |
|         - |  421 | `		atexit(HCensusDump);` |
|         - |  422 | `	}` |
|         - |  423 | `	pRec = HCensusSlot(pSite);` |
|         - |  424 | `	if( pRec == 0 ){` |
|         - |  425 | `		return;` |
|         - |  426 | `	}` |
|         - |  427 | `	pRec->nCall++;` |
|         - |  428 | `	pRec->nKeyByte += nKeyLen;` |
|         - |  429 | `	pRec->nHit += bHit ? 1 : 0;` |
|         - |  430 | `	pRec->nCI += (pHash->xHash == SyStrHash) ? 1 : 0;` |
|         - |  431 | `	sHCensus.nCall++;` |
|         - |  432 | `	sHCensus.nKeyByte += nKeyLen;` |
|         - |  433 | `	sHCensus.nHit += bHit ? 1 : 0;` |
|         - |  434 | `}` |
|         - |  435 | `#endif /* PHL_HASH_CENSUS */` |
| 157890374 |  436 | `static SyHashEntry_Pr * HashGetEntryHashed(SyHash *pHash,const void *pKey,sxu32 nKeyLen,sxu32 nHash)` |
|         5 |  437 | `{` |
|         - |  438 | `	SyHashEntry_Pr *pEntry;` |
|         - |  439 |  |
| 157890379 |  440 | `	pEntry = pHash->apBucket[nHash & (pHash->nBucketSize - 1)];` |
| 161204765 |  441 | `	for(;;){` |
| 324850891 |  442 | `		if( pEntry == 0 ){` |
|  68582506 |  443 | `			break;` |
|         - |  444 | `		}` |
| 300988326 |  445 | `		if( pEntry->nHash == nHash && pEntry->nKeyLen == nKeyLen &&` |
|  89336726 |  446 | `			pHash->xCmp(pEntry->pKey,pKey,nKeyLen) == 0 ){` |
|  89307878 |  447 | `				return pEntry;` |
|         - |  448 | `		}` |
| 166960517 |  449 | `		pEntry = pEntry->pNextCollide;` |
|         5 |  450 | `	}` |
|         - |  451 | `	/* Entry not found */` |
|  68582506 |  452 | `	return 0;` |
|  78662609 |  453 | `}` |
| 157767525 |  454 | `static SyHashEntry_Pr * HashGetEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  455 | `{` |
| 157767530 |  456 | `	return HashGetEntryHashed(&(*pHash),pKey,nKeyLen,pHash->xHash(pKey,nKeyLen));` |
|         5 |  457 | `}` |
| 158397541 |  458 | `PH7_PRIVATE SyHashEntry * SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  459 | `{` |
|         - |  460 | `	SyHashEntry_Pr *pEntry;` |
|         - |  461 | `#if defined(PHL_HASH_CENSUS)` |
|         - |  462 | `	void *pCensusSite = PHL_HCENSUS_SITE();` |
|         - |  463 | `#endif` |
|         - |  464 | `#if defined(UNTRUST)` |
|         - |  465 | `	if( INVALID_HASH(pHash) ){` |
|         - |  466 | `		return 0;` |
|         - |  467 | `	}` |
|         - |  468 | `#endif` |
| 158397546 |  469 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  470 | `		/* Don't bother hashing,return immediately */` |
|         - |  471 | `#if defined(PHL_HASH_CENSUS)` |
|         - |  472 | `		HCensusNote(pCensusSite,pHash,0,0);` |
|         - |  473 | `#endif` |
|   7526978 |  474 | `		return 0;` |
|         - |  475 | `	}` |
| 150870573 |  476 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|         - |  477 | `#if defined(PHL_HASH_CENSUS)` |
|         - |  478 | `	HCensusNote(pCensusSite,pHash,nKeyLen,pEntry != 0);` |
|         - |  479 | `#endif` |
| 150870573 |  480 | `	if( pEntry == 0 ){` |
|  68556883 |  481 | `		return 0;` |
|         - |  482 | `	}` |
|  82313695 |  483 | `	return (SyHashEntry *)pEntry;` |
|  78911681 |  484 | `}` |
|         - |  485 | `/*` |
|         - |  486 | ` * The KEY hashed once, for a caller that is about to ask several questions with it.` |
|         - |  487 | ` *` |
|         - |  488 | ` * A property access asks two -- does the executing scope declare a private of this` |
|         - |  489 | ` * name, and where is the slot on this object -- and used to hash the same bytes for` |
|         - |  490 | ` * each. Hashing is what a lookup spends, so the caller hashes once here` |
|         - |  491 | ` * and hands the answer to SyHashGetHashed below.` |
|         - |  492 | ` *` |
|         - |  493 | ` * The hash belongs to the TABLE, not to the key: two tables with different xHash` |
|         - |  494 | ` * answer differently for the same bytes. Only pass a hash taken from this function,` |
|         - |  495 | ` * and only to a table that shares this one's xHash.` |
|         - |  496 | ` */` |
|   1274079 |  497 | `PH7_PRIVATE sxu32 SyHashKey(SyHash *pHash,const void *pKey,sxu32 nKeyLen)` |
|         5 |  498 | `{` |
|         - |  499 | `	sxu32 nHash;` |
|         - |  500 | `#if defined(PHL_HASH_CENSUS)` |
|         - |  501 | `	void *pCensusSite = PHL_HCENSUS_SITE();` |
|         - |  502 | `#endif` |
|   1274084 |  503 | `	nHash = pHash->xHash(pKey,nKeyLen);` |
|         - |  504 | `#if defined(PHL_HASH_CENSUS)` |
|         - |  505 | `	/* Counted as a question asked at this site, because that is what it is: the` |
|         - |  506 | `	 * bytes are charged HERE, and the prehashed probes below then show none. A` |
|         - |  507 | `	 * census that only counted SyHashGet would make the work disappear. */` |
|         - |  508 | `	HCensusNote(pCensusSite,pHash,nKeyLen,1);` |
|         - |  509 | `#endif` |
|   1274084 |  510 | `	return nHash;` |
|         5 |  511 | `}` |
|         - |  512 | `/*` |
|         - |  513 | ` * SyHashGet with the key already hashed (SyHashKey). Hashes nothing.` |
|         - |  514 | ` */` |
|    123179 |  515 | `PH7_PRIVATE SyHashEntry * SyHashGetHashed(SyHash *pHash,const void *pKey,sxu32 nKeyLen,sxu32 nHash)` |
|         5 |  516 | `{` |
|         - |  517 | `	SyHashEntry_Pr *pEntry;` |
|         - |  518 | `#if defined(PHL_HASH_CENSUS)` |
|         - |  519 | `	void *pCensusSite = PHL_HCENSUS_SITE();` |
|         - |  520 | `#endif` |
|         - |  521 | `#if defined(UNTRUST)` |
|         - |  522 | `	if( INVALID_HASH(pHash) ){` |
|         - |  523 | `		return 0;` |
|         - |  524 | `	}` |
|         - |  525 | `	/* The one way to get this wrong is to hand over a hash taken for another` |
|         - |  526 | `	 * table. Answer NOT FOUND rather than whatever sits in the wrong bucket --` |
|         - |  527 | `	 * the same shape as the INVALID_HASH screen above, and like it this only` |
|         - |  528 | `	 * exists in an UNTRUST build. */` |
|         - |  529 | `	if( nKeyLen > 0 && nHash != pHash->xHash(pKey,nKeyLen) ){` |
|         - |  530 | `		return 0;` |
|         - |  531 | `	}` |
|         - |  532 | `#endif` |
|    123184 |  533 | `	if( pHash->nEntry < 1 \|\| nKeyLen < 1 ){` |
|         - |  534 | `#if defined(PHL_HASH_CENSUS)` |
|         - |  535 | `		HCensusNote(pCensusSite,pHash,0,0);` |
|         - |  536 | `#endif` |
|       335 |  537 | `		return 0;` |
|         - |  538 | `	}` |
|    122854 |  539 | `	pEntry = HashGetEntryHashed(&(*pHash),pKey,nKeyLen,nHash);` |
|         - |  540 | `#if defined(PHL_HASH_CENSUS)` |
|         - |  541 | `	HCensusNote(pCensusSite,pHash,0,pEntry != 0);` |
|         - |  542 | `#endif` |
|    122854 |  543 | `	if( pEntry == 0 ){` |
|      7554 |  544 | `		return 0;` |
|         - |  545 | `	}` |
|    115305 |  546 | `	return (SyHashEntry *)pEntry;` |
|     61595 |  547 | `}` |
|   6908036 |  548 | `static sxi32 HashDeleteEntry(SyHash *pHash,SyHashEntry_Pr *pEntry,void **ppUserData)` |
|         5 |  549 | `{` |
|         - |  550 | `	sxi32 rc;` |
|         - |  551 | `	/* Unlink from the collision chain. It is singly linked (see SyHashEntry_Pr), so` |
|         - |  552 | `	 * the predecessor is found by walking the bucket -- SXHASH_FILL_FACTOR entries` |
|         - |  553 | `	 * long on average. An entry that is not in its own bucket is a corrupted table` |
|         - |  554 | `	 * and the walk simply finds nothing rather than writing through a stale link. */` |
|         - |  555 | `	{` |
|   6908041 |  556 | `		SyHashEntry_Pr **ppSlot = &pHash->apBucket[pEntry->nHash & (pHash->nBucketSize - 1)];` |
|   6932332 |  557 | `		while( *ppSlot ){` |
|   6932332 |  558 | `			if( *ppSlot == pEntry ){` |
|   6908041 |  559 | `				*ppSlot = pEntry->pNextCollide;` |
|   6908041 |  560 | `				break;` |
|         - |  561 | `			}` |
|     24296 |  562 | `			ppSlot = &(*ppSlot)->pNextCollide;` |
|         5 |  563 | `		}` |
|         - |  564 | `	}` |
|         - |  565 | `	/* Keep the tail pointer valid when the last entry is the one removed. */` |
|   6908041 |  566 | `	if( pHash->pLast == pEntry ){` |
|     51431 |  567 | `		pHash->pLast = pEntry->pPrev;` |
|     25640 |  568 | `	}` |
|         - |  569 | `	/* ...and the embedded loop cursor, which the caller may be standing on: the` |
|         - |  570 | `	 * cursor is advanced BEFORE the body of a SyHashGetNextEntry() walk runs, so` |
|         - |  571 | `	 * a body that deletes the entry it is about to reach left pCurrent pointing` |
|         - |  572 | `	 * into the pool slot freed below. */` |
|   6908041 |  573 | `	if( pHash->pCurrent == pEntry ){` |
|     19347 |  574 | `		pHash->pCurrent = pEntry->pNext;` |
|      9646 |  575 | `	}` |
|   6908041 |  576 | `	MACRO_LD_REMOVE(pHash->pList,pEntry);` |
|   6908041 |  577 | `	pHash->nEntry--;` |
|   6908041 |  578 | `	if( ppUserData ){` |
|         - |  579 | `		/* Write a pointer to the user data */` |
|       156 |  580 | `		*ppUserData = pEntry->pUserData;` |
|        77 |  581 | `	}` |
|         - |  582 | `	/* Release the entry */` |
|   6908041 |  583 | `	rc = SyMemBackendPoolFree(pHash->pAllocator,pEntry);` |
|   6908041 |  584 | `	return rc;` |
|         5 |  585 | `}` |
|   6896957 |  586 | `PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData)` |
|         5 |  587 | `{` |
|         - |  588 | `	SyHashEntry_Pr *pEntry;` |
|         - |  589 | `	sxi32 rc;` |
|         - |  590 | `#if defined(UNTRUST)` |
|         - |  591 | `	if( INVALID_HASH(pHash) ){` |
|         - |  592 | `		return SXERR_CORRUPT;` |
|         - |  593 | `	}` |
|         - |  594 | `#endif` |
|   6896962 |  595 | `	pEntry = HashGetEntry(&(*pHash),pKey,nKeyLen);` |
|   6896962 |  596 | `	if( pEntry == 0 ){` |
|     18076 |  597 | `		return SXERR_NOTFOUND;` |
|         - |  598 | `	}` |
|   6878888 |  599 | `	rc = HashDeleteEntry(&(*pHash),pEntry,ppUserData);` |
|   6878888 |  600 | `	return rc;` |
|   3448321 |  601 | `}` |
|     29153 |  602 | `PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry)` |
|         5 |  603 | `{` |
|     29158 |  604 | `	SyHashEntry_Pr *pPtr = (SyHashEntry_Pr *)pEntry;` |
|         - |  605 | `	sxi32 rc;` |
|         - |  606 | `#if defined(UNTRUST)` |
|         - |  607 | `	if( pPtr == 0 \|\| INVALID_HASH(pPtr->pHash) ){` |
|         - |  608 | `		return SXERR_CORRUPT;` |
|         - |  609 | `	}` |
|         - |  610 | `#endif` |
|     29158 |  611 | `	rc = HashDeleteEntry(pPtr->pHash,pPtr,0);` |
|     29158 |  612 | `	return rc;` |
|         5 |  613 | `}` |
|  18474116 |  614 | `PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash)` |
|         5 |  615 | `{` |
|         - |  616 | `#if defined(UNTRUST)` |
|         - |  617 | `	if( INVALID_HASH(pHash)  ){` |
|         - |  618 | `		return SXERR_CORRUPT;` |
|         - |  619 | `	}` |
|         - |  620 | `#endif` |
|  18474121 |  621 | `	pHash->pCurrent = pHash->pList;` |
|  18474121 |  622 | `	return SXRET_OK;` |
|         5 |  623 | `}` |
| 150293087 |  624 | `PH7_PRIVATE SyHashEntry * SyHashGetNextEntry(SyHash *pHash)` |
|         5 |  625 | `{` |
|         - |  626 | `	SyHashEntry_Pr *pEntry;` |
|         - |  627 | `#if defined(UNTRUST)` |
|         - |  628 | `	if( INVALID_HASH(pHash) ){` |
|         - |  629 | `		return 0;` |
|         - |  630 | `	}` |
|         - |  631 | `#endif` |
| 150293092 |  632 | `	if( pHash->pCurrent == 0 \|\| pHash->nEntry <= 0 ){` |
|  18473971 |  633 | `		pHash->pCurrent = pHash->pList;` |
|  18473971 |  634 | `		return 0;` |
|         - |  635 | `	}` |
| 131819126 |  636 | `	pEntry = pHash->pCurrent;` |
|         - |  637 | `	/* Advance the cursor */` |
| 131819126 |  638 | `	pHash->pCurrent = pEntry->pNext;` |
|         - |  639 | `	/* Return the current entry */` |
| 131819126 |  640 | `	return (SyHashEntry *)pEntry;` |
|  75069689 |  641 | `}` |
|        82 |  642 | `PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         3 |  643 | `{` |
|         - |  644 | `	SyHashEntry_Pr *pEntry;` |
|         - |  645 | `	sxi32 rc;` |
|         - |  646 | `	sxu32 n;` |
|         - |  647 | `#if defined(UNTRUST)` |
|         - |  648 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  649 | `		return 0;` |
|         - |  650 | `	}` |
|         - |  651 | `#endif` |
|        85 |  652 | `	pEntry = pHash->pList;` |
|    137624 |  653 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  654 | `		/* Invoke the callback */` |
|    137542 |  655 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|    137542 |  656 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  657 | `			return rc;` |
|         - |  658 | `		}` |
|         - |  659 | `		/* Point to the next entry */` |
|    137542 |  660 | `		pEntry = pEntry->pNext;` |
|     66795 |  661 | `	}` |
|        85 |  662 | `	return SXRET_OK;` |
|        44 |  663 | `}` |
|         - |  664 | `/*` |
|         - |  665 | ` * Like SyHashForEach but walks the entries from the tail (pLast) back to the` |
|         - |  666 | ` * head via pPrev. The frame's local-variable table is built with SyHashInsert` |
|         - |  667 | ` * (head-push), so its forward pList order is reverse-insertion (LIFO); walking` |
|         - |  668 | ` * it backward yields DECLARATION order, which is what php's get_defined_vars()` |
|         - |  669 | ` * reports. Kept as its own primitive so the shared head-push insert path — and` |
|         - |  670 | ` * the SyHashLastEntry()==pList head contract every RefObj install relies on —` |
|         - |  671 | ` * stays untouched.` |
|         - |  672 | ` */` |
|       150 |  673 | `PH7_PRIVATE sxi32 SyHashForEachReverse(SyHash *pHash,sxi32 (*xStep)(SyHashEntry *,void *),void *pUserData)` |
|         5 |  674 | `{` |
|         - |  675 | `	SyHashEntry_Pr *pEntry;` |
|         - |  676 | `	sxi32 rc;` |
|         - |  677 | `	sxu32 n;` |
|         - |  678 | `#if defined(UNTRUST)` |
|         - |  679 | `	if( INVALID_HASH(pHash) \|\| xStep == 0){` |
|         - |  680 | `		return 0;` |
|         - |  681 | `	}` |
|         - |  682 | `#endif` |
|       155 |  683 | `	pEntry = pHash->pLast;` |
|    144691 |  684 | `	for( n = 0 ; n < pHash->nEntry ; n++ ){` |
|         - |  685 | `		/* Invoke the callback */` |
|    144541 |  686 | `		rc = xStep((SyHashEntry *)pEntry,pUserData);` |
|    144541 |  687 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  688 | `			return rc;` |
|         - |  689 | `		}` |
|         - |  690 | `		/* Point to the previous entry */` |
|    144541 |  691 | `		pEntry = pEntry->pPrev;` |
|     72240 |  692 | `	}` |
|       155 |  693 | `	return SXRET_OK;` |
|        80 |  694 | `}` |
|    286129 |  695 | `static sxi32 HashGrowTable(SyHash *pHash)` |
|         5 |  696 | `{` |
|    286134 |  697 | `	sxu32 nNewSize = pHash->nBucketSize * 2;` |
|         - |  698 | `	SyHashEntry_Pr *pEntry;` |
|         - |  699 | `	SyHashEntry_Pr **apNew;` |
|         - |  700 | `	sxu32 n,iBucket;` |
|         - |  701 |  |
|         - |  702 | `	/* Allocate a new larger table */` |
|    286134 |  703 | `	apNew = (SyHashEntry_Pr **)SyMemBackendAlloc(pHash->pAllocator,nNewSize * sizeof(SyHashEntry_Pr *));` |
|    286134 |  704 | `	if( apNew == 0 ){` |
|         - |  705 | `		/* Not so fatal,simply a performance hit */` |
|       ! 0 |  706 | `		return SXRET_OK;` |
|         - |  707 | `	}` |
|         - |  708 | `	/* Zero the new table */` |
|    286134 |  709 | `	SyZero((void *)apNew,nNewSize * sizeof(SyHashEntry_Pr *));` |
|         - |  710 | `	/* Rehash all entries */` |
|  55277334 |  711 | `	for( n = 0,pEntry = pHash->pList; n < pHash->nEntry ; n++  ){` |
|         - |  712 | `		/* Install in the new bucket */` |
|  54991205 |  713 | `		iBucket = pEntry->nHash & (nNewSize - 1);` |
|  54991205 |  714 | `		pEntry->pNextCollide = apNew[iBucket];` |
|  54991205 |  715 | `		apNew[iBucket] = pEntry;` |
|         - |  716 | `		/* Point to the next entry */` |
|  54991205 |  717 | `		pEntry = pEntry->pNext;` |
|  27458789 |  718 | `	}` |
|         - |  719 | `	/* Release the old table and reflect the change */` |
|    286134 |  720 | `	SyMemBackendFree(pHash->pAllocator,(void *)pHash->apBucket);` |
|    286134 |  721 | `	pHash->apBucket = apNew;` |
|    286134 |  722 | `	pHash->nBucketSize = nNewSize;` |
|    286134 |  723 | `	return SXRET_OK;` |
|    142883 |  724 | `}` |
|  95278825 |  725 | `static sxi32 HashInsert(SyHash *pHash,SyHashEntry_Pr *pEntry,int bTail)` |
|         5 |  726 | `{` |
|  95278830 |  727 | `	sxu32 iBucket = pEntry->nHash & (pHash->nBucketSize - 1);` |
|         - |  728 | `	/* Insert the entry in its corresponding bucket */` |
|  95278830 |  729 | `	pEntry->pNextCollide = pHash->apBucket[iBucket];` |
|  95278830 |  730 | `	pHash->apBucket[iBucket] = pEntry;` |
|         - |  731 | `	/* Link to the entry list. The default is head-insert (LIFO); bTail appends` |
|         - |  732 | `	 * to the tail (O(1) via pLast) so iteration follows insertion order — for` |
|         - |  733 | `	 * callers that need a FIFO traversal. */` |
|  95278830 |  734 | `	if( bTail && pHash->pLast != 0 ){` |
|  16075627 |  735 | `		pHash->pLast->pNext = pEntry;` |
|  16075627 |  736 | `		pEntry->pPrev = pHash->pLast;` |
|  16075627 |  737 | `		pHash->pLast = pEntry;` |
|   8032874 |  738 | `	}else{` |
|  79203208 |  739 | `		MACRO_LD_PUSH(pHash->pList,pEntry);` |
|         - |  740 | `	}` |
|  95278830 |  741 | `	if( pHash->nEntry == 0 ){` |
|         - |  742 | `		/* First entry: it is simultaneously the head, the tail and the cursor. */` |
|   6057920 |  743 | `		pHash->pCurrent = pHash->pList;` |
|   6057920 |  744 | `		pHash->pLast = pEntry;` |
|   3026603 |  745 | `	}` |
|  95278830 |  746 | `	pHash->nEntry++;` |
|  95278830 |  747 | `	return SXRET_OK;` |
|         5 |  748 | `}` |
|  95278825 |  749 | `static sxi32 SyHashInsertCore(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData,int bTail)` |
|         5 |  750 | `{` |
|         - |  751 | `	SyHashEntry_Pr *pEntry;` |
|         - |  752 | `	sxi32 rc;` |
|         - |  753 | `#if defined(UNTRUST)` |
|         - |  754 | `	if( INVALID_HASH(pHash) \|\| pKey == 0 ){` |
|         - |  755 | `		return SXERR_CORRUPT;` |
|         - |  756 | `	}` |
|         - |  757 | `#endif` |
|  95278830 |  758 | `	if( pHash->nEntry >= pHash->nBucketSize * SXHASH_FILL_FACTOR ){` |
|    286134 |  759 | `		rc = HashGrowTable(&(*pHash));` |
|    286134 |  760 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  761 | `			return rc;` |
|         - |  762 | `		}` |
|    142878 |  763 | `	}` |
|         - |  764 | `	/* Allocate a new hash entry */` |
|  95278830 |  765 | `	pEntry = (SyHashEntry_Pr *)SyMemBackendPoolAlloc(pHash->pAllocator,sizeof(SyHashEntry_Pr));` |
|  95278830 |  766 | `	if( pEntry == 0 ){` |
|       ! 0 |  767 | `		return SXERR_MEM;` |
|         - |  768 | `	}` |
|         - |  769 | `	/* Zero the entry */` |
|  95278830 |  770 | `	SyZero(pEntry,sizeof(SyHashEntry_Pr));` |
|  95278830 |  771 | `	pEntry->pHash = pHash;` |
|  95278830 |  772 | `	pEntry->pKey = pKey;` |
|  95278830 |  773 | `	pEntry->nKeyLen = nKeyLen;` |
|  95278830 |  774 | `	pEntry->pUserData = pUserData;` |
|  95278830 |  775 | `	pEntry->nHash = pHash->xHash(pEntry->pKey,pEntry->nKeyLen);` |
|         - |  776 | `	/* Finally insert the entry in its corresponding bucket */` |
|  95278830 |  777 | `	rc = HashInsert(&(*pHash),pEntry,bTail);` |
|  95278830 |  778 | `	return rc;` |
|  47390676 |  779 | `}` |
|  76287672 |  780 | `PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  781 | `{` |
|  76287677 |  782 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,0);` |
|         5 |  783 | `}` |
|         - |  784 | `/*` |
|         - |  785 | ` * Like SyHashInsert but appends the entry to the tail of the iteration list, so` |
|         - |  786 | ` * SyHashGetNextEntry() yields entries in insertion order (FIFO) rather than the` |
|         - |  787 | ` * default reverse-insertion (LIFO). Used for ordered collections such as dynamic` |
|         - |  788 | ` * object properties, where PHP preserves property-creation order.` |
|         - |  789 | ` */` |
|  18991153 |  790 | `PH7_PRIVATE sxi32 SyHashInsertTail(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData)` |
|         5 |  791 | `{` |
|  18991158 |  792 | `	return SyHashInsertCore(&(*pHash),pKey,nKeyLen,pUserData,1);` |
|         5 |  793 | `}` |
|         - |  794 | `/*` |
|         - |  795 | ` * The iteration list, walkable WITHOUT the hash's single embedded cursor: the` |
|         - |  796 | ` * head entry and, from any entry, its successor. A consumer that must survive` |
|         - |  797 | ` * re-entrancy (nested walks of one table) or a delete under its own feet keeps` |
|         - |  798 | ` * its own SyHashEntry* here instead of sharing pCurrent — see the instance` |
|         - |  799 | ` * attribute iterator in oo.c.` |
|         - |  800 | ` */` |
|    216938 |  801 | `PH7_PRIVATE SyHashEntry * SyHashFirstEntry(SyHash *pHash)` |
|         5 |  802 | `{` |
|         - |  803 | `#if defined(UNTRUST)` |
|         - |  804 | `	if( INVALID_HASH(pHash) ){` |
|         - |  805 | `		return 0;` |
|         - |  806 | `	}` |
|         - |  807 | `#endif` |
|    216943 |  808 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  809 | `}` |
|   1475458 |  810 | `PH7_PRIVATE SyHashEntry * SyHashEntryNext(SyHashEntry *pEntry)` |
|         5 |  811 | `{` |
|   1475463 |  812 | `	if( pEntry == 0 ){` |
|       ! 0 |  813 | `		return 0;` |
|         - |  814 | `	}` |
|   1475463 |  815 | `	return (SyHashEntry *)((SyHashEntry_Pr *)pEntry)->pNext;` |
|    737734 |  816 | `}` |
|       160 |  817 | `PH7_PRIVATE SyHashEntry * SyHashTailEntry(SyHash *pHash)` |
|         4 |  818 | `{` |
|         - |  819 | `#if defined(UNTRUST)` |
|         - |  820 | `	if( INVALID_HASH(pHash) ){` |
|         - |  821 | `		return 0;` |
|         - |  822 | `	}` |
|         - |  823 | `#endif` |
|         - |  824 | `	/* The tail of the head-pushed list, i.e. the FIRST entry inserted. Walk from` |
|         - |  825 | `	 * here with SyHashEntryPrev for declaration order (see SyHashForEachReverse,` |
|         - |  826 | `	 * which does the same thing with a callback). */` |
|       164 |  827 | `	return (SyHashEntry *)pHash->pLast;` |
|         4 |  828 | `}` |
|       252 |  829 | `PH7_PRIVATE SyHashEntry * SyHashEntryPrev(SyHashEntry *pEntry)` |
|         4 |  830 | `{` |
|       256 |  831 | `	if( pEntry == 0 ){` |
|       ! 0 |  832 | `		return 0;` |
|         - |  833 | `	}` |
|       256 |  834 | `	return (SyHashEntry *)((SyHashEntry_Pr *)pEntry)->pPrev;` |
|       130 |  835 | `}` |
|   1457778 |  836 | `PH7_PRIVATE SyHashEntry * SyHashLastEntry(SyHash *pHash)` |
|         5 |  837 | `{` |
|         - |  838 | `#if defined(UNTRUST)` |
|         - |  839 | `	if( INVALID_HASH(pHash) ){` |
|         - |  840 | `		return 0;` |
|         - |  841 | `	}` |
|         - |  842 | `#endif` |
|         - |  843 | `	/* Last inserted entry */` |
|   1457783 |  844 | `	return (SyHashEntry *)pHash->pList;` |
|         5 |  845 | `}` |
|         - |  846 |  |
